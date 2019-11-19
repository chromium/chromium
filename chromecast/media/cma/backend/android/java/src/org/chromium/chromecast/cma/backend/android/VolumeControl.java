// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.cma.backend.android;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.os.Build;
import android.util.SparseArray;
import android.util.SparseIntArray;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chromecast.media.AudioContentType;

/**
 * Implements the java-side of the volume control logic running on Android when using CMA backend by
 * setting the volume levels and mute states directly in the OS using AudioManager. kOther is mostly
 * used for voice calls, so use Android's STREAM_VOICE_CALL. The following mapping is used between
 * Cast's native AudioContentType and Android's internal stream types:
 *
 *   AudioContentType::kMedia         -> AudioManager.STREAM_MUSIC
 *   AudioContentType::kAlarm         -> AudioManager.STREAM_ALARM
 *   AudioContentType::kCommunication -> AudioManager.STREAM_SYSTEM
 *   AudioContentType::kOther         -> AudioManager.STREAM_VOICE_CALL
 *
 * In addition it listens to volume and mute state changes broadcasted by the system via (hidden)
 * intents and reports detected changes back to the native volume controller code.
 */
@JNINamespace("chromecast::media")
@TargetApi(Build.VERSION_CODES.M)
class VolumeControl {
    /**
     * Helper class storing settings and reading/writing volume and mute settings from/to Android's
     * AudioManager.
     */
    private class Settings {
        Settings(int streamType) {
            mStreamType = streamType;
            mMaxVolumeIndexAsFloat = (float) mAudioManager.getStreamMaxVolume(mStreamType);
            mMinVolumeIndex = getStreamMinVolume(mAudioManager, mStreamType);
            refreshVolume();
            refreshMuteState();
        }

        /** Returns the current volume level in the range [0.0f .. 1.0f]. */
        float getVolumeLevel() {
            return mVolumeIndexAsFloat / mMaxVolumeIndexAsFloat;
        }

        /** Sets the given volume level in AudioManager. The given level is in the range
         * [0.0f .. 1.0f] and converted to a volume index in the range
         * [mMinVolumeIndex .. mMaxVolumeIndex] before writing to AudioManager. */
        void setVolumeLevel(float level) {
            int volumeIndex = Math.round(level * mMaxVolumeIndexAsFloat);
            volumeIndex = Math.max(volumeIndex, mMinVolumeIndex);
            mVolumeIndexAsFloat = (float) volumeIndex;
            if (DEBUG_LEVEL >= 1) {
                Log.i(TAG,
                        "setVolumeLevel: index=" + mVolumeIndexAsFloat
                                + " level=" + getVolumeLevel() + " (from:" + level + ")");
            }
            mAudioManager.setStreamVolume(mStreamType, volumeIndex, 0);
        }

        /** Refreshes the stored volume level by reading it from AudioManager.
         *  Returns true if the value changed, false otherwise. */
        boolean refreshVolume() {
            float oldVolume = mVolumeIndexAsFloat;
            mVolumeIndexAsFloat = (float) mAudioManager.getStreamVolume(mStreamType);
            if (DEBUG_LEVEL >= 2) {
                Log.i(TAG, "refresh: index=" + mVolumeIndexAsFloat + " level=" + getVolumeLevel());
            }
            return oldVolume != mVolumeIndexAsFloat;
        }

        /** Returns the current mute state. */
        boolean isMuted() {
            return mIsMuted;
        }

        /** Sets the given mute state in AudioManager. */
        void setMuted(boolean muted) {
            if (DEBUG_LEVEL >= 1) Log.i(TAG, "setMuted: muted=" + muted);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                int direction = muted ? AudioManager.ADJUST_MUTE : AudioManager.ADJUST_UNMUTE;
                int flag = 0;
                mAudioManager.adjustStreamVolume(mStreamType, direction, flag);
            } else {
                mAudioManager.setStreamMute(mStreamType, muted);
            }
        }

        /** Refreshes the stored mute state by reading it from AudioManager.
         * Returns true if the state changed, false otherwise. */
        boolean refreshMuteState() {
            boolean oldMuteState = mIsMuted;
            mIsMuted = mAudioManager.isStreamMute(mStreamType);
            if (DEBUG_LEVEL >= 2) Log.i(TAG, "refresh: muted=" + mIsMuted);
            return oldMuteState != mIsMuted;
        }

        private final int mStreamType;

        // Cached maximum volume index. Stored as float for easier calculations.
        private final float mMaxVolumeIndexAsFloat;

        // Cached minimum volume index.
        private int mMinVolumeIndex;

        // Current volume index. Stored as float for easier calculations.
        float mVolumeIndexAsFloat;

        boolean mIsMuted;
    }

    private static final String TAG = "VolumeControlAndroid";
    private static final int DEBUG_LEVEL = 0;

    // Hidden intent actions of AudioManager.
    private static final String VOLUME_CHANGED_ACTION = "android.media.VOLUME_CHANGED_ACTION";
    private static final String STREAM_MUTE_CHANGED_ACTION =
            "android.media.STREAM_MUTE_CHANGED_ACTION";
    private static final String EXTRA_VOLUME_STREAM_TYPE = "android.media.EXTRA_VOLUME_STREAM_TYPE";

    // Mapping from Android's stream_type to Cast's AudioContentType (used for callback).
    private static final SparseIntArray ANDROID_TYPE_TO_CAST_TYPE_MAP = new SparseIntArray(4) {
        {
            append(AudioManager.STREAM_MUSIC, AudioContentType.MEDIA);
            append(AudioManager.STREAM_ALARM, AudioContentType.ALARM);
            append(AudioManager.STREAM_SYSTEM, AudioContentType.COMMUNICATION);
            append(AudioManager.STREAM_VOICE_CALL, AudioContentType.OTHER);
        }
    };

    private final long mNativeVolumeControl;

    private Context mContext;

    private AudioManager mAudioManager;

    private BroadcastReceiver mMediaEventIntentListener;

    // Mapping from Cast's AudioContentType to their respective Settings instance.
    private SparseArray<Settings> mSettings;

    /** Construction */
    @CalledByNative
    static VolumeControl createVolumeControl(long nativeVolumeControl) {
        Log.i(TAG, "Creating new VolumeControl instance");
        return new VolumeControl(nativeVolumeControl);
    }

    /**
     * Creates a new instance. The given cast type ids are used to create the
     * settings array, mapping Cast's AudioContentType::{kMedia, kAlarm,
     * kCommunication} to Android's AudioManager.STREAM_{MUSIC,ALARM,SYSTEM} settings.
     */
    private VolumeControl(long nativeVolumeControl) {
        mNativeVolumeControl = nativeVolumeControl;

        mContext = ContextUtils.getApplicationContext();
        mAudioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);

        // Populate settings.
        mSettings = new SparseArray<Settings>(4);
        mSettings.append(AudioContentType.MEDIA, new Settings(AudioManager.STREAM_MUSIC));
        mSettings.append(AudioContentType.ALARM, new Settings(AudioManager.STREAM_ALARM));
        mSettings.append(AudioContentType.COMMUNICATION, new Settings(AudioManager.STREAM_SYSTEM));
        mSettings.append(AudioContentType.OTHER, new Settings(AudioManager.STREAM_VOICE_CALL));

        registerIntentListeners();
    }

    /** Registers the intent listeners for volume and mute changes. */
    private void registerIntentListeners() {
        mMediaEventIntentListener = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();
                int type = intent.getIntExtra(EXTRA_VOLUME_STREAM_TYPE, -1);
                if (type != AudioManager.STREAM_MUSIC && type != AudioManager.STREAM_ALARM
                        && type != AudioManager.STREAM_SYSTEM
                        && type != AudioManager.STREAM_VOICE_CALL) {
                    return;
                }
                if (DEBUG_LEVEL >= 1) Log.i(TAG, "Got intent:" + action + " for type:" + type);
                if (action.equals(VOLUME_CHANGED_ACTION)) {
                    handleVolumeChange(type);
                }
                if (action.equals(STREAM_MUTE_CHANGED_ACTION)) {
                    handleMuteChange(type);
                }
            }
        };
        IntentFilter mediaEventIntentFilter = new IntentFilter();
        mediaEventIntentFilter.addAction(VOLUME_CHANGED_ACTION);
        mediaEventIntentFilter.addAction(STREAM_MUTE_CHANGED_ACTION);
        mContext.registerReceiver(mMediaEventIntentListener, mediaEventIntentFilter);
    }

    /**
     * Handles received volume change events by checking the value for the provided type and
     * triggering the native callback function if changes are detected.
     */
    private void handleVolumeChange(int androidType) {
        int castType = ANDROID_TYPE_TO_CAST_TYPE_MAP.get(androidType);
        Settings s = mSettings.get(castType);
        if (s.refreshVolume()) {
            if (DEBUG_LEVEL >= 1) {
                Log.i(TAG, "New volume for castType " + castType + " is " + s.getVolumeLevel());
            }
            VolumeControlJni.get().onVolumeChange(
                    mNativeVolumeControl, VolumeControl.this, castType, s.getVolumeLevel());
        }
    }

    /**
     * Handles mute state change events by checking the state for the provided type and triggering
     * the native callback function if changes are detected.
     */
    private void handleMuteChange(int androidType) {
        int castType = ANDROID_TYPE_TO_CAST_TYPE_MAP.get(androidType);
        Settings s = mSettings.get(castType);
        if (s.refreshMuteState()) {
            if (DEBUG_LEVEL >= 1) {
                Log.i(TAG, "New mute state for castType " + castType + " is " + s.isMuted());
            }
            VolumeControlJni.get().onMuteChange(
                    mNativeVolumeControl, VolumeControl.this, castType, s.isMuted());
        }
    }

    /** Returns the volume for the given cast type as a float in the range [0.0f .. 1.0f]. */
    @CalledByNative
    float getVolume(int castType) {
        Settings s = mSettings.get(castType);
        s.refreshVolume();
        return s.getVolumeLevel();
    }

    /** Sets the given volume (range [0 .. 1.0]) for the given cast type. */
    @CalledByNative
    void setVolume(int castType, float level) {
        level = Math.min(1.0f, Math.max(0.0f, level));
        mSettings.get(castType).setVolumeLevel(level);
    }

    /** Returns the mute state (true/false) for the given cast type. */
    @CalledByNative
    boolean isMuted(int castType) {
        Settings s = mSettings.get(castType);
        s.refreshMuteState();
        return s.isMuted();
    }

    /** Sets the mute state for streams of the given cast type. */
    @CalledByNative
    void setMuted(int castType, boolean muted) {
        mSettings.get(castType).setMuted(muted);
    }

    @SuppressWarnings("NewApi")
    private static int getStreamMinVolume(AudioManager audioManager, int streamType) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return audioManager.getStreamMinVolume(streamType);
        }
        return 0;
    }

    @NativeMethods
    interface Natives {
        void onVolumeChange(
                long nativeVolumeControlAndroid, VolumeControl caller, int type, float level);

        void onMuteChange(
                long nativeVolumeControlAndroid, VolumeControl caller, int type, boolean muted);
    }
}
