// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Helpers to access feature params for {@link SearchEnginesFeatures}. */
@NullMarked
public final class SearchEnginesFeatureUtils {

    static final String ENABLE_CHOICE_APIS_DEBUG_SWITCH = "enable-choice-apis-debug";
    static final String ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH = "enable-choice-apis-fake-backend";
    private static final int CHOICE_APIS_CONNECTION_MAX_RETRIES = 2;
    private static @Nullable SearchEnginesFeatureUtils sInstance;

    public static SearchEnginesFeatureUtils getInstance() {
        if (sInstance == null) {
            sInstance = new SearchEnginesFeatureUtils();
        }
        return sInstance;
    }

    public static void setInstanceForTesting(SearchEnginesFeatureUtils instance) {
        var oldInstance = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldInstance);
    }

    /**
     * Whether verbose logs associated with device choice APIs should be enabled.
     *
     * <p>This can be controlled by starting chrome with the <code>--enable-choice-apis-debug</code>
     * command line flag.
     */
    public static boolean isChoiceApisDebugEnabled() {
        return CommandLine.getInstance().hasSwitch(ENABLE_CHOICE_APIS_DEBUG_SWITCH);
    }

    /**
     * Whether the SearchEngineChoiceService should be powered by a fake backend. This avoids having
     * dependencies on other device apps/components to test the feature.
     *
     * <p>This can be controlled by starting chrome with the <code>
     * --enable-choice-apis-fake-backend</code> command line flag.
     *
     * <p>In "fake backend" mode, if the "ClayBlocking" feature's "dialog_timeout_millis" param is
     * set, it is also used to simulate a long-running backend query. It will respond that blocking
     * is required after {@code min(3000, paramValue("dialog_timeout_millis"))} milliseconds.
     */
    public static boolean isChoiceApisFakeBackendEnabled() {
        return CommandLine.getInstance().hasSwitch(ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH);
    }

    @Deprecated
    public static boolean clayBlockingUseFakeBackend() {
        return isChoiceApisFakeBackendEnabled();
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

    @Deprecated
    public static boolean clayBlockingEnableVerboseLogging() {
        return isChoiceApisDebugEnabled();
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
        return clayBlockingFeatureParamAsInt("escape_hatch_block_limit", 10);
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

    /**
     * Number of times a failed backend call will be retried.
     *
     * <p>Should be a positive value. {@code 0} or values that are not positive integers will result
     * in failures being directly propagated without retrying.
     */
    public static int clayConnectionV2MaxRetries() {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BACKEND_CONNECTION_V2);

        return SearchEnginesFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsInt(
                        SearchEnginesFeatures.CLAY_BACKEND_CONNECTION_V2, "max_retries", 2);
    }

    /** Number of times a failed backend call will be retried. */
    public int choiceApisConnectionMaxRetries() {
        return CHOICE_APIS_CONNECTION_MAX_RETRIES;
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
