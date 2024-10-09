// Copyright 2021 The Chromium Authors
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

import java.util.Collections;
import java.util.Map;

/** AllCachedFieldTrialParameters caches all the parameters for a feature. */
public class AllCachedFieldTrialParameters extends CachedFieldTrialParameter<Map<String, String>> {
    private Supplier<String> mValueSupplier;

    public AllCachedFieldTrialParameters(FeatureMap featureMap, String featureName) {
        // As this includes all parameters, the parameterName is empty.
        super(
                featureMap,
                featureName,
                /* parameterName= */ "",
                FieldTrialParameterType.ALL,
                Collections.emptyMap());
    }

    /** Returns a map of field trial parameter to value. */
    @AnyThread
    public Map<String, String> getParams() {
        return CachedFlagsSharedPreferences.decodeJsonEncodedMap(getConsistentStringValue());
    }

    @AnyThread
    private String getConsistentStringValue() {
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
                                        .getStringFieldTrialParam(preferenceName, "");
                        if (value == null) {
                            value =
                                    CachedFlagsSharedPreferences.getInstance()
                                            .readString(preferenceName, "");
                        }
                        return value;
                    };
        }
        return mValueSupplier;
    }

    @Override
    void writeCacheValueToEditor(final SharedPreferences.Editor editor) {
        final Map<String, String> params =
                mFeatureMap.getFieldTrialParamsForFeature(getFeatureName());
        editor.putString(
                getSharedPreferenceKey(), CachedFlagsSharedPreferences.encodeParams(params));
    }

    /** Sets the parameters for the specified feature when used in tests. */
    public static void setForTesting(String featureName, Map<String, String> params) {
        String preferenceKey =
                CachedFlagsSharedPreferences.generateParamSharedPreferenceKey(featureName, "");
        String overrideValue = CachedFlagsSharedPreferences.encodeParams(params);
        ValuesOverridden.setOverrideForTesting(preferenceKey, overrideValue);
    }
}
