// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.content.Context;
import android.content.Intent;
import android.speech.RecognizerIntent;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Integration with the Google app Voice Plate, which performs voice capture and transcription on
 * behalf of Chrome.
 *
 * <p>The base implementation never supplies an intent, so voice recognition falls back to the
 * system speech recognizer. A downstream implementation in //clank provides the Google app specific
 * eligibility checks and intent.
 *
 * <p>The integration only supplies the intent. Chrome remains responsible for enterprise policy and
 * permission checks, launching the intent, parsing the {@link RecognizerIntent#EXTRA_RESULTS}
 * result, metrics, and falling back to the system speech recognizer.
 */
@NullMarked
public class VoicePlateIntegration {
    private static @Nullable VoicePlateIntegration sInstance;

    /** Returns the singleton instance of {@link VoicePlateIntegration}. */
    public static VoicePlateIntegration getInstance() {
        if (sInstance == null) {
            VoicePlateIntegration downstream =
                    ServiceLoaderUtil.maybeCreate(VoicePlateIntegration.class);
            sInstance = downstream != null ? downstream : new VoicePlateIntegration();
        }
        return sInstance;
    }

    /**
     * Returns the intent that launches the Voice Plate, or null if the request cannot be fulfilled,
     * in which case the system speech recognizer is used instead. Overridden by the downstream
     * implementation in //clank.
     *
     * <p>The launched activity must return its transcription using the {@link
     * RecognizerIntent#EXTRA_RESULTS} contract. Chrome treats every returned result as high
     * confidence.
     *
     * @param context The context used to inspect the device and current configuration.
     * @param profile The regular (non-Incognito) profile that initiated the voice recognition.
     */
    public @Nullable Intent createIntent(Context context, Profile profile) {
        return null;
    }

    /** Sets the instance for testing. */
    public static void setInstanceForTesting(@Nullable VoicePlateIntegration instance) {
        var previous = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = previous);
    }
}
