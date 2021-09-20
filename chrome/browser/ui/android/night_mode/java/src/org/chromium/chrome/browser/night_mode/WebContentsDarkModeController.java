// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * A controller class could enable or disable web content dark mode feature based on the night mode
 * and the user preference.
 *
 * TODO(https://crbug.com/1249345): Rework and try removing Pref.WEB_KIT_FORCE_DARK_MODE_ENABLED.
 */
public class WebContentsDarkModeController implements ApplicationStateListener {
    private NightModeStateProvider.Observer mNightModeObserver;
    private static WebContentsDarkModeController sController;

    private WebContentsDarkModeController() {
        final int applicationState = ApplicationStatus.getStateForApplication();
        if (applicationState == ApplicationState.HAS_RUNNING_ACTIVITIES
                || applicationState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            start();
        } else {
            // For other application state, set the correct state based on current settings.
            enableWebContentsDarkMode(shouldEnableWebContentsDarkMode());
        }
        ApplicationStatus.registerApplicationStateListener(this);
    }

    /**
     * @return The instance can enable or disable the feature. Call the start method to listen
     * the user setting and app night mode change so that the instance can automatically apply the
     * change. Call the stop method to stop the listening.
     */
    public static WebContentsDarkModeController createInstance() {
        if (sController == null) {
            sController = new WebContentsDarkModeController();
        }
        return sController;
    }

    /**
     * Enable or disable the global user settings for auto dark mode. If the global settings is
     * enabled, the web contents will be darkened by default if Chrome is in dark mode.
     * @param enabled The new global setting state of the web content auto dark mode.
     */
    public static void setGlobalUserSettings(boolean enabled) {
        WebsitePreferenceBridge.setContentSettingEnabled(Profile.getLastUsedRegularProfile(),
                ContentSettingsType.AUTO_DARK_WEB_CONTENT, enabled);
        enableWebContentsDarkMode(shouldEnableWebContentsDarkMode());
    }

    /** Return whether web content dark mode is enabled by settings. */
    public static boolean isGlobalUserSettingsEnabled() {
        return WebsitePreferenceBridge.isContentSettingEnabled(
                Profile.getLastUsedRegularProfile(), ContentSettingsType.AUTO_DARK_WEB_CONTENT);
    }

    private static boolean shouldEnableWebContentsDarkMode() {
        return GlobalNightModeStateProviderHolder.getInstance().isInNightMode()
                && isGlobalUserSettingsEnabled();
    }

    private static void enableWebContentsDarkMode(boolean enabled) {
        UserPrefs.get(Profile.getLastUsedRegularProfile())
                .setBoolean(Pref.WEB_KIT_FORCE_DARK_MODE_ENABLED, enabled);
    }

    /**
     * start listening to any event can enable or disable web content dark mode
     */
    private void start() {
        if (mNightModeObserver != null) return;
        mNightModeObserver = () -> enableWebContentsDarkMode(shouldEnableWebContentsDarkMode());

        enableWebContentsDarkMode(shouldEnableWebContentsDarkMode());
        GlobalNightModeStateProviderHolder.getInstance().addObserver(mNightModeObserver);
    }

    /**
     * stop listening to any event can enable or disable web content dark mode
     */
    private void stop() {
        if (mNightModeObserver == null) return;
        GlobalNightModeStateProviderHolder.getInstance().removeObserver(mNightModeObserver);
        mNightModeObserver = null;
    }

    @Override
    public void onApplicationStateChange(int newState) {
        // TODO(https://crbug.com/1249503): Listen to foreground top-level activities only.
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            start();
        } else if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
            stop();
        }
    }

    @VisibleForTesting
    public static void setTestInstance(WebContentsDarkModeController testInstance) {
        sController = testInstance;
    }
}
