// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feedback.FeedbackSource;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/** Feedback Source around relevant user and UI settings for auto dark mode. */
public class AutoDarkFeedbackSource implements FeedbackSource {
    @VisibleForTesting static final String AUTO_DARK_FEEDBACK_KEY = "auto_dark_web_content_enabled";
    @VisibleForTesting static final String ENABLED_VALUE = "Enabled";

    /** Feature flag for auto dark is disabled. */
    @VisibleForTesting static final String DISABLED_VALUE = "Disabled";

    private final HashMap<String, String> mMap;

    public AutoDarkFeedbackSource(Profile profile, Context context, GURL url) {
        mMap = new HashMap<String, String>(1);

        // Ignore user settings in incognito, or not in auto dark feature is not enabled.
        if (profile.isOffTheRecord()) return;

        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
            mMap.put(AUTO_DARK_FEEDBACK_KEY, DISABLED_VALUE);
        } else {
            boolean enabledState =
                    WebContentsDarkModeController.getEnabledState(profile, context, url);
            mMap.put(AUTO_DARK_FEEDBACK_KEY, enabledState ? ENABLED_VALUE : DISABLED_VALUE);
        }
    }

    @Override
    public Map<String, String> getFeedback() {
        return mMap;
    }
}
