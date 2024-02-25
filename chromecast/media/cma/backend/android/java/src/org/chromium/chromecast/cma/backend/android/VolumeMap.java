// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.cma.backend.android;

import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.util.SparseIntArray;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chromecast.media.AudioContentType;

/**
 * Implements the java-side of the volume control API that maps between volume levels ([0..100])
 * and dBFS values. It uses an Android API that was made public in SDK version 28.
 */
@JNINamespace("chromecast::media")
public final class VolumeMap {
    private static final String TAG = "VolumeMap";

    private static final int DEVICE_TYPE = AudioDeviceInfo.TYPE_BUILTIN_SPEAKER;

    private static AudioManager sAudioManager;

    // Mapping from Android's stream_type to Cast's AudioContentType (used for callback).
    private static final SparseIntArray ANDROID_TYPE_TO_CAST_TYPE_MAP;
    static {
        var array = new SparseIntArray(4);
        array.append(AudioManager.STREAM_MUSIC, AudioContentType.MEDIA);
        array.append(AudioManager.STREAM_ALARM, AudioContentType.ALARM);
        array.append(AudioManager.STREAM_SYSTEM, AudioContentType.COMMUNICATION);
        array.append(AudioManager.STREAM_VOICE_CALL, AudioContentType.OTHER);
        ANDROID_TYPE_TO_CAST_TYPE_MAP = array;
    }

    private static final SparseIntArray MAX_VOLUME_INDEX;
    static {
        var array = new SparseIntArray(4);
        array.append(AudioManager.STREAM_MUSIC,
                getAudioManager().getStreamMaxVolume(AudioManager.STREAM_MUSIC));
        array.append(AudioManager.STREAM_ALARM,
                getAudioManager().getStreamMaxVolume(AudioManager.STREAM_ALARM));
        array.append(AudioManager.STREAM_SYSTEM,
                getAudioManager().getStreamMaxVolume(AudioManager.STREAM_SYSTEM));
        array.append(AudioManager.STREAM_VOICE_CALL,
                getAudioManager().getStreamMaxVolume(AudioManager.STREAM_VOICE_CALL));
        MAX_VOLUME_INDEX = array;
    }

    private static int getStreamMinVolume(int streamType) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return getAudioManager().getStreamMinVolume(streamType);
        }
        //  Try to use reflection in case API is hidden.
        try {
            return (int) getAudioManager()
                    .getClass()
                    .getMethod("getStreamMinVolume", int.class)
                    .invoke(sAudioManager, streamType);
        } catch (Exception e) {
            Log.w(TAG, "Unsupported Android SDK version: " + Build.VERSION.SDK_INT, e);
            return 0;
        }
    };

    private static final SparseIntArray MIN_VOLUME_INDEX;
    static {
        var array = new SparseIntArray(4);
        array.append(AudioManager.STREAM_MUSIC, getStreamMinVolume(AudioManager.STREAM_MUSIC));
        array.append(AudioManager.STREAM_ALARM, getStreamMinVolume(AudioManager.STREAM_ALARM));
        array.append(AudioManager.STREAM_SYSTEM, getStreamMinVolume(AudioManager.STREAM_SYSTEM));
        array.append(
                AudioManager.STREAM_VOICE_CALL, getStreamMinVolume(AudioManager.STREAM_VOICE_CALL));
        MIN_VOLUME_INDEX = array;
    }

    private static AudioManager getAudioManager() {
        if (sAudioManager == null) {
            Context context = ContextUtils.getApplicationContext();
            sAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        }
        return sAudioManager;
    }

    private static int getStreamType(int castType) {
        int i = ANDROID_TYPE_TO_CAST_TYPE_MAP.indexOfValue(castType);
        return ANDROID_TYPE_TO_CAST_TYPE_MAP.keyAt(i);
    }

    // Returns the current volume in dB for the given stream type and volume index.
    private static float getStreamVolumeDB(int streamType, int idx) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return getAudioManager().getStreamVolumeDb(streamType, idx, DEVICE_TYPE);
        }
        float db = 0;
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.O_MR1
                || Build.VERSION.SDK_INT == Build.VERSION_CODES.N) {
            // API is hidden, use reflection.
            try {
                db = (float) getAudioManager()
                             .getClass()
                             .getMethod("getStreamVolumeDb", int.class, int.class, int.class)
                             .invoke(sAudioManager, streamType, idx, DEVICE_TYPE);
            } catch (Exception e) {
                Log.e(TAG, "Can not call AudioManager.getStreamVolumeDb():", e);
            }
        } else {
            Log.e(TAG, "Unsupported Android SDK version:" + Build.VERSION.SDK_INT);
        }
        return db;
    }

    /** Return the max volume index for the given cast type. */
    @CalledByNative
    static int getMaxVolumeIndex(int castType) {
        int streamType = getStreamType(castType);
        return MAX_VOLUME_INDEX.get(streamType);
    }

    /**
     * Logs the dB value at each discrete Android volume index for the given cast type.
     * Note that this is not identical to the volume table, which may contain a different number
     * of points and at different levels.
     */
    static void dumpVolumeTables(int castType) {
        int streamType = getStreamType(castType);
        int maxIndex = MAX_VOLUME_INDEX.get(streamType);
        int minIndex = MIN_VOLUME_INDEX.get(streamType);
        Log.i(TAG,
                "Volume points for stream " + streamType + " (maxIndex=" + maxIndex
                        + " minIndex=" + minIndex + "):");
        for (int idx = minIndex; idx <= maxIndex; idx++) {
            float db = getStreamVolumeDB(streamType, idx);
            float level = (float) idx / (float) maxIndex;
            Log.i(TAG, "    " + idx + "(" + level + ") -> " + db);
        }
    }

    /**
     * Returns the dB value for the given volume level using the volume table for the given type.
     */
    @CalledByNative
    static float volumeToDbFs(int castType, float level) {
        level = Math.min(1.0f, Math.max(0.0f, level));
        int streamType = getStreamType(castType);
        int volumeIndex = Math.round(level * (float) MAX_VOLUME_INDEX.get(streamType));
        volumeIndex = Math.max(volumeIndex, MIN_VOLUME_INDEX.get(streamType));
        return getStreamVolumeDB(streamType, volumeIndex);
    }

    /**
     * Returns the volume level for the given dB value using the volume table for the given type.
     */
    static float dbFsToVolume(int castType, float db) {
        int streamType = getStreamType(castType);
        int maxIndex = MAX_VOLUME_INDEX.get(streamType);
        int minIndex = MIN_VOLUME_INDEX.get(streamType);

        float dbMin = getStreamVolumeDB(streamType, minIndex);
        if (db <= dbMin) return (float) minIndex / (float) maxIndex;
        float dbMax = getStreamVolumeDB(streamType, maxIndex);
        if (db >= dbMax) return 1.0f;

        // There are only a few volume index steps, so simply loop through them
        // and find the interval [dbLeft .. dbRight] that contains db, then
        // interpolate to estimate the volume level to return.
        float dbLeft = dbMin;
        float dbRight = dbMin;
        int idx = minIndex + 1;
        for (; idx <= maxIndex; idx++) {
            dbLeft = dbRight;
            dbRight = getStreamVolumeDB(streamType, idx);
            if (db <= dbRight) {
                break;
            }
        }
        float interpolatedIdx = (db - dbLeft) / (dbRight - dbLeft) + (idx - 1);
        float level = Math.min(1.0f, Math.max(0.0f, interpolatedIdx / (float) maxIndex));
        return level;
    }
}
