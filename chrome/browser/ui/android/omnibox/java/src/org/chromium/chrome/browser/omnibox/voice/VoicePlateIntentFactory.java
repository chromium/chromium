// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.content.Context;
import android.content.Intent;
import android.speech.RecognizerIntent;

import androidx.annotation.IntDef;

import org.chromium.base.PackageManagerUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.gsa.GSAUtils;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionIntentHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Decides whether voice recognition should be delegated to the Android Google App (AGA) Voice
 * Plate, and builds the explicit intent that launches it.
 *
 * <p>The Voice Plate activity has no intent filter, so it can only be launched by an explicit
 * component. It accepts only an allowlisted set of extras and strips everything else. It returns
 * the transcription using the standard {@link RecognizerIntent#EXTRA_RESULTS} contract and does not
 * navigate on its own.
 *
 * <p>This class only decides eligibility and builds the intent. Permission and enterprise policy
 * checks stay with {@link VoiceRecognitionIntentHandler}.
 */
@NullMarked
class VoicePlateIntentFactory {
    static final String VOICE_PLATE_CLASS_NAME =
            "com.google.android.apps.search.googleapp.search.voicesearch.intent.VoicePlateActivity";

    private static final String AGA_EXTRA_PREFIX = "com.google.android.googlequicksearchbox.extra.";

    /** Email of the account signed in to Chrome; AGA verifies it against its primary account. */
    static final String EXTRA_ACCOUNT_NAME = AGA_EXTRA_PREFIX + "ACCOUNT_NAME";

    /** Signals that Chrome is signed out; AGA prompts the user if AGA itself is signed in. */
    static final String EXTRA_SIGNED_OUT = AGA_EXTRA_PREFIX + "SIGNED_OUT";

    /** Overrides the Voice Plate color scheme, see {@link VoicePlateColorScheme}. */
    static final String EXTRA_COLOR_SCHEME = AGA_EXTRA_PREFIX + "COLOR_SCHEME";

    /** Hides the Voice Plate Stop button, which would otherwise return a partial transcription. */
    static final String EXTRA_DISABLE_STOP_BUTTON = AGA_EXTRA_PREFIX + "DISABLE_STOP_BUTTON";

    /** Values accepted by {@link #EXTRA_COLOR_SCHEME}, defined by the AGA Voice Plate contract. */
    @IntDef({VoicePlateColorScheme.LIGHT, VoicePlateColorScheme.DARK})
    @Retention(RetentionPolicy.SOURCE)
    @interface VoicePlateColorScheme {
        int LIGHT = 1;
        int DARK = 2;
    }

    /**
     * Returns the intent that delegates voice recognition to the AGA Voice Plate, or null if the
     * request is not eligible and the system speech recognizer should be used instead.
     *
     * @param context The context used to resolve the current color scheme.
     * @param profile The profile that initiated the voice recognition.
     * @param source The voice recognition entry point.
     */
    @Nullable Intent createIntent(
            Context context, @Nullable Profile profile, @VoiceInteractionSource int source) {
        if (profile == null || !isEligible(profile, source)) return null;

        Intent intent =
                new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)
                        .setClassName(GSAUtils.GSA_PACKAGE_NAME, VOICE_PLATE_CLASS_NAME);
        // Resolve the bare intent first: it is the cheapest check, and it fails on devices where
        // AGA is absent, disabled, or too old to provide the Voice Plate.
        if (!PackageManagerUtils.canResolveActivity(intent)) return null;
        if (!GSAUtils.isValidAgsaPackage()) return null;

        populateExtras(intent, context, getAccountName(profile));
        return intent;
    }

    /**
     * Returns whether the voice recognition request may be delegated to the AGA Voice Plate.
     *
     * @param profile The profile that initiated the voice recognition.
     * @param source The voice recognition entry point.
     */
    private static boolean isEligible(Profile profile, @VoiceInteractionSource int source) {
        if (!OmniboxFeatures.sOmniboxVoicePlateIntent.isEnabled()) return false;
        if (!isSupportedSource(source)) return false;
        // AGA cannot honor Chrome's Incognito state; use the in-Chrome recognizer instead.
        if (profile.isOffTheRecord()) return false;
        return TemplateUrlServiceFactory.getForProfile(profile).isDefaultSearchEngineGoogle();
    }

    /**
     * Returns whether the voice recognition entry point may use the AGA Voice Plate.
     *
     * <p>Supported entry points are the mic buttons sharing the Omnibox voice recognition flow: the
     * Omnibox, the New Tab Page fakebox, and the toolbar.
     *
     * @param source The voice recognition entry point.
     */
    private static boolean isSupportedSource(@VoiceInteractionSource int source) {
        return source == VoiceInteractionSource.OMNIBOX
                || source == VoiceInteractionSource.NTP
                || source == VoiceInteractionSource.TOOLBAR;
    }

    /**
     * Returns the email of the account signed in to Chrome, or null if Chrome is signed out.
     *
     * @param profile The regular (non-Incognito) profile that initiated the voice recognition.
     */
    private static @Nullable String getAccountName(Profile profile) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (identityManager == null) return null;
        return AccountInfo.getEmailFrom(identityManager.getPrimaryAccountInfo());
    }

    /**
     * Adds the Voice Plate extras to the explicit AGA intent.
     *
     * <p>AGA always runs voice search under its own primary account. The Chrome account (or the
     * signed-out hint) only lets AGA detect a mismatch on-device and ask the user for consent
     * before opening the microphone; it is not sent to any server.
     *
     * @param intent The explicit AGA Voice Plate intent to populate.
     * @param context The context used to resolve the current color scheme.
     * @param accountName The email of the account signed in to Chrome, or null if signed out.
     */
    private static void populateExtras(
            Intent intent, Context context, @Nullable String accountName) {
        intent.putExtra(EXTRA_DISABLE_STOP_BUTTON, /* value= */ true);
        intent.putExtra(
                EXTRA_COLOR_SCHEME,
                ColorUtils.inNightMode(context)
                        ? VoicePlateColorScheme.DARK
                        : VoicePlateColorScheme.LIGHT);
        if (accountName != null) {
            intent.putExtra(EXTRA_ACCOUNT_NAME, accountName);
        } else {
            intent.putExtra(EXTRA_SIGNED_OUT, /* value= */ true);
        }
    }
}
