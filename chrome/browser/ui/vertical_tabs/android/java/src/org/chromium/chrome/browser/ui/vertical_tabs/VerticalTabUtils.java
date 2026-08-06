// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.vertical_tabs;

import android.content.Context;
import android.util.TypedValue;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

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

    @IntDef({
        LayoutSwitchEntryPoint.APP_MENU,
        LayoutSwitchEntryPoint.TAB_CONTEXT_MENU,
        LayoutSwitchEntryPoint.TAB_STRIP_CONTEXT_MENU
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LayoutSwitchEntryPoint {
        int APP_MENU = 0;
        int TAB_CONTEXT_MENU = 1;
        int TAB_STRIP_CONTEXT_MENU = 2;
        int COUNT = 3;
    }

    // LINT.IfChange(AndroidVerticalTabsLayoutToggleSourceAndDirection)
    @IntDef({
        LayoutToggleSourceAndDirection.ENABLE_APP_MENU,
        LayoutToggleSourceAndDirection.ENABLE_TAB_CONTEXT_MENU,
        LayoutToggleSourceAndDirection.ENABLE_TAB_STRIP_CONTEXT_MENU,
        LayoutToggleSourceAndDirection.DISABLE_APP_MENU,
        LayoutToggleSourceAndDirection.DISABLE_TAB_CONTEXT_MENU,
        LayoutToggleSourceAndDirection.DISABLE_TAB_STRIP_CONTEXT_MENU,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LayoutToggleSourceAndDirection {
        int ENABLE_APP_MENU = 0;
        int ENABLE_TAB_CONTEXT_MENU = 1;
        int ENABLE_TAB_STRIP_CONTEXT_MENU = 2;
        int DISABLE_APP_MENU = 3;
        int DISABLE_TAB_CONTEXT_MENU = 4;
        int DISABLE_TAB_STRIP_CONTEXT_MENU = 5;
        int COUNT = 6;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidVerticalTabsLayoutToggleSourceAndDirection)

    /** Feature parameter name for enabling Vertical Tabs by default. */
    public static final String ENABLE_BY_DEFAULT_PARAM = "enable_by_default";

    /**
     * Returns whether Vertical Tabs should be enabled by default for eligible users who have not
     * explicitly set their preference.
     */
    public static boolean isVerticalTabsEnabledByDefault() {
        return ChromeFeatureList.sAndroidVerticalTabsEnableByDefault.getValue();
    }

    /**
     * Returns whether the current device is eligible for Vertical Tabs. Vertical Tabs require the
     * AndroidVerticalTabs feature flag to be enabled and the device to be a tablet form factor.
     */
    public static boolean isVerticalTabsEligible(Context context) {
        return ChromeFeatureList.sAndroidVerticalTabs.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /**
     * Returns whether Vertical Tabs are enabled.
     *
     * <p>VT is enabled if the device is eligible, and either: 1. The user has explicitly enabled it
     * (preference is true). 2. The user has not set a preference, and VT is enabled by default via
     * the "enable_by_default" feature parameter.
     */
    public static boolean isVerticalTabsEnabled(Context context) {
        if (!isVerticalTabsEligible(context)) {
            return false;
        }
        boolean defaultValue = isVerticalTabsEnabledByDefault();
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, defaultValue);
    }

    /**
     * Sets whether Vertical Tabs are enabled in shared preferences.
     *
     * @param enabled Whether Vertical Tabs should be enabled.
     */
    public static void setVerticalTabsEnabled(boolean enabled) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, enabled);
    }

    /** Returns whether the vertical tabs rail is collapsed as stored in shared preferences. */
    public static boolean isRailCollapsedFromSharedPref() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_COLLAPSED, false);
    }

    /** Sets whether the vertical tabs rail is collapsed in shared preferences. */
    public static void setRailCollapsedInSharedPref(boolean collapsed) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_COLLAPSED, collapsed);
    }

    /**
     * Records the layout switch entry point and direction when toggling Vertical Tabs.
     *
     * @param entryPoint The entry point from which the layout toggle was triggered.
     * @param isEnabling Whether the user is enabling Vertical Tabs (true) or horizontal (false).
     */
    public static void recordLayoutToggle(
            @LayoutSwitchEntryPoint int entryPoint, boolean isEnabling) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.VerticalTabs.LayoutToggleSourceAndDirection",
                getLayoutToggleSourceAndDirection(entryPoint, isEnabling),
                LayoutToggleSourceAndDirection.COUNT);
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

    private static @LayoutToggleSourceAndDirection int getLayoutToggleSourceAndDirection(
            @LayoutSwitchEntryPoint int entryPoint, boolean isEnabling) {
        if (isEnabling) {
            switch (entryPoint) {
                case LayoutSwitchEntryPoint.APP_MENU:
                    return LayoutToggleSourceAndDirection.ENABLE_APP_MENU;
                case LayoutSwitchEntryPoint.TAB_CONTEXT_MENU:
                    return LayoutToggleSourceAndDirection.ENABLE_TAB_CONTEXT_MENU;
                case LayoutSwitchEntryPoint.TAB_STRIP_CONTEXT_MENU:
                    return LayoutToggleSourceAndDirection.ENABLE_TAB_STRIP_CONTEXT_MENU;
            }
        } else {
            switch (entryPoint) {
                case LayoutSwitchEntryPoint.APP_MENU:
                    return LayoutToggleSourceAndDirection.DISABLE_APP_MENU;
                case LayoutSwitchEntryPoint.TAB_CONTEXT_MENU:
                    return LayoutToggleSourceAndDirection.DISABLE_TAB_CONTEXT_MENU;
                case LayoutSwitchEntryPoint.TAB_STRIP_CONTEXT_MENU:
                    return LayoutToggleSourceAndDirection.DISABLE_TAB_STRIP_CONTEXT_MENU;
            }
        }
        assert false : "Invalid entry point or direction";
        return LayoutToggleSourceAndDirection.ENABLE_APP_MENU;
    }
}
