// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

/** Helpers to access feature params for {@link SearchEnginesFeatures}. */
public final class SearchEnginesFeatureUtils {
    public static boolean clayBlockingUseFakeBackend() {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)
                : "Avoid accessing params on disabled features!";

        return SearchEnginesFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsBoolean(
                        SearchEnginesFeatures.CLAY_BLOCKING, "use_fake_backend", false);
    }

    public static boolean clayBlockingIsDarkLaunch() {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)
                : "Avoid accessing params on disabled features!";

        return SearchEnginesFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsBoolean(
                        SearchEnginesFeatures.CLAY_BLOCKING, "is_dark_launch", false);
    }

    public static boolean clayBlockingEnableVerboseLogging() {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)
                : "Avoid accessing params on disabled features!";

        return SearchEnginesFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsBoolean(
                        SearchEnginesFeatures.CLAY_BLOCKING, "enable_verbose_logging", false);
    }

    /**
     * Delay in milliseconds after which the blocking dialog will time out and stop blocking. Should
     * to be a positive value. The timeout feature will be disabled if an unexpected value is
     * provided.
     */
    public static int clayBlockingDialogTimeoutMillis() {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)
                : "Avoid accessing params on disabled features!";

        return SearchEnginesFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsInt(
                        SearchEnginesFeatures.CLAY_BLOCKING, "dialog_timeout_millis", 60_000);
    }

    /**
     * Millis during which we don't block the user with the dialog while determining whether
     * blocking should be done.
     */
    public static int clayBlockingDialogSilentlyPendingDurationMillis() {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)
                : "Avoid accessing params on disabled features!";

        return SearchEnginesFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsInt(
                        SearchEnginesFeatures.CLAY_BLOCKING, "silent_pending_duration_millis", 0);
    }

    // Do not instantiate this class.
    private SearchEnginesFeatureUtils() {}
}
