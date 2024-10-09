// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;

import org.chromium.base.FeatureMap;
import org.chromium.base.cached_flags.CachedFlagsSharedPreferences;
import org.chromium.base.cached_flags.ValuesOverridden;
import org.chromium.base.cached_flags.ValuesReturned;
import org.chromium.base.supplier.Supplier;

/** A String-type {@link CachedFieldTrialParameter}. */
public class StringCachedFieldTrialParameter extends CachedFieldTrialParameter<String> {
    private Supplier<String> mValueSupplier;

    public StringCachedFieldTrialParameter(
            FeatureMap featureMap,
            String featureName,
            String variationName,
            @NonNull String defaultValue) {
        super(featureMap, featureName, variationName, FieldTrialParameterType.STRING, defaultValue);
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    @AnyThread
    public String getValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String preferenceName = getSharedPreferenceKey();

        String value = ValuesOverridden.getString(preferenceName);
        if (value != null) {
            return value;
        }

        return ValuesReturned.getReturnedOrNewStringValue(preferenceName, getValueSupplier());
    }

    private Supplier<String> getValueSupplier() {
        if (mValueSupplier == null) {
            mValueSupplier =
                    () -> {
                        String preferenceName = getSharedPreferenceKey();
                        String value =
                                CachedFlagsSafeMode.getInstance()
                                        .getStringFieldTrialParam(preferenceName, mDefaultValue);
                        if (value == null) {
                            value =
                                    CachedFlagsSharedPreferences.getInstance()
                                            .readString(preferenceName, mDefaultValue);
                        }
                        return value;
                    };
        }
        return mValueSupplier;
    }

    public String getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void writeCacheValueToEditor(final SharedPreferences.Editor editor) {
        final String value = mFeatureMap.getFieldTrialParamByFeature(getFeatureName(), getName());
        editor.putString(getSharedPreferenceKey(), value.isEmpty() ? getDefaultValue() : value);
    }

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * <p>Caveat: this does not affect the value returned by native, only by {@link
     * CachedFieldTrialParameter}.
     *
     * @param overrideValue the value to be returned
     */
    public void setForTesting(String overrideValue) {
        ValuesOverridden.setOverrideForTesting(getSharedPreferenceKey(), overrideValue);
    }
}
