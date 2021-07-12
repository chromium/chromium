// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;

import androidx.annotation.NonNull;

import org.chromium.base.Log;

import java.util.HashSet;
import java.util.Set;

/**
 * Wrapper for Cast code to pass parameter to AudioFocus methods.
 * This maintains backwards compatibility with old APIs - requestAudioFocus() and
 * abandonAudioFocus()
 */
public class CastAudioFocusRequest {
    private static final String TAG = "CastAudioFocus";
    private AudioFocusRequest mAudioFocusRequest;
    private AudioAttributes mAudioAttributes;
    private int mFocusGain;
    private AudioManager.OnAudioFocusChangeListener mAudioFocusChangeListener;

    CastAudioFocusRequest(AudioFocusRequest audioFocusRequest) {
        mAudioFocusRequest = audioFocusRequest;
    }

    CastAudioFocusRequest(AudioAttributes audioAttributes, int focusGain,
            AudioManager.OnAudioFocusChangeListener l) {
        mAudioAttributes = audioAttributes;
        mFocusGain = focusGain;
        mAudioFocusChangeListener = l;
    }

    AudioFocusRequest getAudioFocusRequest() {
        return mAudioFocusRequest;
    }

    private int getStreamType() {
        if (mAudioAttributes != null) {
            switch (mAudioAttributes.getContentType()) {
                case AudioAttributes.CONTENT_TYPE_MOVIE:
                case AudioAttributes.CONTENT_TYPE_MUSIC:
                    return AudioManager.STREAM_MUSIC;
                case AudioAttributes.CONTENT_TYPE_SONIFICATION:
                    return AudioManager.STREAM_ALARM;
                case AudioAttributes.CONTENT_TYPE_SPEECH:
                    return AudioManager.STREAM_VOICE_CALL;
                case AudioAttributes.CONTENT_TYPE_UNKNOWN:
                default:
                    return AudioManager.STREAM_SYSTEM;
            }
        }
        return 0;
    }

    void setAudioFocusChangeListener(AudioManager.OnAudioFocusChangeListener l) {
        mAudioFocusChangeListener = l;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && mAudioFocusRequest != null) {
            mAudioFocusRequest = new AudioFocusRequest.Builder(mAudioFocusRequest)
                                         .setOnAudioFocusChangeListener(mAudioFocusChangeListener)
                                         .build();
        }
    }

    int request(AudioManager audioManager) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return audioManager.requestAudioFocus(mAudioFocusRequest);
        } else {
            return audioManager.requestAudioFocus(
                    mAudioFocusChangeListener, getStreamType(), mFocusGain);
        }
    }

    int abandon(AudioManager audioManager) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return audioManager.abandonAudioFocusRequest(mAudioFocusRequest);
        } else {
            return audioManager.abandonAudioFocus(mAudioFocusChangeListener);
        }
    }

    /**
     * Backwards compatible builder method to create CastAudioFocusRequest object.
     */
    public static class Builder {
        private AudioAttributes mAudioAttributes;
        private int mFocusGain;
        private AudioManager.OnAudioFocusChangeListener mAudioFocusChangeListener;
        private Set<Integer> mValidFocusGainValues;

        public Builder() {
            mAudioAttributes = null;
            mFocusGain = 0;
            mAudioFocusChangeListener = null;
            mValidFocusGainValues = new HashSet<Integer>();
            mValidFocusGainValues.add(AudioManager.AUDIOFOCUS_GAIN);
            mValidFocusGainValues.add(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
            mValidFocusGainValues.add(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK);
            mValidFocusGainValues.add(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE);
        }

        public @NonNull Builder setAudioAttributes(@NonNull AudioAttributes audioAttributes) {
            mAudioAttributes = audioAttributes;
            return this;
        }

        public @NonNull Builder setFocusGain(int focusGain) {
            if (mValidFocusGainValues.contains(focusGain)) {
                mFocusGain = focusGain;
            } else {
                Log.e(TAG, "Invalid focus gain value " + focusGain);
                mFocusGain = AudioManager.AUDIOFOCUS_GAIN;
            }
            return this;
        }

        public @NonNull Builder setAudioFocusChangeListener(
                AudioManager.OnAudioFocusChangeListener l) {
            mAudioFocusChangeListener = l;
            return this;
        }

        public CastAudioFocusRequest build() {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                AudioFocusRequest.Builder builder = new AudioFocusRequest.Builder(mFocusGain);
                if (mAudioAttributes != null) {
                    builder = builder.setAudioAttributes(mAudioAttributes);
                }
                if (mAudioFocusChangeListener != null) {
                    builder = builder.setOnAudioFocusChangeListener(mAudioFocusChangeListener);
                }
                AudioFocusRequest audioFocusRequest = builder.build();
                return new CastAudioFocusRequest(audioFocusRequest);
            } else {
                return new CastAudioFocusRequest(
                        mAudioAttributes, mFocusGain, mAudioFocusChangeListener);
            }
        }
    }
}
