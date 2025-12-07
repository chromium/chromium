// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.IntDef;

import org.chromium.base.FeatureMap;
import org.chromium.base.FeatureParam;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.CheckDiscard;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
import java.util.Set;

/**
 * A feature parameter in the variations framework that is cached to disk be used before native.
 *
 * @param <T> the type of the parameter
 */
@NullMarked
public abstract class CachedFeatureParam<T> extends FeatureParam<T> {
    /** Data types of feature parameters. */
    @IntDef({
        FeatureParamType.STRING,
        FeatureParamType.BOOLEAN,
        FeatureParamType.INT,
        FeatureParamType.DOUBLE,
        FeatureParamType.ALL
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FeatureParamType {
        int STRING = 0;
        int BOOLEAN = 1;
        int INT = 2;
        int DOUBLE = 3;
        int ALL = 4;
    }

    @CheckDiscard("crbug.com/1067145")
    private static @Nullable Set<CachedFeatureParam<?>> sAllInstances;

    private final @FeatureParamType int mType;

    CachedFeatureParam(
            FeatureMap featureMap,
            String featureName,
            String parameterName,
            @FeatureParamType int type,
            T defaultValue) {
        super(featureMap, featureName, parameterName, defaultValue);

        // parameterName does not apply to ALL (because it includes all parameters).
        assert type != FeatureParamType.ALL || parameterName.isEmpty();
        mType = type;

        registerInstance();
    }

    private void registerInstance() {
        if (!BuildConfig.ENABLE_ASSERTS) return;

        if (sAllInstances == null) {
            sAllInstances = new HashSet<>();
        }
        sAllInstances.add(this);
    }

    @CheckDiscard("crbug.com/1067145")
    public static @Nullable Set<CachedFeatureParam<?>> getAllInstances() {
        return sAllInstances;
    }

    /**
     * @return The data type of the feature parameter.
     */
    public @FeatureParamType int getType() {
        return mType;
    }

    /**
     * @return The SharedPreferences key to cache the feature parameter.
     */
    String getSharedPreferenceKey() {
        return CachedFlagsSharedPreferences.FLAGS_FEATURE_PARAM_CACHED.createKey(
                getFeatureName() + ":" + getName());
    }

    /**
     * Gets the current value of the parameter and writes it to the provided SharedPreferences
     * editor. Does not apply or commit the change, that is left up to the caller to perform. Calls
     * to getValue() in a future run will return the value cached in this method, if native is not
     * loaded yet.
     */
    abstract void writeCacheValueToEditor(SharedPreferences.Editor editor);

    /**
     * Assumes the parameter value is the current value of the parameter and writes it to the
     * provided SharedPreferences editor. Does not apply or commit the change, that is left up to
     * the caller to perform. Calls to getValue() in a future run will return the value cached in
     * this method, if native is not loaded yet.
     */
    abstract void writeCacheValueToEditor(SharedPreferences.Editor editor, String value);
}
