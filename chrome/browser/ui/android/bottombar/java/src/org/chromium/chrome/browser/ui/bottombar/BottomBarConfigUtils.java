// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.DeviceFormFactor;

/** Utility class for determining the configuration of the bottom bar. */
@NullMarked
public class BottomBarConfigUtils {
    public static final int DEFAULT_BOTTOM_BAR_HEIGHT_DP = 60;
    public static final int MIN_BOTTOM_BAR_HEIGHT_DP = 48;
    public static final int MAX_BOTTOM_BAR_HEIGHT_DP = 60;

    private BottomBarConfigUtils() {}

    // LINT.IfChange(isBottomBarEnabled)
    /** Whether the bottom bar is enabled. */
    public static boolean isBottomBarEnabled(Context context) {
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && !DeviceInfo.isAutomotive()
                && ChromeFeatureList.sAndroidBottomBar.isEnabled();
    }
    // LINT.ThenChange(//chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/ToolbarVariationUtils.java:isToolbarUiRefactorEnabled)

    /**
     * Returns the configured height of the bottom bar in DP (clamped to [{@link
     * #MIN_BOTTOM_BAR_HEIGHT_DP}, {@link #MAX_BOTTOM_BAR_HEIGHT_DP}]).
     */
    public static int getBottomBarHeightDp() {
        int heightDp = ChromeFeatureList.sAndroidBottomBarHeightDp.getValue();
        if (heightDp <= 0) return DEFAULT_BOTTOM_BAR_HEIGHT_DP;
        return Math.clamp(heightDp, MIN_BOTTOM_BAR_HEIGHT_DP, MAX_BOTTOM_BAR_HEIGHT_DP);
    }

    /** Whether to include the home button in the bottom bar if the flag is enabled. */
    public static boolean shouldIncludeHomeButtonIfEnabled() {
        return !ChromeFeatureList.sAndroidBottomBarKeepHomeButtonInToolbar.getValue();
    }

    /** Whether to include the app menu button in the bottom bar if the flag is enabled. */
    public static boolean shouldIncludeAppMenuButton() {
        return !ChromeFeatureList.sAndroidBottomBarKeepAppMenuInToolbar.getValue();
    }

    /** Whether to show the update badge in the bottom bar app menu button. */
    public static boolean shouldShowAppMenuUpdateBadge() {
        return ChromeFeatureList.sAndroidBottomBarShowUpdateBadge.getValue();
    }

    /** Whether to show the bottom bar on GTS if the flag is enabled. */
    public static boolean shouldShowOnGts() {
        return ChromeFeatureList.sAndroidBottomBarShowBottomBarOnGts.getValue();
    }

    /** Whether to disable the bottom bar on the regular NTP. */
    public static boolean shouldDisableOnNtp() {
        return ChromeFeatureList.sAndroidBottomBarDisableOnNtp.getValue();
    }

    /**
     * Whether bottom controls scroll-off is enabled for the given tab. Scroll-off is enabled for
     * regular (non-incognito) NTP when the bottom bar is enabled.
     */
    public static boolean isNtpScrollOffEnabled(@Nullable Tab tab, @Nullable Context context) {
        if (tab == null || context == null) return false;
        return !tab.isOffTheRecord()
                && isNtpWithBottomBar(tab, context)
                && ChromeFeatureList.sAndroidBottomBarNtpScrollOffEnabled.getValue();
    }

    /**
     * Whether to force {@link BrowserControlsState#BOTH} constraints for the bottom controls.
     *
     * <p>When the current tab is on an NTP or other native page, the constraints emitted for the
     * bottom bar are overridden and forced to {@link BrowserControlsState#BOTH}. This ensures that
     * ScrollingBottomViewResourceFrameLayout allows screenshot updates, preventing stale
     * screenshots. It does not affect the physical scroll behavior of the bottom bar, which is
     * driven by the actual tab constraints.
     */
    public static boolean shouldForceBothConstraintsForBottomControls(
            @Nullable Tab tab, @Nullable Context context) {
        if (tab == null || context == null) return false;

        if (isRegularNtp(tab) && shouldDisableOnNtp()) return false;

        return tab.isNativePage() || UrlUtilities.isInternalScheme(tab.getUrl());
    }

    /** Whether the given tab is a regular NTP (excludes incognito). */
    public static boolean isRegularNtp(@Nullable Tab tab) {
        return isNtp(tab) && !assumeNonNull(tab).isOffTheRecord();
    }

    /** Whether to always use the filled GLIC icon. */
    public static boolean alwaysUseFilledIcon() {
        return ChromeFeatureList.sAndroidBottomBarAlwaysUseFilledGlicIcon.getValue();
    }

    /** Whether to bypass geofencing country check for GLIC. */
    public static boolean bypassGlicGeofencing() {
        return ChromeFeatureList.sAndroidBottomBarBypassGlicGeofencing.getValue();
    }

    private static boolean isNtpWithBottomBar(Tab tab, Context context) {
        return isNtp(tab)
                && isBottomBarEnabled(context)
                && (tab.isOffTheRecord() || !shouldDisableOnNtp());
    }

    /**
     * Whether the given tab is showing any NTP (regular or incognito).
     *
     * <p>Keys off the native page rather than the URL: the visible URL changes when a navigation
     * starts, but the native page is only swapped when it commits. Frozen tabs keep a
     * FrozenNativePage with the original host.
     */
    public static boolean isNtp(@Nullable Tab tab) {
        NativePage nativePage = tab == null ? null : tab.getNativePage();
        return nativePage != null && UrlConstants.NTP_HOST.equals(nativePage.getHost());
    }

    /** Whether the feature parameter to show the GLIC setting toggle is enabled. */
    public static boolean isGlicSettingToggleParamEnabled() {
        return ChromeFeatureList.sAndroidBottomBarShowGlicSettingToggle.getValue();
    }

    /** Returns whether the GLIC button is enabled by the user in the bottom bar. */
    public static boolean isGlicButtonEnabled() {
        if (!isGlicSettingToggleParamEnabled()) return true;
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.BOTTOM_BAR_GLIC_BUTTON_ENABLED, true);
    }

    /** Sets whether the GLIC button is enabled in the bottom bar. */
    public static void setGlicButtonEnabled(boolean enabled) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.BOTTOM_BAR_GLIC_BUTTON_ENABLED, enabled);
    }
}
