// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;
import android.media.AudioManager;
import android.media.audiopolicy.AudioPolicy;
import android.os.Build;
import android.support.annotation.Nullable;

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
        audioFocusLossState.set(AudioFocusLoss.NORMAL);
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

    // Only called on Lollipop and below, in an Activity's onPause() event.
    // On Lollipop and below, setStreamMute() calls are cumulative and per-application, and if
    // Activities don't unmute the streams that they mute, the stream remains muted to other
    // applications, which are unable to unmute the stream themselves. Therefore, when an Activity
    // is paused, it must unmute any streams it had muted.
    // More context in b/19964892 and b/22204758.
    @SuppressWarnings("deprecation")
    public void releaseStreamMuteIfNecessary(int streamType) {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP) {
            // On L, if we try to unmute a stream that is not muted, a warning Toast appears.
            // Check the stream mute state to determine whether to unmute.
            boolean isMuted = false;
            try {
                // isStreamMute() was only made public in M, but it can be accessed through
                // reflection in L.
                isMuted = (Boolean) mInternal.getClass()
                                  .getMethod("isStreamMute", int.class)
                                  .invoke(mInternal, streamType);
            } catch (Exception e) {
                Log.e(TAG, "Can not call AudioManager.isStreamMute().", e);
            }

            if (isMuted) {
                // Note: this is a no-op on fixed-volume devices.
                mInternal.setStreamMute(streamType, false);
            }
        }
    }

    public int getStreamMaxVolume(int streamType) {
        return mInternal.getStreamMaxVolume(streamType);
    }

    public int registerAudioPolicy(AudioPolicy audioPolicy) {
        return mInternal.registerAudioPolicy(audioPolicy);
    }

    public void unregisterAudioPolicyAsync(AudioPolicy audioPolicy) {
        mInternal.unregisterAudioPolicyAsync(audioPolicy);
    }

    // TODO(sanfin): Do not expose this. All needed AudioManager methods can be adapted with
    // CastAudioManager.
    public AudioManager getInternal() {
        return mInternal;
    }

    /**
     * Disambiguates different audio focus loss types that can activate the result of
     * requestAudioFocusWhen().
     */
    public enum AudioFocusLoss {
        NORMAL,
        TRANSIENT,
        TRANSIENT_CAN_DUCK;

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
