// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.AnyThread;

import org.chromium.base.FeatureMap;
import org.chromium.base.cached_flags.CachedFlagsSharedPreferences;
import org.chromium.base.cached_flags.ValuesOverridden;
import org.chromium.base.cached_flags.ValuesReturned;
import org.chromium.base.supplier.Supplier;

/** A double-type {@link CachedFieldTrialParameter}. */
public class DoubleCachedFieldTrialParameter extends CachedFieldTrialParameter<Double> {
    private Supplier<Double> mValueSupplier;

    public DoubleCachedFieldTrialParameter(
            FeatureMap featureMap, String featureName, String variationName, double defaultValue) {
        super(featureMap, featureName, variationName, FieldTrialParameterType.DOUBLE, defaultValue);
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    @AnyThread
    public double getValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String preferenceName = getSharedPreferenceKey();

        Double value = ValuesOverridden.getDouble(preferenceName);
        if (value != null) {
            return value;
        }

        return ValuesReturned.getReturnedOrNewDoubleValue(preferenceName, getValueSupplier());
    }

    private Supplier<Double> getValueSupplier() {
        if (mValueSupplier == null) {
            mValueSupplier =
                    () -> {
                        String preferenceName = getSharedPreferenceKey();
                        Double value =
                                CachedFlagsSafeMode.getInstance()
                                        .getDoubleFieldTrialParam(preferenceName, mDefaultValue);
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

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * <p>Caveat: this does not affect the value returned by native, only by {@link
     * CachedFieldTrialParameter}.
     *
     * @param overrideValue the value to be returned
     */
    public void setForTesting(double overrideValue) {
        ValuesOverridden.setOverrideForTesting(
                getSharedPreferenceKey(), String.valueOf(overrideValue));
    }
}
