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

/** A double-type {@link CachedFeatureParam}. */
@NullMarked
public class DoubleCachedFeatureParam extends CachedFeatureParam<Double> {
    private @Nullable Supplier<Double> mValueSupplier;

    public DoubleCachedFeatureParam(
            FeatureMap featureMap, String featureName, String variationName, double defaultValue) {
        super(featureMap, featureName, variationName, FeatureParamType.DOUBLE, defaultValue);
    }

    /**
     * @return the value of the feature parameter that should be used in this run.
     */
    @AnyThread
    public double getValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String testValue =
                FeatureOverrides.getTestValueForFieldTrialParam(mFeatureName, mParamName);
        if (testValue != null) {
            return Double.parseDouble(testValue);
        }

        return ValuesReturned.getReturnedOrNewDoubleValue(
                getSharedPreferenceKey(), getValueSupplier());
    }

    private Supplier<Double> getValueSupplier() {
        if (mValueSupplier == null) {
            mValueSupplier =
                    () -> {
                        String preferenceName = getSharedPreferenceKey();
                        Double value =
                                CachedFlagsSafeMode.getInstance()
                                        .getDoubleFeatureParam(preferenceName, mDefaultValue);
                        if (value == null) {
                            value =
                                    CachedFlagsSharedPreferences.getInstance()
                                            .readDouble(preferenceName, mDefaultValue);
                        }
                        return value;
                    };
        }
        return mValueSupplier;
    }

    public double getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void writeCacheValueToEditor(final SharedPreferences.Editor editor) {
        // Matches the conversion used in SharedPreferencesManager#writeDouble().
        final long value =
                Double.doubleToRawLongBits(
                        mFeatureMap.getFieldTrialParamByFeatureAsDouble(
                                getFeatureName(), getName(), getDefaultValue()));
        editor.putLong(getSharedPreferenceKey(), value);
    }

    @Override
    void writeCacheValueToEditor(final SharedPreferences.Editor editor, String value) {
        final long doubleValue = Double.doubleToRawLongBits(Double.valueOf(value));
        editor.putLong(getSharedPreferenceKey(), doubleValue);
    }

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * @param overrideValue the value to be returned
     */
    public void setForTesting(double overrideValue) {
        FeatureOverrides.overrideParam(getFeatureName(), getName(), overrideValue);
    }
}
