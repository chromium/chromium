// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.IntDef;

import org.chromium.base.FeatureMap;
import org.chromium.base.cached_flags.CachedFlagsSharedPreferences;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.CheckDiscard;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/**
 * A field trial parameter in the variations framework that is cached to disk be used before native.
 */
public abstract class CachedFieldTrialParameter {

    /** Data types of field trial parameters. */
    @IntDef({
        FieldTrialParameterType.STRING,
        FieldTrialParameterType.BOOLEAN,
        FieldTrialParameterType.INT,
        FieldTrialParameterType.DOUBLE,
        FieldTrialParameterType.ALL
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FieldTrialParameterType {
        int STRING = 0;
        int BOOLEAN = 1;
        int INT = 2;
        int DOUBLE = 3;
        int ALL = 4;
    }

    @CheckDiscard("crbug.com/1067145")
    private static Set<CachedFieldTrialParameter> sAllInstances;

    protected final FeatureMap mFeatureMap;

    private final String mFeatureName;
    private final String mParameterName;
    private final @FieldTrialParameterType int mType;
    private static HashMap<String, CachedFieldTrialParameter> sParamsCreatedForTesting =
            new HashMap<>();

    CachedFieldTrialParameter(
            FeatureMap featureMap,
            String featureName,
            String parameterName,
            @FieldTrialParameterType int type) {
        if (BuildConfig.IS_FOR_TEST) {
            String combinedName = featureName + ":" + parameterName;
            CachedFieldTrialParameter previous = sParamsCreatedForTesting.put(combinedName, this);
            assert previous == null
                    : String.format(
                            "Feature '%s' has a duplicate parameter: '%s'",
                            featureName, parameterName);
        }

        mFeatureMap = featureMap;
        mFeatureName = featureName;
        // parameterName does not apply to ALL (because it includes all parameters).
        assert type != FieldTrialParameterType.ALL || parameterName.isEmpty();
        mParameterName = parameterName;
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
    public static Set<CachedFieldTrialParameter> getAllInstances() {
        return sAllInstances;
    }

    /**
     * @return The name of the related field trial.
     */
    public String getFeatureName() {
        return mFeatureName;
    }

    /**
     * @return The name of the field trial parameter.
     */
    public String getParameterName() {
        return mParameterName;
    }

    /**
     * @return The data type of the field trial parameter.
     */
    public @FieldTrialParameterType int getType() {
        return mType;
    }

    /**
     * @return The SharedPreferences key to cache the field trial parameter.
     */
    String getSharedPreferenceKey() {
        return CachedFlagsSharedPreferences.generateParamSharedPreferenceKey(
                getFeatureName(), getParameterName());
    }

    /**
     * Gets the current value of the parameter and writes it to the provided SharedPreferences
     * editor. Does not apply or commit the change, that is left up to the caller to perform. Calls
     * to getValue() in a future run will return the value cached in this method, if native is not
     * loaded yet.
     */
    abstract void writeCacheValueToEditor(SharedPreferences.Editor editor);
}
