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

/** An int-type {@link CachedFieldTrialParameter}. */
public class IntCachedFieldTrialParameter extends CachedFieldTrialParameter<Integer> {
    private Supplier<Integer> mValueSupplier;

    public IntCachedFieldTrialParameter(
            FeatureMap featureMap, String featureName, String variationName, int defaultValue) {
        super(featureMap, featureName, variationName, FieldTrialParameterType.INT, defaultValue);
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    @AnyThread
    public int getValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String testValue = FeatureList.getTestValueForFieldTrialParam(mFeatureName, mParamName);
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
                                        .getIntFieldTrialParam(preferenceName, mDefaultValue);
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

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * @param overrideValue the value to be returned
     * @deprecated use <code>@EnableFeatures("Feature:param/value")</code> instead.
     */
    @Deprecated
    public void setForTesting(int overrideValue) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(this, String.valueOf(overrideValue));
        FeatureList.mergeTestValues(testValues, /* replace= */ true);
    }
}
