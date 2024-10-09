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

/** A boolean-type {@link CachedFieldTrialParameter}. */
public class BooleanCachedFieldTrialParameter extends CachedFieldTrialParameter<Boolean> {
    private Supplier<Boolean> mValueSupplier;

    public BooleanCachedFieldTrialParameter(
            FeatureMap featureMap, String featureName, String variationName, boolean defaultValue) {
        super(
                featureMap,
                featureName,
                variationName,
                FieldTrialParameterType.BOOLEAN,
                defaultValue);
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    @AnyThread
    public boolean getValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String preferenceName = getSharedPreferenceKey();

        Boolean value = ValuesOverridden.getBool(preferenceName);
        if (value != null) {
            return value;
        }

        return ValuesReturned.getReturnedOrNewBoolValue(preferenceName, getValueSupplier());
    }

    private Supplier<Boolean> getValueSupplier() {
        if (mValueSupplier == null) {
            mValueSupplier =
                    () -> {
                        String preferenceName = getSharedPreferenceKey();
                        Boolean value =
                                CachedFlagsSafeMode.getInstance()
                                        .getBooleanFieldTrialParam(preferenceName, mDefaultValue);
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

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * <p>Caveat: this does not affect the value returned by native, only by {@link
     * CachedFieldTrialParameter}.
     *
     * @param overrideValue the value to be returned
     */
    public void setForTesting(boolean overrideValue) {
        ValuesOverridden.setOverrideForTesting(
                getSharedPreferenceKey(), String.valueOf(overrideValue));
    }
}
