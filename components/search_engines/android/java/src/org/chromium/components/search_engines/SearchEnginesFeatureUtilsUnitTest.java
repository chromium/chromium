// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Map;

@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
public class SearchEnginesFeatureUtilsUnitTest {

    @Test
    public void clayBlockingUseFakeBackend() {
        configureClayBlockingFeatureParams(Map.of("use_fake_backend", "true"));
        assertTrue(SearchEnginesFeatureUtils.clayBlockingUseFakeBackend());

        configureClayBlockingFeatureParams(Map.of("use_fake_backend", "false"));
        assertFalse(SearchEnginesFeatureUtils.clayBlockingUseFakeBackend());

        configureClayBlockingFeatureParams(Map.of("use_fake_backend", ""));
        assertFalse(SearchEnginesFeatureUtils.clayBlockingUseFakeBackend());

        configureClayBlockingFeatureParams(Map.of("use_fake_backend", "bad input"));
        assertFalse(SearchEnginesFeatureUtils.clayBlockingUseFakeBackend());
    }

    @Test
    public void clayBlockingIsDarkLaunch() {
        configureClayBlockingFeatureParams(Map.of("is_dark_launch", "true"));
        assertTrue(SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch());

        configureClayBlockingFeatureParams(Map.of("is_dark_launch", "false"));
        assertFalse(SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch());

        configureClayBlockingFeatureParams(Map.of("is_dark_launch", ""));
        assertFalse(SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch());

        configureClayBlockingFeatureParams(Map.of("is_dark_launch", "bad input"));
        assertFalse(SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch());
    }

    @Test
    public void clayBlockingDialogTimeoutMillis() {
        configureClayBlockingFeatureParams(Map.of("dialog_timeout_millis", "0"));
        assertEquals(SearchEnginesFeatureUtils.clayBlockingDialogTimeoutMillis(), 0);

        configureClayBlockingFeatureParams(Map.of("dialog_timeout_millis", "24"));
        assertEquals(SearchEnginesFeatureUtils.clayBlockingDialogTimeoutMillis(), 24);

        configureClayBlockingFeatureParams(Map.of("dialog_timeout_millis", ""));
        assertThrows(
                NumberFormatException.class,
                SearchEnginesFeatureUtils::clayBlockingDialogTimeoutMillis);

        configureClayBlockingFeatureParams(Map.of("dialog_timeout_millis", "-24"));
        assertEquals(SearchEnginesFeatureUtils.clayBlockingDialogTimeoutMillis(), -24);

        configureClayBlockingFeatureParams(Map.of("dialog_timeout_millis", "bad input"));
        assertThrows(
                NumberFormatException.class,
                SearchEnginesFeatureUtils::clayBlockingDialogTimeoutMillis);
    }

    private static void configureClayBlockingFeatureParams(Map<String, String> paramAndValue) {
        var testFeatures = new FeatureList.TestValues();
        testFeatures.addFeatureFlagOverride(SearchEnginesFeatures.CLAY_BLOCKING, true);

        for (var entry : paramAndValue.entrySet()) {
            testFeatures.addFieldTrialParamOverride(
                    SearchEnginesFeatures.CLAY_BLOCKING, entry.getKey(), entry.getValue());
        }

        FeatureList.setTestValues(testFeatures);
    }
}
