// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.app.Activity;
import android.content.res.Resources;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A controller class for the messages that will educate the user about the auto-dark web contents
 * feature.
 */
public class WebContentsDarkModeMessageController {
    @VisibleForTesting
    static boolean sIsEnabledForTesting;

    private static boolean isEnabled() {
        // Only send message if the feature is enabled and the message has not yet been shown.
        // TODO(crbug.com/1252868): Add feature engagement check
        boolean featureEnabled = UserPrefs.get(Profile.getLastUsedRegularProfile())
                                         .getBoolean(Pref.WEB_KIT_FORCE_DARK_MODE_ENABLED);
        return featureEnabled && sIsEnabledForTesting;
    }

    /**
     * Checks if the auto-dark feature is enabled. If it is, send a user education message showing
     * an overview and how to disable the feature.
     */
    public static void sendMessageIfAutoDarkEnabled(Activity activity,
            SettingsLauncher settingsLauncher, MessageDispatcher messageDispatcher) {
        if (!isEnabled()) return;

        Resources resources = activity.getResources();

        // Set the properties (icon, text, etc.) for the message.
        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.AUTO_DARK_WEB_CONTENTS)
                        .with(MessageBannerProperties.ICON_RESOURCE_ID,
                                R.drawable.ic_brightness_medium_24dp)
                        .with(MessageBannerProperties.ICON_TINT_COLOR,
                                MessageBannerProperties.TINT_NONE)
                        .with(MessageBannerProperties.TITLE,
                                resources.getString(R.string.auto_dark_message_title))
                        .with(MessageBannerProperties.DESCRIPTION,
                                resources.getString(R.string.auto_dark_message_description))
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.auto_dark_message_button))
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> { onPrimaryAction(activity, settingsLauncher); })
                        .with(MessageBannerProperties.ON_DISMISSED,
                                (dismissReason) -> { onMessageDismissed(dismissReason); })
                        .build();

        // Enqueue the message so that it will appear on-screen.
        messageDispatcher.enqueueWindowScopedMessage(message, false);
    }

    /**
     * The primary action associated with the created message. In this case, the settings page is
     * opened to show users where to change the auto-dark settings.
     */
    @VisibleForTesting
    static void onPrimaryAction(Activity activity, SettingsLauncher settingsLauncher) {
        Bundle args = new Bundle();
        args.putInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                ThemeSettingsEntry.AUTO_DARK_MODE_MESSAGE);
        settingsLauncher.launchSettingsActivity(activity, ThemeSettingsFragment.class, args);
    }

    /**
     * Record that the message was dismissed.
     */
    @VisibleForTesting
    static void onMessageDismissed(@DismissReason int dismissReason) {
        // TODO(crbug.com/1252868): Notify feature engagement system that message was shown
    }
}
