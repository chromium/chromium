// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.Manifest;
import android.content.Intent;
import android.speech.RecognizerIntent;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

/**
 * Utilities related to voice recognition.
 */
public class VoiceRecognitionUtil {
    private static Boolean sHasRecognitionIntentHandler;
    private static Boolean sIsVoiceSearchEnabledForTesting;
    private static MutableFlagWithSafeDefault sVoiceSearchAudioCapturePolicyFlag =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY, false);

    /**
     * Returns whether voice search is enabled.
     *
     * Evaluates voice search eligibility based on
     * - Android permissions (user consent),
     * - Enterprise policies,
     * - Presence of a speech-to-text service in the system.
     *
     * Note: Requires native libraries to be loaded and initialized for proper execution.
     * When called prematurely, certain signals may be unavailable, making the system fall back to
     * best-effort defaults.
     *
     * Note: this check does not perform strict policy checking.
     *
     * @return true if all the conditions permit execution of a voice search.
     */
    public static boolean isVoiceSearchEnabled(
            AndroidPermissionDelegate androidPermissionDelegate) {
        if (sIsVoiceSearchEnabledForTesting != null) {
            return sIsVoiceSearchEnabledForTesting.booleanValue();
        }

        if (androidPermissionDelegate == null) return false;
        if (!androidPermissionDelegate.hasPermission(Manifest.permission.RECORD_AUDIO)
                && !androidPermissionDelegate.canRequestPermission(
                        Manifest.permission.RECORD_AUDIO)) {
            return false;
        }

        if (!isVoiceSearchPermittedByPolicy(/* strictPolicyCheck=*/false)) return false;

        return isRecognitionIntentPresent(true);
    }

    /**
     * Returns whether enterprise policies permit voice search.
     *
     * Note: Requires native libraries to be loaded and initialized for proper execution.
     * When called prematurely, certain signals may be unavailable, making the system fall back to
     * best-effort defaults.
     *
     * @param strictPolicyCheck Whether to fail if the policy verification cannot be performed at
     *         this time. May be set to false by the UI code if there is a possibility that the call
     *         is made early (eg. before native libraries are initialized). Must be set to true
     *         ahead of actual check.
     * @return true if the Enterprise policies permit execution of a voice search.
     */
    public static boolean isVoiceSearchPermittedByPolicy(boolean strictPolicyCheck) {
        if (sVoiceSearchAudioCapturePolicyFlag.isEnabled()) {
            // If the PrefService isn't initialized yet we won't know here whether or not voice
            // search is allowed by policy. In that case, treat voice search as enabled but check
            // again when a Profile is set and PrefService becomes available.
            PrefService prefService = getPrefService();

            // Fail if strict policy checking is requested but we do not have the way to verify.
            if (strictPolicyCheck && prefService == null) return false;

            return prefService == null || prefService.getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        }
        return true;
    }

    /**
     * Set whether voice search is enabled. Should be reset back to null after the test has
     * finished.
     * @param isVoiceSearchEnabled
     */
    @VisibleForTesting
    public static void setIsVoiceSearchEnabledForTesting(@Nullable Boolean isVoiceSearchEnabled) {
        sIsVoiceSearchEnabledForTesting = isVoiceSearchEnabled;
    }

    @VisibleForTesting
    static void setHasRecognitionIntentHandlerForTesting(@Nullable Boolean hasIntentHandler) {
        sHasRecognitionIntentHandler = hasIntentHandler;
    }

    /** Returns the PrefService for the active Profile, or null if no profile has been loaded. */
    private static @Nullable PrefService getPrefService() {
        if (!ProfileManager.isInitialized()) return null;
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    /**
     * Determines whether or not the {@link RecognizerIntent#ACTION_RECOGNIZE_SPEECH} {@link Intent}
     * is handled by any {@link android.app.Activity}s in the system.  The result will be cached for
     * future calls.  Passing {@code false} to {@code useCachedValue} will force it to re-query any
     * {@link android.app.Activity}s that can process the {@link Intent}.
     *
     * @param useCachedValue Whether or not to use the cached value from a previous result.
     * @return {@code true} if recognition is supported.  {@code false} otherwise.
     */
    public static boolean isRecognitionIntentPresent(boolean useCachedValue) {
        ThreadUtils.assertOnUiThread();
        if (sHasRecognitionIntentHandler == null || !useCachedValue) {
            sHasRecognitionIntentHandler = PackageManagerUtils.canResolveActivity(
                    new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH));
        }

        return sHasRecognitionIntentHandler;
    }
}
