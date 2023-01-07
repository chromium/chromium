// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;
import android.media.AudioManager;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Observable;

/**
 * Wrapper for Cast code to use a single AudioManager instance.
 *
 * Encapsulates behavior that differs across SDK levels like muting and audio focus, and manages a
 * singleton instance that ensures that all clients are using the same AudioManager.
 */
public class CastAudioManager {
    private static final String TAG = "CastAudioManager";
    private static CastAudioManager sInstance;

    public static CastAudioManager getAudioManager(Context context) {
        if (sInstance == null) {
            sInstance = new CastAudioManager(
                    (AudioManager) context.getSystemService(Context.AUDIO_SERVICE));
        }
        return sInstance;
    }

    private final AudioManager mInternal;

    @VisibleForTesting
    CastAudioManager(AudioManager audioManager) {
        mInternal = audioManager;
    }

    /**
     * Requests audio focus whenever the given Observable is activated.
     *
     * The audio focus request is abandoned when the given Observable is deactivated.
     *
     * Returns an Observable that is activated whenever the audio focus is lost. The activation data
     * of this Observable indicates the type of audio focus loss.
     *
     * The resulting Observable will be activated with AudioFocus.NORMAL when the focus request is
     * abandoned.
     *
     *     Observable<AudioFocusLoss> focusLoss = castAudioManager.requestFocusWhen(focusRequest);
     *     // Get an Observable of when focus is taken:
     *     Observable<?> gotFocus = Observable.not(focusLoss);
     *     // Get an Observable of when a specific request got focus:
     *     Observable<Both<CastAudioFocusRequest, AudioFocusLoss>> requestLost =
     *             focusRequest.andThen(focusLoss);
     *
     * The given Observable<CastAudioFocusRequest> should deactivate before it is garbage-collected,
     * or else the Observable and anything it references will leak.
     */
    public Observable<AudioFocusLoss> requestAudioFocusWhen(
            Observable<CastAudioFocusRequest> event) {
        Controller<AudioFocusLoss> audioFocusLossState = new Controller<>();
        audioFocusLossState.set(AudioFocusLoss.NOT_REQUESTED);
        event.subscribe(focusRequest -> {
            focusRequest.setAudioFocusChangeListener((int focusChange) -> {
                audioFocusLossState.set(AudioFocusLoss.from(focusChange));
            });
            // Request audio focus when the source event is activated.
            if (focusRequest.request(mInternal) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                audioFocusLossState.reset();
            }
            // Abandon audio focus when the source event is deactivated.
            return () -> {
                if (focusRequest.abandon(mInternal) != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                    Log.e(TAG, "Failed to abandon audio focus");
                }
                audioFocusLossState.set(AudioFocusLoss.NORMAL);
            };
        });
        return audioFocusLossState;
    }

    @VisibleForTesting
    AudioManager getInternal() {
        return mInternal;
    }

    /**
     * Disambiguates different audio focus loss types that can activate the result of
     * requestAudioFocusWhen().
     */
    public enum AudioFocusLoss {
        NORMAL,
        TRANSIENT,
        TRANSIENT_CAN_DUCK,
        NOT_REQUESTED;

        private static @Nullable AudioFocusLoss from(int focusChange) {
            switch (focusChange) {
                case AudioManager.AUDIOFOCUS_LOSS:
                    return NORMAL;
                case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                    return TRANSIENT;
                case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                    return TRANSIENT_CAN_DUCK;
                default:
                    return null;
            }
        }
    }
}
