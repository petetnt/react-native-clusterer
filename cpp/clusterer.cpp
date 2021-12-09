#include <map>
#include <jsi/jsi.h>

#include "clusterer.h"
#include "supercluster.hpp"

#include <android/log.h>
#define LOG_TAG "RNClustererJNI"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGSIMPLE(...)

using namespace std;
using namespace facebook;

map<string, mapbox::supercluster::Supercluster *> clusterMap = map<string, mapbox::supercluster::Supercluster *>();

void cluster_init(string name, jsi::Runtime &rt, jsi::Value const &v1, jsi::Value const &v2)
{
    auto options = cluster_parseJSIOptions(rt, v1);
    auto features = cluster_parseJSIFeatures(rt, v2);
    auto *cluster = new mapbox::supercluster::Supercluster(features, options);
    clusterMap[name] = cluster;
}

jsi::Array cluster_getTile(string name, jsi::Runtime &rt, int zoom, int x, int y)
{
    mapbox::supercluster::Supercluster *cluster = clusterMap[name];
    auto tile = cluster->getTile(zoom, x, y);

    jsi::Array result = jsi::Array(rt, tile.size());
    int i = 0;
    for (auto &f : tile)
    {
        result.setValueAtIndex(rt, i, tileToJSIObject(rt, f, true));
        i++;
    }
    return result;
}

jsi::Array cluster_getChildren(string name, jsi::Runtime &rt, int cluster_id)
{
    mapbox::supercluster::Supercluster *cluster = clusterMap[name];
    auto children = cluster->getChildren(cluster_id);
    jsi::Array result = jsi::Array(rt, children.size());
    int i = 0;
    for (auto &f : children)
    {
        result.setValueAtIndex(rt, i, tileToJSIObject(rt, f, false));
        i++;
    }

    return result;
}

jsi::Array cluster_getLeaves(string name, jsi::Runtime &rt, int cluster_id, int limit, int offset)
{
    mapbox::supercluster::Supercluster *cluster = clusterMap[name];
    auto leaves = cluster->getLeaves(cluster_id, limit, offset);

    jsi::Array result = jsi::Array(rt, leaves.size());
    int i = 0;
    for (auto &f : leaves)
    {
        result.setValueAtIndex(rt, i, tileToJSIObject(rt, f, false));
        i++;
    }

    return result;
}

int cluster_getClusterExpansionZoom(string name, int cluster_id)
{
    mapbox::supercluster::Supercluster *cluster = clusterMap[name];
    return (int)cluster->getClusterExpansionZoom(cluster_id);
}

mapbox::feature::feature_collection<double> cluster_parseJSIFeatures(jsi::Runtime &rt, jsi::Value const &value)
{
    mapbox::feature::feature_collection<double> features;
    if (value.asObject(rt).isArray(rt))
    {
        jsi::Array arr = value.asObject(rt).asArray(rt);
        for (int i = 0; i < arr.size(rt); i++)
        {
            mapbox::feature::feature<double> feature = parseJSIFeature(rt, arr.getValueAtIndex(rt, i));
            features.push_back(feature);
        }
    }
    else
    {
        jsi::detail::throwJSError(rt, "Expected array of GeoJSON Feature objects");
    }
    return features;
};

mapbox::supercluster::Options cluster_parseJSIOptions(jsi::Runtime &rt, jsi::Value const &value)
{
    mapbox::supercluster::Options options;
    if (value.isObject())
    {
        jsi::Object obj = value.asObject(rt);

        if (obj.hasProperty(rt, "radius"))
        {
            jsi::Value radius = obj.getProperty(rt, "radius");
            if (radius.isNumber())
            {
                options.radius = (int)radius.asNumber();
            }
            else
                jsi::detail::throwJSError(rt, "Expected number for radius");
        }

        if (obj.hasProperty(rt, "minZoom"))
        {
            jsi::Value minZoom = obj.getProperty(rt, "minZoom");
            if (minZoom.isNumber())
            {
                options.minZoom = (int)minZoom.asNumber();
            }
            else
                jsi::detail::throwJSError(rt, "Expected number for minZoom");
        }

        if (obj.hasProperty(rt, "maxZoom"))
        {
            jsi::Value maxZoom = obj.getProperty(rt, "maxZoom");
            if (maxZoom.isNumber())
            {
                options.maxZoom = (int)maxZoom.asNumber();
            }
            else
                jsi::detail::throwJSError(rt, "Expected number for maxZoom");
        }

        if (obj.hasProperty(rt, "extent"))
        {
            jsi::Value extent = obj.getProperty(rt, "extent");
            if (extent.isNumber())
            {
                options.extent = (int)extent.asNumber();
            }
            else
                jsi::detail::throwJSError(rt, "Expected number for extent");
        }

        if (obj.hasProperty(rt, "minPoints"))
        {
            jsi::Value minPoints = obj.getProperty(rt, "minPoints");
            if (minPoints.isNumber())
            {
                options.minPoints = (int)minPoints.asNumber();
            }
            else
                jsi::detail::throwJSError(rt, "Expected number for minPoints");
        }
    }
    else
        jsi::detail::throwJSError(rt, "Expected object for options");
    return options;
};

mapbox::feature::feature<double> parseJSIFeature(jsi::Runtime &rt, jsi::Value const &value)
{
    mapbox::feature::feature<double> feature;
    if (value.isObject())
    {
        jsi::Object obj = value.asObject(rt);

        if (!obj.hasProperty(rt, "type") || !strcmp(obj.getProperty(rt, "type").asString(rt).utf8(rt).c_str(), "Point"))
            jsi::detail::throwJSError(rt, "Expected GeoJSON Feature object with type 'Point'");

        if (obj.hasProperty(rt, "geometry"))
        {
            jsi::Value geometry = obj.getProperty(rt, "geometry");
            if (geometry.isObject())
            {
                jsi::Object geoObj = geometry.asObject(rt);
                if (geoObj.hasProperty(rt, "coordinates"))
                {
                    jsi::Value coordinates = geoObj.getProperty(rt, "coordinates");
                    if (coordinates.asObject(rt).isArray(rt))
                    {
                        jsi::Array arr = coordinates.asObject(rt).asArray(rt);
                        if (arr.size(rt) == 2)
                        {
                            jsi::Value x = arr.getValueAtIndex(rt, 0);
                            jsi::Value y = arr.getValueAtIndex(rt, 1);
                            if (x.isNumber() && y.isNumber())
                            {
                                double lng = x.asNumber();
                                double lat = y.asNumber();
                                mapbox::geometry::point<double> point(lng, lat);
                                feature.geometry = point;
                            }
                            else
                                jsi::detail::throwJSError(rt, "Expected number for coordinates");
                        }
                        else
                            jsi::detail::throwJSError(rt, "Expected array of size 2 for coordinates");
                    }
                    else
                        jsi::detail::throwJSError(rt, "Expected array for coordinates");
                }
                else
                    jsi::detail::throwJSError(rt, "Expected coordinates property");
            }
            else
                jsi::detail::throwJSError(rt, "Expected geometry object");
        }
        else
            jsi::detail::throwJSError(rt, "Expected geometry property");
    }
    else
        jsi::detail::throwJSError(rt, "Expected GeoJSON Feature object");
    return feature;
};

jsi::Object tileToJSIObject(jsi::Runtime &rt, mapbox::feature::feature<double> &f, bool geometryAsInt)
{
    jsi::Object result = jsi::Object(rt);
    result.setProperty(rt, "type", 1);

    jsi::Array geometryContainer = jsi::Array(rt, 1);
    jsi::Array geometry = jsi::Array(rt, 2);
    auto gem = f.geometry.get<mapbox::geometry::point<double>>();
    geometry.setValueAtIndex(rt, 0, geometryAsInt ? jsi::Value((int)gem.x) : jsi::Value(gem.x));
    geometry.setValueAtIndex(rt, 1, geometryAsInt ? jsi::Value((int)gem.y) : jsi::Value(gem.y));
    geometryContainer.setValueAtIndex(rt, 0, geometry);

    jsi::Object tags = jsi::Object(rt);

    const auto itr = f.properties.find("cluster");
    if (itr != f.properties.end() && itr->second.get<bool>())
    {
        LOGW("Inside cluster");
        double cluster_id = f.properties["cluster_id"].get<uint64_t>();
        double point_count = f.properties["point_count"].get<uint64_t>();
        auto pc_abbreviated = f.properties["point_count_abbreviated"].get<string>();

        tags.setProperty(rt, "cluster", true);
        tags.setProperty(rt, "cluster_id", jsi::Value(cluster_id));
        tags.setProperty(rt, "point_count", jsi::Value(point_count));
        tags.setProperty(rt, "point_count_abbreviated", jsi::String::createFromUtf8(rt, pc_abbreviated));
    }
    else
    {
        LOGW("Inside point");
        // TODO: pass tags from points
    }
    result.setProperty(rt, "tags", tags);
    result.setProperty(rt, "geometry", geometry);

    return result;
}