// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import org.chromium.build.annotations.NullMarked;

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
