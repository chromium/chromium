// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.AnyThread;

import org.chromium.base.FeatureMap;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.cached_flags.ValuesReturned;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A boolean-type {@link CachedFeatureParam}. */
@NullMarked
public class BooleanCachedFeatureParam extends CachedFeatureParam<Boolean> {
    private @Nullable Supplier<Boolean> mValueSupplier;

    public BooleanCachedFeatureParam(
            FeatureMap featureMap, String featureName, String variationName, boolean defaultValue) {
        super(featureMap, featureName, variationName, FeatureParamType.BOOLEAN, defaultValue);
    }

    /**
     * @return the value of the feature parameter that should be used in this run.
     */
    @AnyThread
    public boolean getValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String testValue =
                FeatureOverrides.getTestValueForFieldTrialParam(mFeatureName, mParamName);
        if (testValue != null) {
            return Boolean.parseBoolean(testValue);
        }

        return ValuesReturned.getReturnedOrNewBoolValue(
                getSharedPreferenceKey(), getValueSupplier());
    }

    private Supplier<Boolean> getValueSupplier() {
        if (mValueSupplier == null) {
            mValueSupplier =
                    () -> {
                        String preferenceName = getSharedPreferenceKey();
                        Boolean value =
                                CachedFlagsSafeMode.getInstance()
                                        .getBooleanFeatureParam(preferenceName, mDefaultValue);
                        if (value == null) {
                            value =
                                    CachedFlagsSharedPreferences.getInstance()
                                            .readBoolean(preferenceName, mDefaultValue);
                        }
                        return value;
                    };
        }
        return mValueSupplier;
    }

    public boolean getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void writeCacheValueToEditor(final SharedPreferences.Editor editor) {
        final boolean value =
                mFeatureMap.getFieldTrialParamByFeatureAsBoolean(
                        getFeatureName(), getName(), getDefaultValue());
        editor.putBoolean(getSharedPreferenceKey(), value);
    }

    @Override
    void writeCacheValueToEditor(final SharedPreferences.Editor editor, String value) {
        final boolean booleanValue = Boolean.valueOf(value);
        editor.putBoolean(getSharedPreferenceKey(), booleanValue);
    }

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * @param overrideValue the value to be returned
     */
    public void setForTesting(boolean overrideValue) {
        FeatureOverrides.overrideParam(getFeatureName(), getName(), overrideValue);
    }
}
