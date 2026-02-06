// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Used to store information on whether enterprise policies have overridden URLs. */
@NullMarked
public class PolicyUrlOverrideRegistry {

    // Prevent instantiation.
    private PolicyUrlOverrideRegistry() {}

    /**
     * Returns true if the New Tab Page should be set to according to the NewTabPageLocation policy.
     */
    public static boolean getNewTabPageLocationOverrideEnabled() {
        return getPrefs().readBoolean(getKey(), false);
    }

    /**
     * Sets whether the New Tab Page location is overridden by the NewTabPageLocation policy.
     *
     * @param isOverridden Whether the location is overridden by policy.
     */
    public static void setIsNewTabPageLocationOverriddenByPolicy(boolean isOverridden) {
        getPrefs().writeBoolean(getKey(), isOverridden);
    }

    private static SharedPreferencesManager getPrefs() {
        return ChromeSharedPreferences.getInstance();
    }

    private static String getKey() {
        return ChromePreferenceKeys.NTP_LOCATION_POLICY_ENABLED;
    }

    /** Disables all overrides. */
    public static void resetRegistry() {
        setIsNewTabPageLocationOverriddenByPolicy(false);
    }
}
