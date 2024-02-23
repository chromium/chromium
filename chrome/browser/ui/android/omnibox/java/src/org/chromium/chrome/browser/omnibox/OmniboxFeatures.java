// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.FeatureList;
import org.chromium.base.SysUtils;
import org.chromium.base.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.ui.base.DeviceFormFactor;

/** This is the place where we define these: List of Omnibox features and parameters. */
public class OmniboxFeatures {
    // Threshold for low RAM devices. We won't be showing suggestion images
    // on devices that have less RAM than this to avoid bloat and reduce user-visible
    // slowdown while spinning up an image decompression process.
    // We set the threshold to 1.5GB to reduce number of users affected by this restriction.
    private static final int LOW_MEMORY_THRESHOLD_KB =
            (int) (1.5 * ConversionUtils.KILOBYTES_PER_GIGABYTE);

    /// Holds the information whether logic should focus on preserving memory on this device.
    private static Boolean sIsLowMemoryDevice;

    public static final BooleanCachedFieldTrialParameter ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
                    "enable_modernize_visual_update_on_tablet",
                    false);

    public static final BooleanCachedFieldTrialParameter
            MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX =
                    ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                            ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
                            "modernize_visual_update_active_color_on_omnibox",
                            true);

    public static final BooleanCachedFieldTrialParameter MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
                    "modernize_visual_update_smallest_margins",
                    true);

    public static final BooleanCachedFieldTrialParameter QUERY_TILES_SHOW_AS_CAROUSEL =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.QUERY_TILES_IN_ZPS_ON_NTP, "QueryTilesShowAsCarousel", false);

    public static final int DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION = 5;

    /**
     * @param context The activity context.
     * @return Whether the new modernize visual UI update should be shown.
     */
    public static boolean shouldShowModernizeVisualUpdate(Context context) {
        return ChromeFeatureList.sOmniboxModernizeVisualUpdate.isEnabled()
                && (!isTablet(context) || enabledModernizeVisualUpdateOnTablet());
    }

    /**
     * @return Whether to show an active color for Omnibox which has a different background color
     *     than toolbar.
     */
    public static boolean shouldShowActiveColorOnOmnibox() {
        return MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX.getValue();
    }

    /**
     * Returns whether even smaller vertical and horizontal margins should be used in the visual
     * update.
     */
    public static boolean shouldShowSmallestMargins(Context context) {
        return shouldShowModernizeVisualUpdate(context)
                && MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS.getValue();
    }

    /**
     * @param context The activity context.
     * @return Whether current activity is in tablet mode.
     */
    private static boolean isTablet(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /**
     * @return Whether the new modernize visual UI update should be displayed on tablets.
     */
    private static boolean enabledModernizeVisualUpdateOnTablet() {
        return ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.getValue();
    }

    /** Returns whether the toolbar and status bar color should be matched. */
    public static boolean shouldMatchToolbarAndStatusBarColor() {
        return ChromeFeatureList.sOmniboxMatchToolbarAndStatusBarColor.isEnabled();
    }

    /** Whether Journeys suggestions should be shown in a dedicated row. */
    public static boolean isJourneysRowUiEnabled() {
        return ChromeFeatureList.sOmniboxHistoryClusterProvider.isEnabled();
    }

    /**
     * Returns whether the omnibox's recycler view pool should be pre-warmed prior to initial use.
     */
    public static boolean shouldPreWarmRecyclerViewPool() {
        return !isLowMemoryDevice();
    }

    /**
     * Returns whether the device is to be considered low-end for any memory intensive operations.
     */
    public static boolean isLowMemoryDevice() {
        if (sIsLowMemoryDevice == null) {
            sIsLowMemoryDevice =
                    (SysUtils.amountOfPhysicalMemoryKB() < LOW_MEMORY_THRESHOLD_KB
                            && !CommandLine.getInstance()
                                    .hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE));
        }
        return sIsLowMemoryDevice;
    }

    /**
     * Returns whether clicking the edit url suggestion / search-ready omnibox should be a no-op.
     * Currently the default behavior is to refresh the page.
     */
    public static boolean noopEditUrlSuggestionClicks() {
        return ChromeFeatureList.sOmniboxNoopEditUrlSuggestionClicks.isEnabled();
    }

    /**
     * Returns whether a touch down event on a search suggestion should send a signal to prefetch
     * the corresponding page.
     */
    public static boolean isTouchDownTriggerForPrefetchEnabled() {
        return ChromeFeatureList.sTouchDownTriggerForPrefetch.isEnabled();
    }

    /**
     * Returns the maximum number of prefetches that can be triggered by touch down events within an
     * omnibox session.
     */
    public static int getMaxPrefetchesPerOmniboxSession() {
        if (!FeatureList.isInitialized()) {
            return DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION;
        }
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.OMNIBOX_TOUCH_DOWN_TRIGGER_FOR_PREFETCH,
                "max_prefetches_per_omnibox_session",
                DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION);
    }

    /** Returns whether the visible url in the url bar should be truncated. */
    public static boolean shouldTruncateVisibleUrl() {
        return ChromeFeatureList.sVisibleUrlTruncation.isEnabled();
    }

    public static boolean shouldTruncateVisibleUrlV2() {
        return ChromeFeatureList.sVisibleUrlTruncationV2.isEnabled();
    }

    /**
     * @param context The activity context.
     * @return Whether to calculate the visible hint. We always calculate the visible hint, except
     *     on tablets that have sNoVisibleHintForTablets enabled.
     */
    public static boolean shouldCalculateVisibleHint(Context context) {
        return !(isTablet(context) && ChromeFeatureList.sNoVisibleHintForTablets.isEnabled());
    }

    /**
     * Returns if we should omit calculating the visible hint if the TLD is different than the
     * previous call to setText().
     */
    public static boolean shouldOmitVisibleHintCalculationForDifferentTLD() {
        return ChromeFeatureList.sNoVisibleHintForDifferentTLD.isEnabled();
    }

    /** Returns whether to show the incognito status for tablet. */
    public static boolean showIncognitoStatusForTablet() {
        return ChromeFeatureList.sTabletToolbarIncognitoStatus.isEnabled()
                || ChromeFeatureList.sDynamicTopChrome.isEnabled();
    }
}
