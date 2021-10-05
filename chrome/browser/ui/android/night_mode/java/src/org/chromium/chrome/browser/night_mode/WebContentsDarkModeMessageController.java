// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.app.Activity;
import android.content.res.Resources;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
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
    /**
     * Checks if auto-dark theming is enabled. Also checks if the feature engagement system
     * requirements are met. If both are true, returns true indicating the user education message
     * should be sent. Otherwise return false.
     *
     * @param profile Profile associated with current tab.
     * @return Whether or not the user education message should be shown.
     */
    private static boolean shouldSendMessage(Profile profile) {
        // Only send message if the feature is enabled and the message has not yet been shown.
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        boolean featureEnabled =
                UserPrefs.get(profile).getBoolean(Pref.WEB_KIT_FORCE_DARK_MODE_ENABLED);
        return featureEnabled
                && tracker.shouldTriggerHelpUI(
                        FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE);
    }

    /**
     * Marks in the feature engagement system that the ThemeSettings were opened while auto dark
     * was enabled.
     *
     * @param profile Profile to get tracker for feature engagement system from.
     */
    public static void notifyEventSettingsOpened(Profile profile) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.AUTO_DARK_SETTINGS_OPENED);
    }

    /**
     * Checks if the auto-dark theming is enabled and the feature engagement system requirements are
     * met. If they are, send a user education message showing an overview and how to disable the
     * feature.
     *
     * @param activity Activity for resources and to launch SettingsActivity from.
     * @param profile Profile associated with current tab.
     * @param settingsLauncher Launcher into theme settings.
     * @param messageDispatcher Dispatcher for the message we are creating.
     */
    public static void attemptToSendMessage(Activity activity, Profile profile,
            SettingsLauncher settingsLauncher, MessageDispatcher messageDispatcher) {
        if (!shouldSendMessage(profile)) return;

        // Set the properties (icon, text, etc.) for the message.
        Resources resources = activity.getResources();
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
                                (dismissReason) -> { onMessageDismissed(profile, dismissReason); })
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
    static void onMessageDismissed(Profile profile, @DismissReason int dismissReason) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.dismissed(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE);
    }
}
