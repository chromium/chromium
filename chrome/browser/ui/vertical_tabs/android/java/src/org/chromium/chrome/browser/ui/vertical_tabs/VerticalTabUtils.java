// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.vertical_tabs;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.SuperscriptSpan;
import android.util.TypedValue;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.base.DeviceInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Helper utilities for Vertical Tabs eligibility and preferences. */
@NullMarked
public class VerticalTabUtils {
    /** The width of the vertical tabs SideUiContainer in dp. */
    public static final int SIDE_UI_CONTAINER_WIDTH_DP = 240;

    /** The width of the collapsed vertical tabs SideUiContainer in dp. */
    public static final int SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP = 76;

    /**
     * Minimum width in dp required for the expanded vertical tabs rail before snapping to collapsed
     * state.
     */
    public static final int MIN_EXPANDED_WIDTH_DP = 92;

    /** The ratio of window width that the vertical tabs rail can consume when expanded. */
    public static final float EXPANDED_WINDOW_WIDTH_RATIO = 0.33f;

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

    // LINT.IfChange(AndroidVerticalTabsWindowWidthBoundary)
    @IntDef({
        WindowWidthBoundary.NOT_SHOWABLE,
        WindowWidthBoundary.FORCED_COLLAPSED,
        WindowWidthBoundary.DYNAMIC_EXPANDABLE,
        WindowWidthBoundary.FULLY_EXPANDABLE
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface WindowWidthBoundary {
        /** The vertical tab rail cannot fit and must not be shown. */
        int NOT_SHOWABLE = 0;

        /**
         * The vertical tab rail fits collapsed, but is forced collapsed because the window is too
         * narrow to expand.
         */
        int FORCED_COLLAPSED = 1;

        /**
         * The vertical tab rail can expand with a dynamic auto-resize width strictly less than
         * {@link #SIDE_UI_CONTAINER_WIDTH_DP}.
         */
        int DYNAMIC_EXPANDABLE = 2;

        /**
         * The vertical tab rail can expand to its full fixed width {@link
         * #SIDE_UI_CONTAINER_WIDTH_DP}.
         */
        int FULLY_EXPANDABLE = 3;

        /** Total number of window width boundary categories for histograms. */
        int COUNT = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidVerticalTabsWindowWidthBoundary)

    /** Feature parameter name for enabling Vertical Tabs by default. */
    public static final String ENABLE_BY_DEFAULT_PARAM = "enable_by_default";

    /** Feature parameter name for enabling the incognito button in the footer. */
    public static final String INCOGNITO_BUTTON_PARAM = "incognito_button";

    /**
     * Returns whether Vertical Tabs should be enabled by default for eligible users who have not
     * explicitly set their preference.
     */
    public static boolean isVerticalTabsEnabledByDefault() {
        return ChromeFeatureList.sAndroidVerticalTabsEnableByDefault.getValue();
    }

    /**
     * Returns whether the current device is a tablet (excluding desktop form factor) for sizing
     * calculations.
     */
    public static boolean isTablet(Context context) {
        assert context != null;
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && !DeviceInfo.isDesktop();
    }

    /**
     * Returns whether the current device is eligible for Vertical Tabs. Vertical Tabs require the
     * AndroidVerticalTabs feature flag to be enabled and the device to be a tablet form factor.
     */
    public static boolean isVerticalTabsEligible(@Nullable Context context) {
        return context != null
                && ChromeFeatureList.sAndroidVerticalTabs.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /**
     * Returns whether Vertical Tabs are enabled.
     *
     * <p>VT is enabled if the device is eligible, and either: 1. The user has explicitly enabled it
     * (preference is true). 2. The user has not set a preference, and VT is enabled by default via
     * the "enable_by_default" feature parameter.
     */
    public static boolean isVerticalTabsEnabled(@Nullable Context context) {
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
     * Records the layout switch entry point, direction, and window width boundary when toggling
     * Vertical Tabs.
     *
     * @param context The Context used to retrieve the screen width in dp.
     * @param entryPoint The entry point from which the layout toggle was triggered.
     * @param isEnabling Whether the user is enabling Vertical Tabs (true) or horizontal (false).
     */
    public static void recordLayoutToggle(
            Context context, @LayoutSwitchEntryPoint int entryPoint, boolean isEnabling) {
        assert context != null;

        RecordHistogram.recordEnumeratedHistogram(
                "Android.VerticalTabs.LayoutToggleSourceAndDirection",
                getLayoutToggleSourceAndDirection(entryPoint, isEnabling),
                LayoutToggleSourceAndDirection.COUNT);

        Resources resources = context.getResources();
        Configuration config = resources != null ? resources.getConfiguration() : null;
        if (config != null) {
            int widthDp = config.screenWidthDp;
            @WindowWidthBoundary int boundary = getWindowWidthBoundary(widthDp);
            if (isEnabling) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.VerticalTabs.WindowWidthBoundaryOnToggle.Enable",
                        boundary,
                        WindowWidthBoundary.COUNT);
            } else {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.VerticalTabs.WindowWidthBoundaryOnToggle.Disable",
                        boundary,
                        WindowWidthBoundary.COUNT);
            }
        }
    }

    /** Loads a float resource value (e.g. for alpha) from the given dimen resource id. */
    public static float getFloatResource(Context context, int resId) {
        TypedValue outValue = new TypedValue();
        context.getResources().getValue(resId, outValue, true);
        return outValue.getFloat();
    }

    /** Returns whether expand-on-hover behavior is enabled for Vertical Tabs. */
    public static boolean isExpandOnHoverEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                "expand_on_hover",
                /* defaultValue= */ false);
    }

    /** Returns whether the incognito button in the footer is enabled for Vertical Tabs. */
    public static boolean isIncognitoButtonEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                INCOGNITO_BUTTON_PARAM,
                /* defaultValue= */ false);
    }

    /**
     * Returns whether the "New" badge should be shown for the "Show tabs vertically" menu item.
     *
     * <p>The badge is shown only on tablets (excluding desktop form factor) and capped at 3
     * impressions until the user clicks the menu item, governed by the Feature Engagement Tracker.
     */
    public static boolean shouldShowNewBadgeForVerticalTabs(
            @Nullable Context context, @Nullable Profile profile) {
        // Show only on tablet devices, not on Desktop.
        if (!isVerticalTabsEligible(context) || DeviceInfo.isDesktop()) {
            return false;
        }

        // Requires a profile to track impressions / usage.
        if (profile == null) {
            return false;
        }

        boolean shouldShow =
                TrackerFactory.getTrackerForProfile(profile)
                        .shouldTriggerHelpUi(FeatureConstants.ANDROID_VERTICAL_TABS_NEW_LABEL);
        // We need to immediately "dismiss" this to complete the flow if it was shown.
        if (shouldShow) {
            TrackerFactory.getTrackerForProfile(profile)
                    .dismissed(FeatureConstants.ANDROID_VERTICAL_TABS_NEW_LABEL);
        }

        return shouldShow;
    }

    /**
     * Returns a formatted title string containing the "New" badge for Vertical Tabs entry points.
     *
     * <p>This helper applies visual spans to the string resource and is designed to be reusable
     * across various Vertical Tabs entry points.
     *
     * @param context The active {@link Context}.
     * @param layoutTitleRes The string resource ID for the layout toggle title (e.g., {@code
     *     R.string.show_tabs_vertically}).
     * @return A {@link CharSequence} with styled "New" badge spans attached.
     */
    public static CharSequence getTitleWithNewBadge(
            Context context, @StringRes int layoutTitleRes) {
        String rawTitle = context.getString(layoutTitleRes);
        return SpanApplier.applySpans(
                context.getString(R.string.prefs_new_label, rawTitle),
                new SpanInfo(
                        "<new>",
                        "</new>",
                        new SuperscriptSpan(),
                        new RelativeSizeSpan(0.75f),
                        new ForegroundColorSpan(
                                SemanticColorUtils.getDefaultTextColorAccent1(context))));
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

    /**
     * Returns the window width boundary classification using default web contents constraints.
     *
     * @param windowWidthDp Total window width in dp.
     * @return The {@link WindowWidthBoundary} for the given window width.
     */
    public static @WindowWidthBoundary int getWindowWidthBoundary(int windowWidthDp) {
        int availableWidthDp = windowWidthDp - SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP;
        return getWindowWidthBoundary(windowWidthDp, availableWidthDp);
    }

    /**
     * Returns the window width boundary classification for a given window and available width.
     *
     * @param windowWidthDp Total window width in dp.
     * @param availableWidthDp Maximum width in dp allocated for the Side UI container.
     * @return The {@link WindowWidthBoundary} for the given window and available widths.
     */
    public static @WindowWidthBoundary int getWindowWidthBoundary(
            int windowWidthDp, int availableWidthDp) {
        // 1. Not showable: Available width cannot fit even the collapsed rail.
        if (availableWidthDp < SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP) {
            return WindowWidthBoundary.NOT_SHOWABLE;
        }

        // 2. Forced Collapsed: Window width or available width cannot fit the minimum expanded
        // width (92dp).
        int minWidthByWebContents =
                SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP + MIN_EXPANDED_WIDTH_DP;
        int minWidthByRatio = Math.round(MIN_EXPANDED_WIDTH_DP / EXPANDED_WINDOW_WIDTH_RATIO);
        int minExpandedWindowWidth = Math.max(minWidthByWebContents, minWidthByRatio);
        if (availableWidthDp < MIN_EXPANDED_WIDTH_DP || windowWidthDp < minExpandedWindowWidth) {
            return WindowWidthBoundary.FORCED_COLLAPSED;
        }

        int ratioWidthDp = Math.round(windowWidthDp * EXPANDED_WINDOW_WIDTH_RATIO);
        int targetWidthDp =
                Math.min(SIDE_UI_CONTAINER_WIDTH_DP, Math.min(ratioWidthDp, availableWidthDp));

        // 3. Dynamic Expandable: Auto-resized width is strictly less than full container width.
        if (targetWidthDp < SIDE_UI_CONTAINER_WIDTH_DP) {
            return WindowWidthBoundary.DYNAMIC_EXPANDABLE;
        }

        // 4. Fully Expandable: Expanded to full SIDE_UI_CONTAINER_WIDTH_DP.
        return WindowWidthBoundary.FULLY_EXPANDABLE;
    }

    /** Resets Vertical Tabs SharedPreferences. For testing use only. */
    public static void resetSharedPrefsForTesting() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.VERTICAL_TABS_ENABLED);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.VERTICAL_TABS_COLLAPSED);
    }
}
