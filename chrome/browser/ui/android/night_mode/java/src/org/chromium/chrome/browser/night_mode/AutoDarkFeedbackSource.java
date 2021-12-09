// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feedback.FeedbackSource;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeController.AutoDarkModeEnabledState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/** Feedback Source around relevant user and UI settings for auto dark mode. */
public class AutoDarkFeedbackSource implements FeedbackSource {
    @VisibleForTesting
    static final String AUTO_DARK_FEEDBACK_KEY = "auto_dark_web_content_enabled";
    @VisibleForTesting
    static final String ENABLED_VALUE = "Enabled";

    /** Feature flag for auto dark is disabled. */
    @VisibleForTesting
    static final String DISABLED_FEATURE_VALUE = "DisabledFeatureGroup";

    /** User settings for auto dark is disabled. */
    @VisibleForTesting
    static final String DISABLED_GLOBAL_SETTINGS_VALUE = "DisabledGlobalSettings";

    /** Whether the current URL has auto dark enabled. */
    @VisibleForTesting
    static final String DISABLED_URL_SETTINGS_VALUE = "DisabledUrlSettings";

    /** Settings is enabled, theme is in light */
    @VisibleForTesting
    static final String DISABLED_BY_LIGHT_MODE_VALUE = "DisabledByLightMode";

    private final HashMap<String, String> mMap;

    public AutoDarkFeedbackSource(Profile profile, Context context, GURL url) {
        mMap = new HashMap<String, String>(1);

        // Ignore user settings in incognito, or not in auto dark feature is not enabled.
        if (profile.isOffTheRecord()) return;

        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
            mMap.put(AUTO_DARK_FEEDBACK_KEY, DISABLED_FEATURE_VALUE);
        } else {
            @AutoDarkModeEnabledState
            int enabledState = WebContentsDarkModeController.getEnabledState(profile, context, url);
            mMap.put(AUTO_DARK_FEEDBACK_KEY, toFeedbackValue(enabledState));
        }
    }

    private String toFeedbackValue(@AutoDarkModeEnabledState int enabledState) {
        switch (enabledState) {
            case AutoDarkModeEnabledState.ENABLED:
                return ENABLED_VALUE;
            case AutoDarkModeEnabledState.DISABLED_GLOBAL_SETTINGS:
                return DISABLED_GLOBAL_SETTINGS_VALUE;
            case AutoDarkModeEnabledState.DISABLED_URL_SETTINGS:
                return DISABLED_URL_SETTINGS_VALUE;
            case AutoDarkModeEnabledState.DISABLED_LIGHT_MODE:
                return DISABLED_BY_LIGHT_MODE_VALUE;
            default:
                throw new RuntimeException("Invalid enabled state.");
        }
    }

    @Override
    public Map<String, String> getFeedback() {
        return mMap;
    }
}
