// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines.test.util;

import org.chromium.base.FeatureList;
import org.chromium.components.search_engines.SearchEnginesFeatures;

import java.util.Map;

public final class SearchEnginesFeaturesTestUtil {

    public static void configureClayBlockingFeatureParams(Map<String, String> paramAndValue) {
        var testFeatures = new FeatureList.TestValues();
        testFeatures.addFeatureFlagOverride(SearchEnginesFeatures.CLAY_BLOCKING, true);

        for (var entry : paramAndValue.entrySet()) {
            testFeatures.addFieldTrialParamOverride(
                    SearchEnginesFeatures.CLAY_BLOCKING, entry.getKey(), entry.getValue());
        }

        FeatureList.setTestValues(testFeatures);
    }

    // Block instantiation
    private SearchEnginesFeaturesTestUtil() {}
}
