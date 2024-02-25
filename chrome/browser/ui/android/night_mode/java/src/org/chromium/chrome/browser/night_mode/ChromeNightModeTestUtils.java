// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * Helper methods to be used in tests to specify night mode state. See also {@link
 * org.chromium.ui.test.util.NightModeTestUtils}.
 */
public class ChromeNightModeTestUtils {
    /**
     * Sets up initial states for night mode before
     * {@link org.chromium.chrome.browser.ChromeActivity} is launched.
     */
    public static void setUpNightModeBeforeChromeActivityLaunched() {
        NightModeUtils.setNightModeSupportedForTesting(true);
    }

    /**
     * Sets up the night mode state for {@link org.chromium.chrome.browser.ChromeActivity}.
     * @param nightModeEnabled Whether night mode should be enabled.
     */
    public static void setUpNightModeForChromeActivity(boolean nightModeEnabled) {
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.UI_THEME_SETTING,
                        nightModeEnabled ? ThemeType.DARK : ThemeType.LIGHT);
    }

    /**
     * Resets the night mode state after {@link org.chromium.chrome.browser.ChromeActivity} is
     * destroyed.
     */
    public static void tearDownNightModeAfterChromeActivityDestroyed() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.UI_THEME_SETTING);
    }
}
