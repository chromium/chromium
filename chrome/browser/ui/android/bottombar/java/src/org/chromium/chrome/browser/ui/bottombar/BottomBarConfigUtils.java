// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.DeviceFormFactor;

/** Utility class for determining the configuration of the bottom bar. */
@NullMarked
public class BottomBarConfigUtils {
    private BottomBarConfigUtils() {}

    /** Whether the bottom bar is enabled. */
    public static boolean isBottomBarEnabled(Context context) {
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && !DeviceInfo.isAutomotive()
                && ChromeFeatureList.sAndroidBottomBar.isEnabled();
    }

    /** Whether to include the home button in the bottom bar if the flag is enabled. */
    public static boolean shouldIncludeHomeButtonIfEnabled() {
        return !ChromeFeatureList.sAndroidBottomBarKeepHomeButtonInToolbar.getValue();
    }

    /** Whether to include the app menu button in the bottom bar if the flag is enabled. */
    public static boolean shouldIncludeAppMenuButton() {
        return !ChromeFeatureList.sAndroidBottomBarKeepAppMenuInToolbar.getValue();
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
        return !tab.isIncognito()
                && tab.getNativePage() != null
                && "newtab".equals(tab.getNativePage().getHost())
                && isBottomBarEnabled(context)
                && !shouldDisableOnNtp()
                && ChromeFeatureList.sAndroidBottomBarNtpScrollOffEnabled.getValue();
    }

    /** Whether to always use the filled GLIC icon. */
    public static boolean alwaysUseFilledIcon() {
        return ChromeFeatureList.sAndroidBottomBarAlwaysUseFilledGlicIcon.getValue();
    }
}
