// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.vertical_tabs;

import android.content.Context;
import android.util.TypedValue;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.base.DeviceFormFactor;

/** Helper utilities for Vertical Tabs eligibility and preferences. */
@NullMarked
public class VerticalTabUtils {
    /** The width of the vertical tabs SideUiContainer in dp. */
    public static final int SIDE_UI_CONTAINER_WIDTH_DP = 240;

    /** The width of the collapsed vertical tabs SideUiContainer in dp. */
    public static final int SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP = 76;

    /**
     * Minimum window width threshold in dp required to allow expanding vertical tabs rail and
     * enable collapse button.
     */
    public static final int MIN_EXPAND_WINDOW_WIDTH_DP = 652;

    /**
     * Returns whether the current device is eligible for Vertical Tabs. Vertical Tabs require the
     * AndroidVerticalTabs feature flag to be enabled and the device to be a tablet form factor.
     */
    public static boolean isVerticalTabsEligible(Context context) {
        return ChromeFeatureList.sAndroidVerticalTabs.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /** Returns whether Vertical Tabs are enabled by both eligibility and user preference. */
    public static boolean isVerticalTabsEnabled(Context context) {
        if (!isVerticalTabsEligible(context)) {
            return false;
        }
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false);
    }

    /** Loads a float resource value (e.g. for alpha) from the given dimen resource id. */
    public static float getFloatResource(Context context, int resId) {
        TypedValue outValue = new TypedValue();
        context.getResources().getValue(resId, outValue, true);
        return outValue.getFloat();
    }

    /** Feature parameter name for enabling dragging out tab group headers. */
    public static final String GROUP_HEADER_DRAG_PARAM = "group_header_drag";

    /** Returns whether expand-on-hover behavior is enabled for Vertical Tabs. */
    public static boolean isExpandOnHoverEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, "expand_on_hover", false);
    }

    /** Returns whether dragging out tab group headers is enabled for Vertical Tabs. */
    public static boolean isGroupHeaderDragEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, GROUP_HEADER_DRAG_PARAM, false);
    }
}
