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
        // Returns true if app menu is not kept in toolbar (Arm 1A).
        return !ChromeFeatureList.sAndroidBottomBarKeepAppMenuInToolbar.getValue();
    }

    /** Whether the app menu should be in the toolbar. */
    public static boolean shouldAppMenuBeInToolbar() {
        // Arm 1B has app menu in toolbar.
        return ChromeFeatureList.sAndroidBottomBarKeepAppMenuInToolbar.getValue();
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
