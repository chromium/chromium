// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines.test.util;

import androidx.annotation.Nullable;

import org.chromium.base.FeatureList;
import org.chromium.components.search_engines.SearchEnginesFeatures;

import java.util.Map;

public final class SearchEnginesFeaturesTestUtil {

    /**
     * Helper to call {@link #configureFeatureParams} for {@link
     * SearchEnginesFeatures#CLAY_BLOCKING}.
     */
    public static void configureClayBlockingFeatureParams(
            @Nullable Map<String, String> paramsAndValues) {
        configureFeatureParams(SearchEnginesFeatures.CLAY_BLOCKING, paramsAndValues);
    }

    /**
     * Helper to override feature flag states.
     *
     * @param featureName the name of the feature to configure.
     * @param paramsAndValues the state in which the feature will be set. If {@code null}, the
     *     feature will be disabled. Otherwise, the feature will be enabled and the entries of this
     *     map will be used to configure the feature params.
     */
    public static void configureFeatureParams(
            String featureName, @Nullable Map<String, String> paramsAndValues) {
        var testFeatures = new FeatureList.TestValues();

        if (paramsAndValues == null) {
            // Disable the feature.
            testFeatures.addFeatureFlagOverride(featureName, false);
        } else {
            // Enable the feature and set the params.
            testFeatures.addFeatureFlagOverride(featureName, true);
            for (var entry : paramsAndValues.entrySet()) {
                testFeatures.addFieldTrialParamOverride(
                        featureName, entry.getKey(), entry.getValue());
            }
        }

        FeatureList.mergeTestValues(testFeatures, /* replace= */ true);
    }

    // Block instantiation
    private SearchEnginesFeaturesTestUtil() {}
}
