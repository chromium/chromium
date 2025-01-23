// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import static org.chromium.components.search_engines.SearchEnginesFeatures.CLAY_BLOCKING;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;

@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
public class SearchEnginesFeatureUtilsUnitTest {

    @Test
    public void clayBlockingFeatureParamAsBoolean() {
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder().enable(CLAY_BLOCKING);
        overrides.apply();
        assertFalse(
                SearchEnginesFeatureUtils.clayBlockingFeatureParamAsBoolean("bool_param", false));

        overrides.param("bool_param", true).apply();
        assertTrue(
                SearchEnginesFeatureUtils.clayBlockingFeatureParamAsBoolean("bool_param", false));

        FeatureOverrides.overrideParam(CLAY_BLOCKING, "bool_param", false);
        assertFalse(
                SearchEnginesFeatureUtils.clayBlockingFeatureParamAsBoolean("bool_param", false));

        FeatureOverrides.overrideParam(CLAY_BLOCKING, "bool_param", "");
        assertFalse(
                SearchEnginesFeatureUtils.clayBlockingFeatureParamAsBoolean("bool_param", false));

        FeatureOverrides.overrideParam(CLAY_BLOCKING, "bool_param", "bad input");
        assertFalse(
                SearchEnginesFeatureUtils.clayBlockingFeatureParamAsBoolean("bool_param", false));
    }

    @Test
    public void clayBlockingFeatureParamAsInt() {
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder().enable(CLAY_BLOCKING);
        overrides.apply();
        assertEquals(42, SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42));

        overrides.param("int_param", 0).apply();
        assertEquals(0, SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42));

        overrides.param("int_param", 24).apply();
        assertEquals(24, SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42));

        overrides.param("int_param", "").apply();
        assertThrows(
                NumberFormatException.class,
                () -> SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42));

        overrides.param("int_param", -24).apply();
        assertEquals(-24, SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42));

        overrides.param("int_param", "bad input").apply();
        assertThrows(
                NumberFormatException.class,
                () -> SearchEnginesFeatureUtils.clayBlockingFeatureParamAsInt("int_param", 42));
    }

    @Test
    public void clayBlockingUseFakeBackend() {
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder().enable(CLAY_BLOCKING);
        overrides.apply();
        assertFalse(SearchEnginesFeatureUtils.clayBlockingUseFakeBackend());

        overrides.param("use_fake_backend", true).apply();
        assertTrue(SearchEnginesFeatureUtils.clayBlockingUseFakeBackend());
    }

    @Test
    public void clayBlockingIsDarkLaunch() {
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder().enable(CLAY_BLOCKING);
        overrides.apply();
        assertFalse(SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch());

        overrides.param("is_dark_launch", true).apply();
        assertTrue(SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch());
    }

    @Test
    public void clayBlockingEnableVerboseLogging() {
        // TODO(crbug.com/391570180): Remove this test once the cleanup is done.
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder().enable(CLAY_BLOCKING);
        overrides.apply();
        assertFalse(SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging());

        overrides.param("enable_verbose_logging", false).apply();
        assertFalse(SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging());
    }

    @Test
    public void clayBlockingDialogTimeoutMillis() {
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder().enable(CLAY_BLOCKING);
        overrides.apply();
        assertEquals(60_000, SearchEnginesFeatureUtils.clayBlockingDialogTimeoutMillis());

        overrides.param("dialog_timeout_millis", 24).apply();
        assertEquals(24, SearchEnginesFeatureUtils.clayBlockingDialogTimeoutMillis());
    }

    @Test
    public void clayBlockingDialogSilentlyPendingDurationMillis() {
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder().enable(CLAY_BLOCKING);
        overrides.apply();
        assertEquals(
                0, SearchEnginesFeatureUtils.clayBlockingDialogSilentlyPendingDurationMillis());

        overrides.param("silent_pending_duration_millis", 24).apply();
        assertEquals(
                24, SearchEnginesFeatureUtils.clayBlockingDialogSilentlyPendingDurationMillis());
    }

    @Test
    public void clayBlockingDialogDefaultBrowserPromoSuppressedMillis() {
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder().enable(CLAY_BLOCKING);
        overrides.apply();
        assertEquals(
                24 * 60 * 60 * 1000,
                SearchEnginesFeatureUtils.clayBlockingDialogDefaultBrowserPromoSuppressedMillis());

        overrides.param("default_browser_promo_suppressed_millis", 24).apply();
        assertEquals(
                24,
                SearchEnginesFeatureUtils.clayBlockingDialogDefaultBrowserPromoSuppressedMillis());
    }
}
