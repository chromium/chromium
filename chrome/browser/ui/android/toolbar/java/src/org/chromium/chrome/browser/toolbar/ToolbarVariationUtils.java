// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;

/** Utility class for determining the configuration of the toolbar variations. */
@NullMarked
public final class ToolbarVariationUtils {

    private ToolbarVariationUtils() {}

    /** Whether the back button should be in the omnibox. */
    public static boolean shouldBackButtonBeInOmnibox() {
        boolean keepAppMenu = ChromeFeatureList.sAndroidBottomBarKeepAppMenuInToolbar.getValue();
        boolean keepHome = ChromeFeatureList.sAndroidBottomBarKeepHomeButtonInToolbar.getValue();
        // Returns true if home is kept (Arm 1C) or app menu is not kept (Arm 1A).
        return keepHome || !keepAppMenu;
    }

    /** Whether the app menu should be in the toolbar. */
    public static boolean shouldAppMenuBeInToolbar() {
        // Arm 1B and Arm 1C have app menu in toolbar.
        return ChromeFeatureList.sAndroidBottomBarKeepAppMenuInToolbar.getValue();
    }

    /** Whether the home button should be at the start of the toolbar. */
    public static boolean shouldHomeButtonBeAtStartOfToolbar() {
        // Arm 1C has home button at start of toolbar.
        return ChromeFeatureList.sAndroidBottomBarKeepHomeButtonInToolbar.getValue();
    }

    /**
     * Whether the app menu button, home button, optional button, and tab switcher button should
     * have their visibility and/or position changed in the {@code ToolbarPhone} and {@code
     * LocationBar} based on the current configuration and whether the current tab is a regular NTP.
     *
     * @param context The current context.
     * @param isNtp Whether the current tab is a regular NTP.
     * @return Whether the toolbar buttons should be modified.
     */
    public static boolean shouldModifyToolbarButtons(Context context, boolean isNtp) {
        if (isNtp && ChromeFeatureList.sAndroidBottomBarDisableOnNtp.getValue()) {
            return false;
        }
        return isToolbarUiRefactorEnabled(context);
    }

    // LINT.IfChange(isToolbarUiRefactorEnabled)
    /**
     * Whether the toolbar UI refactor is enabled. This controls changes to the toolbar layout and
     * behavior only for phone form factors when the Android Bottom Bar feature is enabled.
     */
    public static boolean isToolbarUiRefactorEnabled(Context context) {
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && !DeviceInfo.isAutomotive()
                && ChromeFeatureList.sAndroidBottomBar.isEnabled();
    }
    // LINT.ThenChange(//chrome/browser/ui/android/bottombar/java/src/org/chromium/chrome/browser/ui/bottombar/BottomBarConfigUtils.java:isBottomBarEnabled)
}
