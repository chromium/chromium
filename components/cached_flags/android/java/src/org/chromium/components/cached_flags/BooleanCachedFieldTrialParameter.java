// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.AnyThread;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureMap;
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

        String testValue = FeatureList.getTestValueForFieldTrialParam(mFeatureName, mParamName);
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
     * @param overrideValue the value to be returned
     * @deprecated use <code>@EnableFeatures("Feature:param/value")</code> instead.
     */
    @Deprecated
    public void setForTesting(boolean overrideValue) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(this, String.valueOf(overrideValue));
        FeatureList.mergeTestValues(testValues, /* replace= */ true);
    }
}
