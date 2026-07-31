// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * Contains various predicates for whether a given URL is overridden. Hides various registries
 * backing the given overrides.
 */
@NullMarked
public class UrlOverrideUtils {
    /** Returns true if the NTP is overridden. */
    public static boolean isNtpOverrideEnabled() {
        return ExtensionsUrlOverrideRegistry.getNtpOverrideEnabled()
                || PolicyUrlOverrideRegistry.getNewTabPageLocationOverrideEnabled();
    }

    /**
     * Returns true if the WebUI NTP override is enabled. This runtime check evaluates to true only
     * when it's a desktop platform, the WebUI NTP feature is enabled, and the user's DSE is Google.
     *
     * <p>In case the user's DSE changes to a 3P DSE at runtime, then the WebUI NTP override
     * eligibility evaluates to false, and the native NTP is used instead. As of today, the WebUI
     * NTP override only applies to desktop Android platforms.
     */
    public static boolean isWebUiNtpOverrideEnabled() {
        if (!DeviceInfo.isDesktop()) {
            return false;
        }
        if (!ChromeFeatureList.sUseWebUiNtpAndroid.isEnabled()) {
            return false;
        }
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, true);
    }

    /** Returns true if the bookmarks page is overridden. */
    public static boolean isBookmarksPageOverrideEnabled() {
        return ExtensionsUrlOverrideRegistry.getBookmarksPageOverrideEnabled();
    }

    /** Returns true if the history page is overridden. */
    public static boolean isHistoryPageOverrideEnabled() {
        return ExtensionsUrlOverrideRegistry.getHistoryPageOverrideEnabled();
    }

    /** Returns true if the NTP is overridden for incognito. */
    public static boolean isIncognitoNtpOverrideEnabled() {
        return ExtensionsUrlOverrideRegistry.getIncognitoNtpOverrideEnabled();
    }

    /** Returns true if the bookmarks page is overridden for incognito. */
    public static boolean isIncognitoBookmarksPageOverrideEnabled() {
        return ExtensionsUrlOverrideRegistry.getIncognitoBookmarksPageOverrideEnabled();
    }
}
