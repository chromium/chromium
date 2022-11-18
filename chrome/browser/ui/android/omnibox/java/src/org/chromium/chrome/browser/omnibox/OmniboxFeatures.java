// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;

import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * This is the place where we define these:
 *   List of Omnibox features and parameters.
 */
public class OmniboxFeatures {
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
     *         than toolbar.
     */
    public static boolean shouldShowActiveColorOnOmnibox() {
        return MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX.getValue();
    }

    /**
     * @return Whether to show an active color for Omnibox which has a different background color
     *         than toolbar.
     */
    public static boolean shouldShowSmallBottomMargin() {
        return MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN.getValue();
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
     * Returns whether excessive calls to RecycledViewPool#clear should be removed.
     */
    public static boolean shouldRemoveExcessiveRecycledViewClearCalls() {
        return ChromeFeatureList.sOmniboxRemoveExcessiveRecycledViewClearCalls.isEnabled();
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
}
