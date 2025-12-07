// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import android.content.SharedPreferences;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Utility methods for {@link CachedFlag}s. */
@NullMarked
@JNINamespace("cached_flags")
public class CachedFlagUtils {
    /** Caches flags that must take effect on startup but are set via native code. */
    public static void cacheNativeFlags(List<List<CachedFlag>> listsOfFeaturesToCache) {
        if (listsOfFeaturesToCache.isEmpty()) return;

        // Batch the updates into a single apply() call to avoid calling the expensive
        // SharedPreferencesImpl$EditorImpl.commitToMemory() method many times unnecessarily.
        final SharedPreferences.Editor editor =
                CachedFlagsSharedPreferences.getInstance().getEditor();
        for (final List<CachedFlag> featuresToCache : listsOfFeaturesToCache) {
            for (CachedFlag feature : featuresToCache) {
                feature.writeCacheValueToEditor(editor);
            }
        }
        editor.apply();
    }

    /** Caches feature params that must take effect on startup but are set via native code. */
    public static void cacheFeatureParams(List<List<CachedFeatureParam<?>>> listsOfParameters) {
        if (listsOfParameters.isEmpty()) return;

        // Batch the updates into a single apply() call to avoid calling the expensive
        // SharedPreferencesImpl$EditorImpl.commitToMemory() method many times unnecessarily.
        final SharedPreferences.Editor editor =
                CachedFlagsSharedPreferences.getInstance().getEditor();
        for (final List<CachedFeatureParam<?>> parameters : listsOfParameters) {
            for (final CachedFeatureParam<?> parameter : parameters) {
                parameter.writeCacheValueToEditor(editor);
            }
        }
        editor.apply();
    }

    /** The full list of all instances of CachedFlag. */
    private static @Nullable List<List<CachedFlag>> sListsOfCachedFlags;

    /** The full list of all instances of CachedFeatureParam. */
    private static @Nullable List<List<CachedFeatureParam<?>>> sListsOfFeatureParams;

    /** Store a reference to the full list of CachedFlags for future use. */
    public static void setFullListOfFlags(List<List<CachedFlag>> listsOfFlags) {
        sListsOfCachedFlags = listsOfFlags;
    }

    /** Store a reference to the full list of CachedFeatureParam for future use. */
    public static void setFullListOfFeatureParams(
            List<List<CachedFeatureParam<?>>> listsOfParameters) {
        sListsOfFeatureParams = listsOfParameters;
    }

    /**
     * Find a specific CachedFlag from the full list of CachedFlags by its feature name. This only
     * works after setFullListOfFlags() is called.
     */
    static @Nullable CachedFlag getSpecificCachedFlag(String featureName) {
        if (sListsOfCachedFlags == null) {
            throw new IllegalStateException(
                    "getSpecificCachedFlag() is called before setFullListOfFlags()");
        }

        for (List<CachedFlag> listOfCachedFlags : sListsOfCachedFlags) {
            for (CachedFlag cachedFlag : listOfCachedFlags) {
                if (featureName.equals(cachedFlag.getFeatureName())) {
                    return cachedFlag;
                }
            }
        }
        return null;
    }

    /**
     * Find a specific CachedFeatureParam from the full list of CachedFeatureParams by its feature
     * name and parameter name. This only works after setFullListOfFeatureParams() is called.
     */
    static @Nullable CachedFeatureParam<?> getSpecificCachedFeatureParam(
            String featureName, String paramName) {
        if (sListsOfFeatureParams == null) {
            throw new IllegalStateException(
                    "getSpecificCachedFeatureParam() is called before"
                            + " setFullListOfFeatureParams()");
        }

        for (List<CachedFeatureParam<?>> listOfFeatureParams : sListsOfFeatureParams) {
            for (CachedFeatureParam<?> featureParam : listOfFeatureParams) {
                if (featureName.equals(featureParam.getFeatureName())
                        && paramName.equals(featureParam.getName())) {
                    return featureParam;
                }
            }
        }
        return null;
    }

    /**
     * Immediately caches flags that must take effect on startup but are set via native code.
     * Compared to cacheNativeFlags() which is called only when the native library is loaded, this
     * function is called whenever a feature flag is set by the user, which ensures that the cached
     * values are available in the next run.
     *
     * @param features A map from feature name to feature value that needs to be cached immediately.
     *     The feature value should be either "true" or "false".
     */
    @CalledByNative
    public static void cacheNativeFlagsImmediately(
            @JniType("std::map<std::string, std::string>") Map<String, String> features) {
        if (features.isEmpty()) return;

        final SharedPreferences.Editor editor =
                CachedFlagsSharedPreferences.getInstance().getEditor();
        for (var entry : features.entrySet()) {
            String featureName = entry.getKey();
            String featureValueString = entry.getValue();
            assert featureValueString.equals("true") || featureValueString.equals("false");
            boolean featureValue = Boolean.valueOf(featureValueString);
            CachedFlag feature = getSpecificCachedFlag(featureName);
            // If a flag is not cached and thus cannot be found
            // in the list of CachedFlags, it will be skipped
            if (feature == null) continue;
            feature.writeCacheValueToEditor(editor, featureValue);
        }
        editor.apply();
    }

    /**
     * Immediately caches params that must take effect on startup but are set via native code.
     * Compared to cacheFeatureParams() which is called only when the native library is loaded, this
     * function is called whenever a feature flag is set by the user, which ensures that the cached
     * values are available in the next run.
     *
     * @param featureParams A map from feature name to param name to param value, containing the
     *     feature params that needs to be cached. For each feature name, we first clear all the
     *     cached feature params related to this feature name, then cache each param that exist in
     *     the inner map.
     */
    @CalledByNative
    public static void cacheFeatureParamsImmediately(
            @JniType("std::map<std::string, std::map<std::string, std::string>>")
                    Map<String, Map<String, String>> featureParams) {
        if (featureParams.isEmpty()) return;

        eraseFeatureParamCachedValues(new ArrayList<>(featureParams.keySet()));

        final SharedPreferences.Editor editor =
                CachedFlagsSharedPreferences.getInstance().getEditor();
        for (var entry : featureParams.entrySet()) {
            String featureName = entry.getKey();
            for (var innerEntry : entry.getValue().entrySet()) {
                String paramName = innerEntry.getKey();
                String paramValue = innerEntry.getValue();
                CachedFeatureParam<?> featureParam =
                        getSpecificCachedFeatureParam(featureName, paramName);
                if (featureParam == null) continue;
                featureParam.writeCacheValueToEditor(editor, paramValue);
            }
        }
        editor.apply();
    }

    /**
     * Given a list of feature names, find the CachedFlag instances corresponding to these feature
     * names, and erase all the values in SharedPrefs cached by these CachedFlags.
     *
     * @param featuresToErase A list of feature names which we need to remove the cached values.
     */
    @CalledByNative
    public static void eraseNativeFlagCachedValues(
            @JniType("std::vector<std::string>") List<String> featuresToErase) {
        if (featuresToErase.isEmpty()) return;

        final SharedPreferences.Editor editor =
                CachedFlagsSharedPreferences.getInstance().getEditor();
        for (String featureName : featuresToErase) {
            CachedFlag feature = getSpecificCachedFlag(featureName);
            if (feature == null) continue;
            editor.remove(feature.getSharedPreferenceKey());
        }
        editor.apply();
    }

    /**
     * Given a list of feature names, erase all the values in SharedPrefs cached by all the
     * CachedFeatureParams related to these feature names.
     *
     * @param featuresWithParamsToErase A list of feature names which we need to remove the cached
     *     values.
     */
    @CalledByNative
    public static void eraseFeatureParamCachedValues(
            @JniType("std::vector<std::string>") List<String> featuresWithParamsToErase) {
        if (featuresWithParamsToErase.isEmpty()) return;

        final SharedPreferencesManager manager = CachedFlagsSharedPreferences.getInstance();
        for (String featureName : featuresWithParamsToErase) {
            // All cached feature params related to this feature name should have a prefix of
            // CachedFlagsSharedPreferences.FLAGS_FEATURE_PARAM_CACHED.createKey(featureName + "*")
            // in the SharedPrefs, so we clear all keys with this prefix in the SharedPrefs
            manager.removeKeysWithPrefix(
                    CachedFlagsSharedPreferences.FLAGS_FEATURE_PARAM_CACHED, featureName);
        }
    }
}
