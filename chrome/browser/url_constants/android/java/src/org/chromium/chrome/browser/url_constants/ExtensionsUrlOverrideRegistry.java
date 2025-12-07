// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Used to store information on whether extensions have overridden URLs. */
@NullMarked
public class ExtensionsUrlOverrideRegistry {
    private static final String EXTENSIONS_BOOKMARKS_URL_OVERRIDE_ENABLED = "BOOKMARKS";
    private static final String EXTENSIONS_HISTORY_URL_OVERRIDE_ENABLED = "HISTORY";
    private static final String EXTENSIONS_INCOGNITO_BOOKMARKS_URL_OVERRIDE_ENABLED =
            "BOOKMARKS_INCOGNITO";
    private static final String EXTENSIONS_INCOGNITO_NTP_URL_OVERRIDE_ENABLED = "NTP_INCOGNITO";
    private static final String EXTENSIONS_NTP_URL_OVERRIDE_ENABLED = "NTP";

    // Prevent instantiation.
    private ExtensionsUrlOverrideRegistry() {}

    /** Returns true if an extension has overridden the NTP page. */
    public static boolean getNtpOverrideEnabled() {
        return getPrefs().readBoolean(buildKey(EXTENSIONS_NTP_URL_OVERRIDE_ENABLED), false);
    }

    /** Returns true if extensions can override pages in incognito mode for incognito mode. */
    public static boolean getIncognitoNtpOverrideEnabled() {
        return getPrefs()
                .readBoolean(buildKey(EXTENSIONS_INCOGNITO_NTP_URL_OVERRIDE_ENABLED), false);
    }

    /** Returns true if an extension has overridden the history page. */
    public static boolean getHistoryPageOverrideEnabled() {
        return getPrefs().readBoolean(buildKey(EXTENSIONS_HISTORY_URL_OVERRIDE_ENABLED), false);
    }

    /** Returns true if an extension has overridden the bookmarks page. */
    public static boolean getBookmarksPageOverrideEnabled() {
        return getPrefs().readBoolean(buildKey(EXTENSIONS_BOOKMARKS_URL_OVERRIDE_ENABLED), false);
    }

    /** Returns true if an extension has overridden the bookmarks page. */
    public static boolean isBookmarksPageOverridden(boolean isIncognito) {
        return isIncognito
                ? getIncognitoBookmarksPageOverrideEnabled()
                : getBookmarksPageOverrideEnabled();
    }

    /** Returns true if an extension has overridden the bookmarks page for incognito mode. */
    public static boolean getIncognitoBookmarksPageOverrideEnabled() {
        return getPrefs()
                .readBoolean(buildKey(EXTENSIONS_INCOGNITO_BOOKMARKS_URL_OVERRIDE_ENABLED), false);
    }

    /** Sets if an extension has overridden the NTP page. */
    public static void setNtpOverrideEnabled(boolean status) {
        getPrefs().writeBoolean(buildKey(EXTENSIONS_NTP_URL_OVERRIDE_ENABLED), status);
    }

    /** Sets if extensions can override pages in incognito mode. */
    public static void setIncognitoNtpOverrideEnabled(boolean status) {
        getPrefs().writeBoolean(buildKey(EXTENSIONS_INCOGNITO_NTP_URL_OVERRIDE_ENABLED), status);
    }

    /** Sets if an extension has overridden the history page. */
    public static void setHistoryPageOverrideEnabled(boolean status) {
        getPrefs().writeBoolean(buildKey(EXTENSIONS_HISTORY_URL_OVERRIDE_ENABLED), status);
    }

    /** Sets if an extension has overridden the bookmarks page. */
    public static void setBookmarksPageOverrideEnabled(boolean status) {
        getPrefs().writeBoolean(buildKey(EXTENSIONS_BOOKMARKS_URL_OVERRIDE_ENABLED), status);
    }

    /** Sets if an extension has overridden the bookmarks page for incognito mode. */
    public static void setIncognitoBookmarksPageOverrideEnabled(boolean status) {
        getPrefs()
                .writeBoolean(
                        buildKey(EXTENSIONS_INCOGNITO_BOOKMARKS_URL_OVERRIDE_ENABLED), status);
    }

    private static SharedPreferencesManager getPrefs() {
        return ChromeSharedPreferences.getInstance();
    }

    private static String buildKey(String string) {
        return ChromePreferenceKeys.EXTENSIONS_CHROME_PAGE_URL_OVERRIDE_ENABLED.createKey(string);
    }

    /** Disables all overrides. */
    public static void resetRegistry() {
        setNtpOverrideEnabled(false);
        setBookmarksPageOverrideEnabled(false);
        setHistoryPageOverrideEnabled(false);
        setIncognitoNtpOverrideEnabled(false);
        setIncognitoBookmarksPageOverrideEnabled(false);
    }
}
