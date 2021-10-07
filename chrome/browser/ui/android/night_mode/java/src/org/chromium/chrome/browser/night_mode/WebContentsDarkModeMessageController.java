// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Bundle;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

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
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

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

    // User education message implementation.

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

    // User feedback dialog implementation.

    /**
     * Record in the feature engagement system when a site is blocked. If the feature has been
     * disabled enough times (determined by the feature engagement system), show dialog informing
     * user how to disable the feature globally and how to give feedback.
     *
     * @param context The context from which to launch theme settings.
     * @param modalDialogManager Manager that triggers the dialog.
     * @param settingsLauncher Launcher for theme settings.
     */
    public static void attemptToShowDialog(Context context, Profile profile,
            ModalDialogManager modalDialogManager, SettingsLauncher settingsLauncher) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.AUTO_DARK_DISABLED_IN_APP_MENU);
        if (!tracker.shouldTriggerHelpUI(FeatureConstants.AUTO_DARK_OPT_OUT_FEATURE)) return;

        // Set the properties (icon, text, etc.) for the dialog.
        Resources resources = context.getResources();
        Controller controller = new Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                // TODO(crbug.com/1257260): Set clickable to false for title icon.
                if (buttonType == ButtonType.TITLE_ICON) return;
                if (buttonType == ButtonType.POSITIVE) {
                    // TODO(1255301): Implement feedback logic
                }

                modalDialogManager.dismissDialog(model,
                        buttonType == ButtonType.POSITIVE
                                ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                tracker.dismissed(FeatureConstants.AUTO_DARK_OPT_OUT_FEATURE);
            }
        };
        PropertyModel dialog = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                       .with(ModalDialogProperties.CONTROLLER, controller)
                                       .with(ModalDialogProperties.TITLE, resources,
                                               R.string.auto_dark_dialog_title)
                                       .with(ModalDialogProperties.TITLE_ICON,
                                               AppCompatResources.getDrawable(context,
                                                       R.drawable.ic_brightness_medium_24dp))
                                       .with(ModalDialogProperties.MESSAGE,
                                               getFormattedMessageText(context, settingsLauncher))
                                       .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                               R.string.auto_dark_dialog_positive_button)
                                       .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                               R.string.cancel)
                                       .build();

        modalDialogManager.showDialog(dialog, ModalDialogType.TAB);
    }

    /**
     * Returns link-formatted message text for the auto dark dialog.
     */
    private static CharSequence getFormattedMessageText(
            Context context, SettingsLauncher settingsLauncher) {
        Resources resources = context.getResources();
        String messageText = resources.getString(R.string.auto_dark_dialog_message);
        return SpanApplier.applySpans(messageText,
                new SpanInfo(
                        "<link>", "</link>", new AutoDarkClickableSpan(context, settingsLauncher)));
    }

    @VisibleForTesting
    static class AutoDarkClickableSpan extends ClickableSpan {
        private Context mContext;
        private SettingsLauncher mSettingsLauncher;

        AutoDarkClickableSpan(Context context, SettingsLauncher settingsLauncher) {
            mContext = context;
            mSettingsLauncher = settingsLauncher;
        }

        @Override
        public void onClick(@NonNull View view) {
            Bundle args = new Bundle();
            args.putInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                    ThemeSettingsEntry.AUTO_DARK_MODE_DIALOG);
            mSettingsLauncher.launchSettingsActivity(mContext, ThemeSettingsFragment.class, args);
        }
    }
}
