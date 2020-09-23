// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.app.Activity;

/**
 * An interface that allows the Site Settings UI to link to and open embedder-specific help pages.
 */
public interface SiteSettingsHelpClient {
    /**
     * @return true if Help and Feedback links and menu items should be shown to the user.
     */
    boolean isHelpAndFeedbackEnabled();

    /**
     * Launches a support page relevant to settings UI pages.
     *
     * @see org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher#show
     */
    void launchSettingsHelpAndFeedbackActivity(Activity currentActivity);

    /**
     * Launches a support page related to protected content.
     *
     * @see org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher#show
     */
    void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity);
}
