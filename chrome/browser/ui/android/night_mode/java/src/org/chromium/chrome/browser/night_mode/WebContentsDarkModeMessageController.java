// Copyright 2019 The Chromium Authors
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
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.content_public.browser.WebContents;
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
    @VisibleForTesting static final String FEEDBACK_DIALOG_PARAM = "feedback_dialog";
    @VisibleForTesting static final String OPT_OUT_PARAM = "opt_out";

    /**
     * Checks if auto-dark theming is enabled. Also checks if the feature engagement system
     * requirements are met. If both are true, returns true indicating the user education message
     * should be sent. Otherwise return false.
     *
     * @param profile Profile associated with current tab.
     * @param context {@link Context} used to check whether UI is in night mode.
     * @return Whether or not the user education message should be shown.
     */
    private static boolean shouldSendMessage(Profile profile, Context context) {
        // Only send message if the feature is enabled and the message has not yet been shown.
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        boolean featureEnabled = WebContentsDarkModeController.isFeatureEnabled(context, profile);
        boolean optOut =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING,
                        OPT_OUT_PARAM,
                        true);
        if (optOut) {
            return featureEnabled
                    && tracker.shouldTriggerHelpUI(
                            FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE);
        } else {
            return !featureEnabled
                    && tracker.shouldTriggerHelpUI(
                            FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_OPT_IN_FEATURE);
        }
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
     * @param webContents WebContents associated with current tab.
     * @param messageDispatcher Dispatcher for the message we are creating.
     */
    public static void attemptToSendMessage(
            Activity activity,
            Profile profile,
            @Nullable WebContents webContents,
            MessageDispatcher messageDispatcher) {
        if (!shouldSendMessage(profile, activity)) return;

        // Create and send message based on arm.
        if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING,
                OPT_OUT_PARAM,
                true)) {
            sendOptOutMessage(activity, profile, messageDispatcher, null);
        } else {
            sendOptInMessage(activity, profile, webContents, messageDispatcher);
        }
    }

    private static void sendOptOutMessage(
            Activity activity,
            Profile profile,
            MessageDispatcher messageDispatcher,
            @Nullable String description) {
        Resources resources = activity.getResources();
        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.AUTO_DARK_WEB_CONTENTS)
                        .with(
                                MessageBannerProperties.ICON_RESOURCE_ID,
                                R.drawable.ic_brightness_medium_24dp)
                        .with(
                                MessageBannerProperties.ICON_TINT_COLOR,
                                MessageBannerProperties.TINT_NONE)
                        .with(
                                MessageBannerProperties.TITLE,
                                resources.getString(R.string.auto_dark_message_title))
                        .with(MessageBannerProperties.DESCRIPTION, description)
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.auto_dark_message_button))
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    onOptOutPrimaryAction(activity);
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .with(
                                MessageBannerProperties.ON_DISMISSED,
                                (dismissReason) -> {
                                    onOptOutMessageDismissed(profile);
                                })
                        .build();
        messageDispatcher.enqueueWindowScopedMessage(message, false);
    }

    private static void sendOptInMessage(
            Activity activity,
            Profile profile,
            WebContents webContents,
            MessageDispatcher messageDispatcher) {
        Resources resources = activity.getResources();
        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.AUTO_DARK_WEB_CONTENTS)
                        .with(
                                MessageBannerProperties.ICON_RESOURCE_ID,
                                R.drawable.ic_brightness_medium_24dp)
                        .with(
                                MessageBannerProperties.ICON_TINT_COLOR,
                                MessageBannerProperties.TINT_NONE)
                        .with(
                                MessageBannerProperties.TITLE,
                                resources.getString(R.string.auto_dark_message_opt_in_title))
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                resources.getString(R.string.auto_dark_message_opt_in_body))
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.auto_dark_message_opt_in_button))
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    onOptInPrimaryAction(profile, webContents);
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .with(
                                MessageBannerProperties.ON_DISMISSED,
                                (dismissReason) -> {
                                    onOptInMessageDismissed(
                                            activity, profile, messageDispatcher, dismissReason);
                                })
                        .build();
        messageDispatcher.enqueueWindowScopedMessage(message, false);
    }

    /**
     * The primary action associated with the created message for the opt-out arm. In this case, the
     * settings page is opened to show users where to change the auto-dark settings.
     */
    private static void onOptOutPrimaryAction(Activity activity) {
        Bundle args = new Bundle();
        args.putInt(
                ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                ThemeSettingsEntry.AUTO_DARK_MODE_MESSAGE);
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(activity, ThemeSettingsFragment.class, args);
    }

    /**
     * The primary action associated with the created message for the opt-in arm. In this case, the
     * global setting is enabled.
     */
    private static void onOptInPrimaryAction(Profile profile, WebContents webContents) {
        WebContentsDarkModeController.setGlobalUserSettings(profile, true);
        if (webContents != null) {
            webContents.notifyRendererPreferenceUpdate();
        }
    }

    /** Record that the opt-out message was dismissed. */
    private static void onOptOutMessageDismissed(Profile profile) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.dismissed(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE);
    }

    /**
     * Record that the opt-in message was dismissed. If the CTA was pressed, show the opt-out
     * message.
     */
    private static void onOptInMessageDismissed(
            Activity activity,
            Profile profile,
            MessageDispatcher messageDispatcher,
            @DismissReason int dismissReason) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.dismissed(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_OPT_IN_FEATURE);

        if (dismissReason == DismissReason.PRIMARY_ACTION) {
            sendOptOutMessage(
                    activity,
                    profile,
                    messageDispatcher,
                    activity.getResources().getString(R.string.auto_dark_message_opt_in_body));
        }
    }

    // User feedback dialog implementation.

    /**
     * Record in the feature engagement system when a site is blocked. If the feature has been
     * disabled enough times (determined by the feature engagement system), show dialog informing
     * user how to disable the feature globally and how to give feedback.
     *
     * @param activity The activity from which to launch theme settings.
     * @param profile The current profile.
     * @param url The url the user is currently on.
     * @param modalDialogManager Manager that triggers the dialog.
     */
    public static void attemptToShowDialog(
            Activity activity, Profile profile, String url, ModalDialogManager modalDialogManager) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.AUTO_DARK_DISABLED_IN_APP_MENU);
        if (!tracker.shouldTriggerHelpUI(FeatureConstants.AUTO_DARK_OPT_OUT_FEATURE)) return;

        // Set values and click action based on whether or not the feedback flow is enabled.
        Resources resources = activity.getResources();
        boolean feedbackDialogEnabled =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING,
                        FEEDBACK_DIALOG_PARAM,
                        false);
        int titleId =
                feedbackDialogEnabled
                        ? R.string.auto_dark_dialog_title
                        : R.string.auto_dark_dialog_no_feedback_title;
        CharSequence message =
                feedbackDialogEnabled
                        ? getFormattedMessageText(activity)
                        : resources.getString(R.string.auto_dark_dialog_no_feedback_message);
        int positiveButtonId =
                feedbackDialogEnabled
                        ? R.string.auto_dark_dialog_positive_button
                        : R.string.auto_dark_dialog_no_feedback_positive_button;
        Controller controller =
                new Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        // TODO(crbug.com/40200588): Set clickable to false for title icon.
                        if (buttonType == ButtonType.TITLE_ICON) return;
                        if (buttonType == ButtonType.POSITIVE) {
                            if (feedbackDialogEnabled) {
                                showFeedback(activity, profile, url);
                            } else {
                                openSettings(activity);
                            }
                        }

                        modalDialogManager.dismissDialog(
                                model,
                                buttonType == ButtonType.POSITIVE
                                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        tracker.dismissed(FeatureConstants.AUTO_DARK_OPT_OUT_FEATURE);
                    }
                };

        // Set the properties (icon, text, etc.) for the dialog.
        PropertyModel dialog =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.TITLE, resources, titleId)
                        .with(
                                ModalDialogProperties.TITLE_ICON,
                                AppCompatResources.getDrawable(
                                        activity, R.drawable.ic_brightness_medium_24dp))
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, message)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                positiveButtonId)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .build();

        modalDialogManager.showDialog(dialog, ModalDialogType.TAB);
    }

    /** Show feedback. */
    private static void showFeedback(Activity activity, Profile profile, String url) {
        // TODO(crbug.com/40201746): Import ScreenshotMode instead of hardcoding value once new
        // build
        //  target added.
        HelpAndFeedbackLauncherFactory.getForProfile(profile)
                .showFeedback(activity, url, null, /* ScreenshotMode.DEFAULT */ 0, null);
    }

    /** Open settings */
    private static void openSettings(Context context) {
        Bundle args = new Bundle();
        args.putInt(
                ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                ThemeSettingsEntry.AUTO_DARK_MODE_DIALOG);
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(context, ThemeSettingsFragment.class, args);
    }

    /** Returns link-formatted message text for the auto dark dialog. */
    private static CharSequence getFormattedMessageText(Context context) {
        Resources resources = context.getResources();
        String messageText = resources.getString(R.string.auto_dark_dialog_message);
        return SpanApplier.applySpans(
                messageText, new SpanInfo("<link>", "</link>", new AutoDarkClickableSpan(context)));
    }

    @VisibleForTesting
    static class AutoDarkClickableSpan extends ClickableSpan {
        private Context mContext;

        AutoDarkClickableSpan(Context context) {
            mContext = context;
        }

        @Override
        public void onClick(@NonNull View view) {
            openSettings(mContext);
        }
    }
}
