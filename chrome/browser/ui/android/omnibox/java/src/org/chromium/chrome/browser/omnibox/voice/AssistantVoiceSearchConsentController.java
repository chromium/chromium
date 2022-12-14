// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Facilitates the consent UI shown to users when they initiate Assistant voice search for the first
 * time.
 */
class AssistantVoiceSearchConsentController
        implements WindowAndroid.ActivityStateObserver, AssistantVoiceSearchConsentUi.Observer {
    @VisibleForTesting
    static final String CONSENT_OUTCOME_HISTOGRAM = "Assistant.VoiceSearch.ConsentOutcome";

    /**
     * Show the consent UI to the user.
     * @param windowAndroid The current {@link WindowAndroid} for the app.
     * @param sharedPreferencesManager The {@link SharedPreferencesManager} to read/write prefs.
     * @param launchAssistantSettingsAction Runnable launching settings activity.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet
     *                              UI. This can be null when starting the consent flow from
     *                              SearchActivity.
     * @param completionCallback A callback to be invoked if the user is continuing with the
     *                           requested voice search.
     */
    static void show(@NonNull WindowAndroid windowAndroid,
            @NonNull SharedPreferencesManager sharedPreferencesManager,
            @NonNull Runnable launchAssistantSettingsAction,
            @Nullable BottomSheetController bottomSheetController,
            @NonNull Callback<Boolean> completionCallback) {
        AssistantVoiceSearchConsentUi consentUi;
        assert (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH));

        // When attempting voice search through the search widget, the bottom sheet isn't
        // available. When this happens, bail out of the consent flow and fallback to system-ui.
        // Consent will be retried the next time.
        if (bottomSheetController == null) {
            PostTask.postTask(TaskTraits.USER_VISIBLE,
                    () -> { completionCallback.onResult(/* useAssistant= */ false); });
            return;
        }

        // Use the bottom sheet consent by default.
        consentUi = new AssistantVoiceSearchConsentBottomSheet(
                windowAndroid.getContext().get(), bottomSheetController);

        AssistantVoiceSearchConsentController consent =
                new AssistantVoiceSearchConsentController(windowAndroid, sharedPreferencesManager,
                        completionCallback, launchAssistantSettingsAction, consentUi);
        consent.show();
    }

    // AssistantConsentOutcome defined in tools/metrics/histograms/enums.xml. Do not reorder or
    // remove items, only add new items before HISTOGRAM_BOUNDARY.
    @IntDef({ConsentOutcome.ACCEPTED_VIA_UI, ConsentOutcome.ACCEPTED_VIA_SETTINGS,
            ConsentOutcome.REJECTED_VIA_UI, ConsentOutcome.REJECTED_VIA_SETTINGS,
            ConsentOutcome.REJECTED_VIA_DISMISS, ConsentOutcome.CANCELED_VIA_UI,
            ConsentOutcome.MAX_VALUE})
    @Retention(RetentionPolicy.SOURCE)
    @interface ConsentOutcome {
        int ACCEPTED_VIA_UI = 0;
        int ACCEPTED_VIA_SETTINGS = 1;
        int REJECTED_VIA_UI = 2;
        int REJECTED_VIA_SETTINGS = 3;
        int REJECTED_VIA_DISMISS = 4;
        // Deprecated. Individual ConsentUi implementations should track these if needed.
        // int REJECTED_VIA_BACK_BUTTON_PRESS = 5;
        // int REJECTED_VIA_SCRIM_TAP = 6;
        // int CANCELED_VIA_BACK_BUTTON_PRESS = 7;
        // int CANCELED_VIA_SCRIM_TAP = 8;
        int CANCELED_VIA_UI = 9;
        int NON_USER_CANCEL = 10;
        // STOP: When updating this, also update values in enums.xml.
        int MAX_VALUE = 11;
    }

    private final WindowAndroid mWindowAndroid;
    private final SharedPreferencesManager mSharedPreferencesManager;
    private @Nullable Callback<Boolean> mCompletionCallback;
    private final Runnable mLaunchAssistantSettingsAction;
    private AssistantVoiceSearchConsentUi mConsentUi;

    @VisibleForTesting
    AssistantVoiceSearchConsentController(@NonNull WindowAndroid windowAndroid,
            @NonNull SharedPreferencesManager sharedPreferencesManager,
            @NonNull Callback<Boolean> completionCallback,
            @NonNull Runnable launchAssistantSettingsAction,
            @NonNull AssistantVoiceSearchConsentUi consentUi) {
        mWindowAndroid = windowAndroid;
        mWindowAndroid.addActivityStateObserver(this);
        mSharedPreferencesManager = sharedPreferencesManager;
        mCompletionCallback = completionCallback;
        mLaunchAssistantSettingsAction = launchAssistantSettingsAction;
        mConsentUi = consentUi;
    }

    /**
     * Show the dialog.
     */
    @VisibleForTesting
    void show() {
        mConsentUi.show(this);
    }

    /**
     * Runs the callback to signal the result from the Consent UI interaction. This should be called
     * exactly once per show().
     * @param startAssistant Whether Assistant should be shown or, if false, the system experience
     *                       should be used instead.
     */
    private void onResult(boolean startAssistant) {
        mWindowAndroid.removeActivityStateObserver(this);
        mCompletionCallback.onResult(startAssistant);
        mCompletionCallback = null;
    }

    private void onConsentAccepted(@ConsentOutcome int consentOutcome) {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED, true);
        RecordHistogram.recordEnumeratedHistogram(
                CONSENT_OUTCOME_HISTOGRAM, consentOutcome, ConsentOutcome.MAX_VALUE);
        onResult(true);
    }

    private void onConsentRejected(@ConsentOutcome int consentOutcome) {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED, false);
        RecordHistogram.recordEnumeratedHistogram(
                CONSENT_OUTCOME_HISTOGRAM, consentOutcome, ConsentOutcome.MAX_VALUE);
        onResult(false);
    }

    // AssistantVoiceSearchConsentUi.Observer implementation.

    @Override
    public void onConsentAccepted() {
        onConsentAccepted(ConsentOutcome.ACCEPTED_VIA_UI);
    }

    @Override
    public void onConsentRejected() {
        onConsentRejected(ConsentOutcome.REJECTED_VIA_UI);
    }

    @Override
    public void onConsentCanceled() {
        // Treat cancels as rejections by default.
        boolean maxReached = true;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ASSISTANT_CONSENT_V2)) {
            int repromptsCount = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.ASSISTANT_CONSENT_V2, "count", -1);
            if (repromptsCount < 0) {
                // If the reprompt count is not specified in the config it means there is no limit.
                maxReached = false;
            } else {
                // Otherwise we increment the counter.
                int counter = mSharedPreferencesManager.readInt(
                        ChromePreferenceKeys.ASSISTANT_VOICE_CONSENT_OUTSIDE_TAPS, 0);
                counter++;
                maxReached = counter > repromptsCount;
                mSharedPreferencesManager.writeInt(
                        ChromePreferenceKeys.ASSISTANT_VOICE_CONSENT_OUTSIDE_TAPS, counter);
            }
        }

        if (maxReached) {
            onConsentRejected(ConsentOutcome.REJECTED_VIA_DISMISS);
        } else {
            RecordHistogram.recordEnumeratedHistogram(CONSENT_OUTCOME_HISTOGRAM,
                    ConsentOutcome.CANCELED_VIA_UI, ConsentOutcome.MAX_VALUE);
            onResult(false);
        }
    }

    @Override
    public void onNonUserCancel() {
        RecordHistogram.recordEnumeratedHistogram(CONSENT_OUTCOME_HISTOGRAM,
                ConsentOutcome.NON_USER_CANCEL, ConsentOutcome.MAX_VALUE);
        onResult(false);
    }

    @Override
    public void onLearnMoreClicked() {
        mLaunchAssistantSettingsAction.run();
    }

    // WindowAndroid.ActivityStateObserver implementation.

    @Override
    public void onActivityResumed() {
        // It's possible the user clicked through "learn more" and enabled/disabled it via settings.
        if (!mSharedPreferencesManager.contains(
                    ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED)) {
            return;
        }
        mConsentUi.dismiss();
        if (mSharedPreferencesManager.readBoolean(
                    ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false)) {
            onConsentAccepted(ConsentOutcome.ACCEPTED_VIA_SETTINGS);
        } else {
            onConsentRejected(ConsentOutcome.REJECTED_VIA_SETTINGS);
        }
    }

    @Override
    public void onActivityPaused() {}

    @Override
    public void onActivityDestroyed() {}
}
