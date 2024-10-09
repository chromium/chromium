// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import androidx.annotation.VisibleForTesting;

/** Helpers to access feature params for {@link SearchEnginesFeatures}. */
public final class SearchEnginesFeatureUtils {
    /**
     * Whether the feature should be powered by a fake backend. This avoid having dependencies on
     * other device apps/components to test the feature.
     *
     * <p>In "fake backend" mode, if the "dialog_timeout_millis" param is set, it is also used to
     * simulate a long-running backend query. It will respond that blocking is required after {@code
     * min(3000, paramValue("dialog_timeout_millis"))} milliseconds.
     *
     * <p>This param is surfaced in {@code chrome://flags}.
     */
    public static boolean clayBlockingUseFakeBackend() {
        return clayBlockingFeatureParamAsBoolean("use_fake_backend", false);
    }

    /**
     * Whether the feature is in "dark launch" mode. It means that even if the feature is enabled:
     *
     * <ul>
     *   <li>The dialog will not be shown
     *   <li>The service query timeout mechanism will not be active, since it starts counting from
     *       when the dialog is shown
     *   <li>Some metrics and events will not be recorded, if they depend on the dialog being shown.
     * </ul>
     */
    public static boolean clayBlockingIsDarkLaunch() {
        return clayBlockingFeatureParamAsBoolean("is_dark_launch", false);
    }

    /**
     * Whether verbose logs should be enabled.
     *
     * <p>This param is surfaced in {@code chrome://flags}.
     */
    public static boolean clayBlockingEnableVerboseLogging() {
        return clayBlockingFeatureParamAsBoolean("enable_verbose_logging", false);
    }

    /**
     * Delay in milliseconds after which the blocking dialog will time out and stop blocking. The
     * timer starts when the dialog is actually shown.
     *
     * <p>Should to be a positive value. The timeout feature will be disabled if an unexpected value
     * is provided.
     */
    public static int clayBlockingDialogTimeoutMillis() {
        return clayBlockingFeatureParamAsInt("dialog_timeout_millis", 60_000);
    }

    /**
     * Millis during which we don't block the user with the dialog while determining whether
     * blocking should be done. After this delay, if we didn't get a response from the backend about
     * whether the blocking should be done, we start blocking by showing the dialog in "Pending"
     * mode, with the action button disable.
     *
     * <p>Should be a positive value. Showing the dialog in "Pending" mode will be disabled if an
     * unexpected value is provided. {@code 0} deliberately also disables the dialog. If we want to
     * show the pending dialog "immediately", we can use another very small duration (e.g. {@code 1
     * ms}), which should be functionally identical.
     */
    public static int clayBlockingDialogSilentlyPendingDurationMillis() {
        return clayBlockingFeatureParamAsInt("silent_pending_duration_millis", 0);
    }

    /**
     * Number of blocked Chrome sessions after which we suppress the blocking dialog. This is
     * intended as an escape hatch for initial iterations of the feature, to mitigate potential
     * bugs.
     *
     * <p>Should be a positive value. The Escape Hatch triggering and suppressing dialog will be
     * disabled if an unexpected value (including {@code 0}) is provided.
     */
    public static int clayBlockingEscapeHatchBlockLimit() {
        return clayBlockingFeatureParamAsInt("escape_hatch_block_limit", 0);
    }

    /**
     * Millis after the OS default browser choice has been made during which Chrome should not offer
     * the user to set Chrome as their default browser.
     *
     * <p>Should be a positive value. Suppressing the default browser promo will not happen if an
     * unexpected value (including {@code 0}) is provided.
     */
    public static int clayBlockingDialogDefaultBrowserPromoSuppressedMillis() {
        // `Integer.MAX_VALUE` should give us a bit more than 24 days, enough for our purposes,
        return clayBlockingFeatureParamAsInt(
                "default_browser_promo_suppressed_millis", 24 * 60 * 60 * 1000);
    }

    @VisibleForTesting
    static boolean clayBlockingFeatureParamAsBoolean(String param, boolean defaultValue) {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)
                : "Avoid accessing params on disabled features!";

        return SearchEnginesFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsBoolean(
                        SearchEnginesFeatures.CLAY_BLOCKING, param, defaultValue);
    }

    @VisibleForTesting
    static int clayBlockingFeatureParamAsInt(String param, int defaultValue) {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)
                : "Avoid accessing params on disabled features!";

        return SearchEnginesFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsInt(
                        SearchEnginesFeatures.CLAY_BLOCKING, param, defaultValue);
    }

    // Do not instantiate this class.
    private SearchEnginesFeatureUtils() {}
}
