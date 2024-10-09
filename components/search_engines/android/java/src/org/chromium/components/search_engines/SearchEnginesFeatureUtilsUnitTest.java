// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import static org.chromium.components.search_engines.test.util.SearchEnginesFeaturesTestUtil.configureClayBlockingFeatureParams;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Map;

@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
public class SearchEnginesFeatureUtilsUnitTest {

    @Test
    public void clayBlockingFeatureParamAsBoolean() {
        configureClayBlockingFeatureParams(Map.of("bool_param", "true"));
        assertTrue(
                SearchEnginesFeatureUtils.clayBlockingFeatureParamAsBoolean("bool_param", false));

        configureClayBlockingFeatureParams(Map.of("bool_param", "false"));
        assertFalse(
                SearchEnginesFeatureUtils.clayBlockingFeatureParamAsBoolean("bool_param", false));

        configureClayBlockingFeatureParams(Map.of("bool_param", ""));
        assertFalse(
                SearchEnginesFeatureUtils.clayBlockingFeatureParamAsBoolean("bool_param", false));

        configureClayBlockingFeatureParams(Map.of("bool_param", "bad input"));
        assertFalse(
                SearchEnginesFeatureUtils.clayBlockingFeatureParamAsBoolean("bool_param", false));

        configureClayBlockingFeatureParams(Map.of());
        assertFalse(
                SearchEnginesFeatureUtils.clayBlockingFeatureParamAsBoolean("bool_param", false));
    }

    @Test
    public void clayBlockingFeatureParamAsInt() {
        configureClayBlockingFeatureParams(Map.of("int_param", "0"));
        assertEquals(SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42), 0);

        configureClayBlockingFeatureParams(Map.of("int_param", "24"));
        assertEquals(SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42), 24);

        configureClayBlockingFeatureParams(Map.of("int_param", ""));
        assertThrows(
                NumberFormatException.class,
                () -> SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42));

        configureClayBlockingFeatureParams(Map.of("int_param", "-24"));
        assertEquals(SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42), -24);

        configureClayBlockingFeatureParams(Map.of("int_param", "bad input"));
        assertThrows(
                NumberFormatException.class,
                () -> SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42));

        configureClayBlockingFeatureParams(Map.of());
        assertEquals(SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42), 42);
    }

    @Test
    public void clayBlockingUseFakeBackend() {
        configureClayBlockingFeatureParams(Map.of("use_fake_backend", "true"));
        assertTrue(SearchEnginesFeatureUtils.clayBlockingUseFakeBackend());

        configureClayBlockingFeatureParams(Map.of());
        assertFalse(SearchEnginesFeatureUtils.clayBlockingUseFakeBackend());
    }

    @Test
    public void clayBlockingIsDarkLaunch() {
        configureClayBlockingFeatureParams(Map.of("is_dark_launch", "true"));
        assertTrue(SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch());

        configureClayBlockingFeatureParams(Map.of());
        assertFalse(SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch());
    }

    @Test
    public void clayBlockingEnableVerboseLogging() {
        configureClayBlockingFeatureParams(Map.of("enable_verbose_logging", "true"));
        assertTrue(SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging());

        configureClayBlockingFeatureParams(Map.of());
        assertFalse(SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging());
    }

    @Test
    public void clayBlockingDialogTimeoutMillis() {
        configureClayBlockingFeatureParams(Map.of("dialog_timeout_millis", "24"));
        assertEquals(SearchEnginesFeatureUtils.clayBlockingDialogTimeoutMillis(), 24);

        configureClayBlockingFeatureParams(Map.of());
        assertEquals(SearchEnginesFeatureUtils.clayBlockingDialogTimeoutMillis(), 60_000);
    }

    @Test
    public void clayBlockingDialogSilentlyPendingDurationMillis() {
        configureClayBlockingFeatureParams(Map.of("silent_pending_duration_millis", "24"));
        assertEquals(
                SearchEnginesFeatureUtils.clayBlockingDialogSilentlyPendingDurationMillis(), 24);

        configureClayBlockingFeatureParams(Map.of());
        assertEquals(
                SearchEnginesFeatureUtils.clayBlockingDialogSilentlyPendingDurationMillis(), 0);
    }

    @Test
    public void clayBlockingDialogDefaultBrowserPromoSuppressedMillis() {
        configureClayBlockingFeatureParams(Map.of("default_browser_promo_suppressed_millis", "24"));
        assertEquals(
                24,
                SearchEnginesFeatureUtils.clayBlockingDialogDefaultBrowserPromoSuppressedMillis());

        configureClayBlockingFeatureParams(Map.of());
        assertEquals(
                24 * 60 * 60 * 1000,
                SearchEnginesFeatureUtils.clayBlockingDialogDefaultBrowserPromoSuppressedMillis());
    }
}
