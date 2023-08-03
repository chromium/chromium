// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * This is the place where we define these:
 *   List of Omnibox features and parameters.
 */
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
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
                    "enable_modernize_visual_update_on_tablet", false);

    public static final BooleanCachedFieldTrialParameter
            MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX = new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
                    "modernize_visual_update_active_color_on_omnibox", false);

    public static final BooleanCachedFieldTrialParameter
            MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN = new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
                    "modernize_visual_update_small_bottom_margin", false);

    public static final BooleanCachedFieldTrialParameter MODERNIZE_VISUAL_UPDATE_SMALLER_MARGINS =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
                    "modernize_visual_update_smaller_margins", false);

    public static final BooleanCachedFieldTrialParameter MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
                    "modernize_visual_update_smallest_margins", false);

    public static final BooleanCachedFieldTrialParameter
            MODERNIZE_VISUAL_UPDATE_MERGE_CLIPBOARD_ON_NTP = new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
                    "modernize_visual_update_merge_clipboard_on_ntp", false);

    private static final MutableFlagWithSafeDefault sOmniboxConsumesImeInsets =
            new MutableFlagWithSafeDefault(ChromeFeatureList.OMNIBOX_CONSUMERS_IME_INSETS, true);
    private static final MutableFlagWithSafeDefault sShouldAdaptToNarrowTabletWindows =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.OMNIBOX_ADAPT_NARROW_TABLET_WINDOWS, false);

    private static final MutableFlagWithSafeDefault sJourneysActionChipFlag =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_ACTION_CHIP, false);
    private static final MutableFlagWithSafeDefault sJourneysRowUiFlag =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER, false);

    private static final MutableFlagWithSafeDefault sCacheSuggestionResources =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.OMNIBOX_CACHE_SUGGESTION_RESOURCES, false);

    private static final MutableFlagWithSafeDefault sWarmRecycledViewPoolFlag =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.OMNIBOX_WARM_RECYCLED_VIEW_POOL, false);

    private static final MutableFlagWithSafeDefault sNoopEditUrlSuggestionClicks =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.OMNIBOX_NOOP_EDIT_URL_SUGGESTION_CLICKS, false);

    private static final MutableFlagWithSafeDefault sAvoidRelayoutDuringFocusAnimation =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.AVOID_RELAYOUT_DURING_FOCUS_ANIMATION, false);

    private static final MutableFlagWithSafeDefault sShortCircuitUnfocusAnimation =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.SHORT_CIRCUIT_UNFOCUS_ANIMATION, false);

    /**
     * @param context The activity context.
     * @return Whether the new modernize visual UI update should be shown.
     */
    public static boolean shouldShowModernizeVisualUpdate(Context context) {
        return ChromeFeatureList.sOmniboxModernizeVisualUpdate.isEnabled()
                && (!isTablet(context) || enabledModernizeVisualUpdateOnTablet());
    }

    /**
     * Returns whether the omnibox dropdown should be switched to a phone-like appearance when the
     * window width is <600dp.
     */
    public static boolean shouldAdaptToNarrowTabletWindows() {
        return sShouldAdaptToNarrowTabletWindows.isEnabled();
    }

    /**
     * @return Whether to show an active color for Omnibox which has a different background color
     *         than toolbar.
     */
    public static boolean shouldShowActiveColorOnOmnibox() {
        return MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX.getValue();
    }

    /**
     * Returns whether the margin between groups should be "small" in the visual update.
     */
    public static boolean shouldShowSmallBottomMargin() {
        return MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN.getValue();
    }

    /**
     * Returns whether smaller vertical and horizontal margins should be used in the visual update.
     */
    public static boolean shouldShowSmallerMargins(Context context) {
        return shouldShowModernizeVisualUpdate(context)
                && MODERNIZE_VISUAL_UPDATE_SMALLER_MARGINS.getValue();
    }

    /**
     * Returns whether even smaller vertical and horizontal margins should be used in the visual
     * update.
     */
    public static boolean shouldShowSmallestMargins(Context context) {
        return shouldShowModernizeVisualUpdate(context)
                && MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS.getValue();
    }

    /** Returns whether the omnibox should directly consume IME (keyboard) insets. */
    public static boolean omniboxConsumesImeInsets() {
        return sOmniboxConsumesImeInsets.isEnabled();
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

    /**
     * Returns whether the toolbar and status bar color should be matched.
     */
    public static boolean shouldMatchToolbarAndStatusBarColor() {
        return ChromeFeatureList.sOmniboxMatchToolbarAndStatusBarColor.isEnabled();
    }

    /**
     * Returns whether we need to add a RecycledViewPool to MostVisitedTiles.
     */
    public static boolean shouldAddMostVisitedTilesRecycledViewPool() {
        return ChromeFeatureList.sOmniboxMostVisitedTilesAddRecycledViewPool.isEnabled();
    }

    /** Whether Journeys suggestions should be shown as an action chip. */
    public static boolean isJourneysActionChipEnabled() {
        return sJourneysActionChipFlag.isEnabled();
    }

    /** Whether Journeys suggestions should be shown in a dedicated row. */
    public static boolean isJourneysRowUiEnabled() {
        return sJourneysRowUiFlag.isEnabled();
    }

    /**
     * Returns whether suggestion resources should be cached directly instead of relying on Android
     * system caching.
     */
    public static boolean shouldCacheSuggestionResources() {
        return sCacheSuggestionResources.isEnabled();
    }

    /**
     * Returns whether the omnibox's recycler view pool should be pre-warmed prior to initial use.
     */
    public static boolean shouldPreWarmRecyclerViewPool() {
        return sWarmRecycledViewPoolFlag.isEnabled();
    }

    /**
     * Returns whether the device is to be considered low-end for any memory intensive operations.
     */
    public static boolean isLowMemoryDevice() {
        if (sIsLowMemoryDevice == null) {
            sIsLowMemoryDevice = (SysUtils.amountOfPhysicalMemoryKB() < LOW_MEMORY_THRESHOLD_KB
                    && !CommandLine.getInstance().hasSwitch(
                            BaseSwitches.DISABLE_LOW_END_DEVICE_MODE));
        }
        return sIsLowMemoryDevice;
    }

    /**
     * Returns whether clicking the edit url suggestion / search-ready omnibox should be a no-op.
     * Currently the default behavior is to refresh the page.
     */
    public static boolean noopEditUrlSuggestionClicks() {
        return sNoopEditUrlSuggestionClicks.isEnabled();
    }

    public static boolean shouldAvoidRelayoutDuringFocusAnimation() {
        return sAvoidRelayoutDuringFocusAnimation.isEnabled();
    }

    /**
     * Whether the omnibox unfocus animation should be short-circuited when navigating to a
     * suggestion in order to speed up navigation.
     */
    public static boolean shouldShortCircuitUnfocusAnimation() {
        return sShortCircuitUnfocusAnimation.isEnabled();
    }
}
