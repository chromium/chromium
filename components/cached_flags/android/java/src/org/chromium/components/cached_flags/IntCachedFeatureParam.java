// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.AnyThread;

import org.chromium.base.FeatureMap;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.cached_flags.ValuesReturned;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Supplier;

/** An int-type {@link CachedFeatureParam}. */
@NullMarked
public class IntCachedFeatureParam extends CachedFeatureParam<Integer> {
    private @Nullable Supplier<Integer> mValueSupplier;

    public IntCachedFeatureParam(
            FeatureMap featureMap, String featureName, String variationName, int defaultValue) {
        super(featureMap, featureName, variationName, FeatureParamType.INT, defaultValue);
    }

    /**
     * @return the value of the feature parameter that should be used in this run.
     */
    @AnyThread
    public int getValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String testValue =
                FeatureOverrides.getTestValueForFieldTrialParam(mFeatureName, mParamName);
        if (testValue != null) {
            return Integer.parseInt(testValue);
        }

        return ValuesReturned.getReturnedOrNewIntValue(
                getSharedPreferenceKey(), getValueSupplier());
    }

    private Supplier<Integer> getValueSupplier() {
        if (mValueSupplier == null) {
            mValueSupplier =
                    () -> {
                        String preferenceName = getSharedPreferenceKey();
                        Integer value =
                                CachedFlagsSafeMode.getInstance()
                                        .getIntFeatureParam(preferenceName, mDefaultValue);
                        if (value == null) {
                            value =
                                    CachedFlagsSharedPreferences.getInstance()
                                            .readInt(preferenceName, mDefaultValue);
                        }
                        return value;
                    };
        }
        return mValueSupplier;
    }

    public int getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void writeCacheValueToEditor(final SharedPreferences.Editor editor) {
        final int value =
                mFeatureMap.getFieldTrialParamByFeatureAsInt(
                        getFeatureName(), getName(), getDefaultValue());
        editor.putInt(getSharedPreferenceKey(), value);
    }

    @Override
    void writeCacheValueToEditor(final SharedPreferences.Editor editor, String value) {
        final int intValue = Integer.valueOf(value);
        editor.putInt(getSharedPreferenceKey(), intValue);
    }

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * @param overrideValue the value to be returned
     */
    public void setForTesting(int overrideValue) {
        FeatureOverrides.overrideParam(getFeatureName(), getName(), overrideValue);
    }
}
