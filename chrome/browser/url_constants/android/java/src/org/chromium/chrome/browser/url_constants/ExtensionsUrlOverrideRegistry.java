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
    // Prevent instantiation.
    private ExtensionsUrlOverrideRegistry() {}

    /** Returns true if extensions can override pages in incognito mode. */
    public boolean getIncognitoOverridesEnabledStatus() {
        return getPrefs()
                .readBoolean(
                        ChromePreferenceKeys.EXTENSIONS_INCOGNITO_URL_OVERRIDES_ENABLED, false);
    }

    /** Returns true if an extension has overridden the NTP page. */
    public boolean getNtpOverrideEnabled() {
        return getPrefs()
                .readBoolean(ChromePreferenceKeys.EXTENSIONS_NTP_URL_OVERRIDE_ENABLED, false);
    }

    /** Returns true if an extension has overridden the history page. */
    public boolean getHistoryPageOverrideEnabled() {
        return getPrefs()
                .readBoolean(ChromePreferenceKeys.EXTENSIONS_HISTORY_URL_OVERRIDE_ENABLED, false);
    }

    /** Returns true if an extension has overridden the bookmarks page. */
    public boolean getBookmarksPageOverrideEnabled() {
        return getPrefs()
                .readBoolean(ChromePreferenceKeys.EXTENSIONS_BOOKMARKS_URL_OVERRIDE_ENABLED, false);
    }

    /** Sets if extensions can override pages in incognito mode. */
    public void setIncognitoOverridesEnabledStatus(boolean status) {
        getPrefs()
                .writeBoolean(
                        ChromePreferenceKeys.EXTENSIONS_INCOGNITO_URL_OVERRIDES_ENABLED, status);
    }

    /** Sets if an extension has overridden the NTP page. */
    public void setNtpOverrideEnabled(boolean status) {
        getPrefs().writeBoolean(ChromePreferenceKeys.EXTENSIONS_NTP_URL_OVERRIDE_ENABLED, status);
    }

    /** Sets if an extension has overridden the history page. */
    public void setHistoryPageOverrideEnabled(boolean status) {
        getPrefs()
                .writeBoolean(ChromePreferenceKeys.EXTENSIONS_HISTORY_URL_OVERRIDE_ENABLED, status);
    }

    /** Sets if an extension has overridden the bookmarks page. */
    public void setBookmarksPageOverrideEnabled(boolean status) {
        getPrefs()
                .writeBoolean(
                        ChromePreferenceKeys.EXTENSIONS_BOOKMARKS_URL_OVERRIDE_ENABLED, status);
    }

    private SharedPreferencesManager getPrefs() {
        return ChromeSharedPreferences.getInstance();
    }
}
