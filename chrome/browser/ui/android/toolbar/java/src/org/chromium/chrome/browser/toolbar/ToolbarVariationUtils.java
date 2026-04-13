// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

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
        if (ChromeFeatureList.sAndroidBottomBarRemoveHomeButton.getValue()) return false;
        // Arm 1C has home button at start of toolbar.
        return ChromeFeatureList.sAndroidBottomBarKeepHomeButtonInToolbar.getValue();
    }

    /** Whether the new toolbar variation UI is enabled. */
    public static boolean isNewToolbarUiEnabled() {
        return ChromeFeatureList.sAndroidBottomBar.isEnabled();
    }
}
