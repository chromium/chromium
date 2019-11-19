// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.cma.backend.android;

import android.annotation.TargetApi;
import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTimestamp;
import android.media.AudioTrack;
import android.os.Build;
import android.os.SystemClock;
import android.support.annotation.IntDef;
import android.util.SparseIntArray;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chromecast.media.AudioContentType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Implements an audio sink object using Android's AudioTrack module to
 * playback audio samples.
 * It assumes the following fixed configuration parameters:
 *   - PCM audio format (i.e., no encoded data like mp3)
 *   - samples are 4-byte floats, interleaved channels (i.e., interleaved audio
 *     data for stereo is "LRLRLRLRLR").
 * The configurable audio parameters are the sample rate (typically 44.1 or
 * 48 KHz) and the channel number.
 *
 * PCM data is shared through the JNI using memory-mapped ByteBuffer objects.
 * The AudioTrack.write() function is called in BLOCKING mode. That means when
 * in PLAYING state the call will block until all data has been accepted
 * (queued) by the Audio server. The native side feeding data in through the
 * JNI is assumed to be running in a dedicated thread to avoid hanging other
 * parts of the application.
 *
 * No locking of instance data is done as it is assumed to be called from a
 * single thread in native code.
 *
 */
@JNINamespace("chromecast::media")
@TargetApi(Build.VERSION_CODES.N)
class AudioSinkAudioTrackImpl {
    private static final String TAG = "AATrack";
    private static final int DEBUG_LEVEL = 0;

    // Mapping from Android's stream_type to Cast's AudioContentType (used for callback).
    private static final SparseIntArray CAST_TYPE_TO_ANDROID_USAGE_TYPE_MAP = new SparseIntArray(
            4) {
        {
            append(AudioContentType.MEDIA, AudioAttributes.USAGE_MEDIA);
            append(AudioContentType.ALARM, AudioAttributes.USAGE_ALARM);
            append(AudioContentType.COMMUNICATION, AudioAttributes.USAGE_ASSISTANCE_SONIFICATION);
            append(AudioContentType.OTHER, AudioAttributes.USAGE_VOICE_COMMUNICATION);
        }
    };

    private static final SparseIntArray CAST_TYPE_TO_ANDROID_CONTENT_TYPE_MAP = new SparseIntArray(
            4) {
        {
            append(AudioContentType.MEDIA, AudioAttributes.CONTENT_TYPE_MUSIC);
            // Note: ALARM uses the same as COMMUNICATON.
            append(AudioContentType.ALARM, AudioAttributes.CONTENT_TYPE_SONIFICATION);
            append(AudioContentType.COMMUNICATION, AudioAttributes.CONTENT_TYPE_SONIFICATION);
            append(AudioContentType.OTHER, AudioAttributes.CONTENT_TYPE_SPEECH);
        }
    };

    /** See VolumeControl for this mapping. */
    private static final SparseIntArray CAST_TYPE_TO_ANDROID_STREAM_TYPE = new SparseIntArray(4) {
        {
            append(AudioContentType.MEDIA, AudioManager.STREAM_MUSIC);
            append(AudioContentType.ALARM, AudioManager.STREAM_ALARM);
            append(AudioContentType.COMMUNICATION, AudioManager.STREAM_SYSTEM);
            append(AudioContentType.OTHER, AudioManager.STREAM_VOICE_CALL);
        }
    };

    // Hardcoded AudioTrack config parameters.
    private static final int AUDIO_FORMAT = AudioFormat.ENCODING_PCM_FLOAT;
    private static final int AUDIO_MODE = AudioTrack.MODE_STREAM;
    private static final int BYTES_PER_SAMPLE = 4; // float (4-bytes)

    // Parameter to determine the proper internal buffer size of the AudioTrack instance. In order
    // to minimize latency we want a buffer as small as possible. However, to avoid underruns we
    // need a size several times the size returned by AudioTrack.getMinBufferSize() (see
    // the Android documentation for details).
    private static final int MIN_BUFFER_SIZE_MULTIPLIER = 3;

    private static final long NO_TIMESTAMP = Long.MIN_VALUE;
    private static final long NO_FRAME_POSITION = -1;

    private static final long SEC_IN_NSEC = 1000000000L;
    private static final long SEC_IN_USEC = 1000000L;
    private static final long MSEC_IN_NSEC = 1000000L;
    private static final long USEC_IN_NSEC = 1000L;

    private static final long TIMESTAMP_UPDATE_PERIOD = 250 * MSEC_IN_NSEC;
    private static final long UNDERRUN_LOG_THROTTLE_PERIOD = SEC_IN_NSEC;

    // Internally Android fetches data from AudioTrack buffer in periods of 20ms.
    private static final long ANDROID_AUDIO_PERIOD_SIZE_US = 20000;

    // Threshold at which we start logging low buffer warnings.
    private static final long VERY_LOW_BUFFER_LEVEL = ANDROID_AUDIO_PERIOD_SIZE_US;

    private static long sInstanceCounter;

    // Maximum amount a timestamp may deviate from the previous one to be considered stable at
    // startup or after an underrun event.
    private static final long MAX_STABLE_TIMESTAMP_DEVIATION_NSEC = 150 * USEC_IN_NSEC;
    // Number of consecutive stable timestamps needed to make it a valid reference point at startup
    // or after an underrun event.
    private static final int MIN_TIMESTAMP_STABILITY_CNT = 3;
    // Minimum time timestamps need to be stable to make it a valid reference point at startup or
    // after an underrun event. This is an additional safeguard.
    private static final long MIN_TIMESTAMP_STABILITY_TIME_NSEC = 150 * USEC_IN_NSEC;
    // After startup, any timestamp deviating more than this amount is ignored.
    private static final long TSTAMP_DEV_THRESHOLD_TO_IGNORE_NSEC = 500 * USEC_IN_NSEC;
    // Don't ignore timestamps for longer than this amount of time.
    private static final long MAX_TIME_IGNORING_TSTAMPS_NSECS = SEC_IN_NSEC;

    // Additional padding for minimum buffer time, determined experimentally.
    private static final long MIN_BUFFERED_TIME_PADDING_US = 120000;

    // Max retries for AudioTrackBuilder
    private static final int MAX_RETRIES_FOR_AUDIO_TRACKS = 1;

    private static AudioManager sAudioManager;

    private static int sSessionIdMedia = AudioManager.ERROR;
    private static int sSessionIdCommunication = AudioManager.ERROR;

    private final long mNativeAudioSinkAudioTrackImpl;

    private String mTag = TAG;

    private ThrottledLog mBufferLevelWarningLog;
    private ThrottledLog mUnderrunWarningLog;
    private ThrottledLog mTStampJitterWarningLog;

    @IntDef({ReferenceTimestampState.STARTING_UP, ReferenceTimestampState.STABLE,
            ReferenceTimestampState.RESYNCING_AFTER_PAUSE,
            ReferenceTimestampState.RESYNCING_AFTER_UNDERRUN,
            ReferenceTimestampState.RESYNCING_AFTER_EXCESSIVE_TIMESTAMP_DRIFT})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ReferenceTimestampState {
        int STARTING_UP = 0; // Starting up, no valid reference time yet.
        int STABLE = 1; // Reference time exists and is updated regularly.
        int RESYNCING_AFTER_PAUSE = 2; // Sync the timestamp after pause so that the renderer delay
                                       // will be correct.
        int RESYNCING_AFTER_UNDERRUN = 3; // The AudioTrack hit an underrun and we need to find a
                                          // new reference timestamp after the underrun point.
        int RESYNCING_AFTER_EXCESSIVE_TIMESTAMP_DRIFT =
                4; // We experienced excessive and consistent
                   // jitters in the timestamps and we should find a
                   // new reference timestamp.
    }

    private @ReferenceTimestampState int mReferenceTimestampState;

    private boolean mIsInitialized;

    // Dynamic AudioTrack config parameter.
    private int mSampleRateInHz;
    private int mChannelCount;

    private AudioTrack mAudioTrack;

    // Timestamping logic for RenderingDelay calculations. See also the description for
    // getNewFramePos0Timestamp() for additional information.
    private long mRefNanoTimeAtFramePos0; // Reference time used to interpolate new timestamps at
                                          // different frame positions.
    private long mOriginalFramePosOfLastTimestamp; // The original frame position of the
                                                   // AudioTimestamp that was last read from the
                                                   // AudioTrack. This is used to filter duplicate
                                                   // timestamps.
    private long mRefNanoTimeAtFramePos0Candidate; // Candidate that still needs to show it is
                                                   // stable.
    private long mLastTimestampUpdateNsec; // Last time we updated the timestamp.
    private boolean mTriggerTimestampUpdateNow; // Set to true to trigger an early update.
    private long mTimestampStabilityCounter; // Counts consecutive stable timestamps at startup.
    private long mTimestampStabilityStartTimeNsec; // Time when we started being stable.

    private long mLastRenderingDelayUsecs;

    private int mLastUnderrunCount;

    // Statistics
    private long mTotalFramesWritten;

    // Sample Rate calculator
    private long mSRWindowStartTimeNsec;
    private long mSRWindowFramesWritten;

    // Buffers shared between native and java space to move data across the JNI.
    // We use a direct buffers so that the native class can have access to
    // the underlying memory address. This avoids the need to copy from a
    // jbyteArray to native memory. More discussion of this here:
    // http://developer.android.com/training/articles/perf-jni.html
    private ByteBuffer mPcmBuffer; // PCM audio data (native->java)
    private ByteBuffer mRenderingDelayBuffer; // RenderingDelay return value
                                              // (java->native)

    /**
     * Converts the given nanoseconds value into microseconds with proper rounding. It is assumed
     * that the value given is positive.
     */
    private static long convertNsecsToUsecs(long nsecs) {
        return (nsecs + 500) / 1000;
    }

    private static AudioManager getAudioManager() {
        if (sAudioManager == null) {
            sAudioManager = (AudioManager) ContextUtils.getApplicationContext().getSystemService(
                    Context.AUDIO_SERVICE);
        }
        return sAudioManager;
    }

    private static int getChannelConfig(int channelCount) {
        switch (channelCount) {
            case 1:
                return AudioFormat.CHANNEL_OUT_MONO;
            case 2:
                return AudioFormat.CHANNEL_OUT_STEREO;
            case 4:
                return AudioFormat.CHANNEL_OUT_QUAD;
            case 6:
                return AudioFormat.CHANNEL_OUT_5POINT1;
            case 8:
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    return AudioFormat.CHANNEL_OUT_7POINT1_SURROUND;
                } else {
                    return AudioFormat.CHANNEL_OUT_7POINT1;
                }
            default:
                Log.e(TAG, "Unsupported channel count: " + channelCount);
                return AudioFormat.CHANNEL_OUT_DEFAULT;
        }
    }

    @CalledByNative
    public static long getMinimumBufferedTime(int channelCount, int sampleRateInHz) {
        int sizeBytes = AudioTrack.getMinBufferSize(
                sampleRateInHz, getChannelConfig(channelCount), AUDIO_FORMAT);
        long sizeUs = SEC_IN_USEC * (long) sizeBytes
                / (BYTES_PER_SAMPLE * channelCount * (long) sampleRateInHz);
        return sizeUs + MIN_BUFFERED_TIME_PADDING_US;
    }

    @CalledByNative
    public static int getSessionIdMedia() {
        if (sSessionIdMedia == AudioManager.ERROR) {
            sSessionIdMedia = getAudioManager().generateAudioSessionId();
            if (sSessionIdMedia == AudioManager.ERROR) {
                Log.e(TAG, "Cannot generate session-id for media tracks!");
            } else {
                Log.i(TAG, "Session-id for media tracks is " + sSessionIdMedia);
            }
        }
        return sSessionIdMedia;
    }

    @CalledByNative
    public static int getSessionIdCommunication() {
        if (sSessionIdCommunication == AudioManager.ERROR) {
            sSessionIdCommunication = getAudioManager().generateAudioSessionId();
            if (sSessionIdCommunication == AudioManager.ERROR) {
                Log.e(TAG, "Cannot generate session-id for communication tracks!");
            } else {
                Log.i(TAG, "Session-id for communication tracks is " + sSessionIdCommunication);
            }
        }
        return sSessionIdCommunication;
    }

    /** Construction */
    @CalledByNative
    private static AudioSinkAudioTrackImpl createAudioSinkAudioTrackImpl(
            long nativeAudioSinkAudioTrackImpl) {
        return new AudioSinkAudioTrackImpl(nativeAudioSinkAudioTrackImpl);
    }

    private AudioSinkAudioTrackImpl(long nativeAudioSinkAudioTrackImpl) {
        mNativeAudioSinkAudioTrackImpl = nativeAudioSinkAudioTrackImpl;
        reset();
    }

    private void reset() {
        mIsInitialized = false;
        mLastTimestampUpdateNsec = NO_TIMESTAMP;
        mLastRenderingDelayUsecs = NO_TIMESTAMP;
        mTriggerTimestampUpdateNow = false;
        mTimestampStabilityCounter = 0;
        mReferenceTimestampState = ReferenceTimestampState.STARTING_UP;
        mOriginalFramePosOfLastTimestamp = NO_FRAME_POSITION;
        mLastUnderrunCount = 0;
        mTotalFramesWritten = 0;
    }

    private boolean haveValidRefPoint() {
        return mLastTimestampUpdateNsec != NO_TIMESTAMP;
    }

    /** Converts the given number of frames into an equivalent nanoTime period. */
    private long convertFramesToNanoTime(long numOfFrames) {
        // Use proper rounding (assumes all numbers are positive).
        return (SEC_IN_NSEC * numOfFrames + mSampleRateInHz / 2) / mSampleRateInHz;
    }

    /**
     * Initializes the instance by creating the AudioTrack object and allocating
     * the shared memory buffers.
     */
    @CalledByNative
    private void init(@AudioContentType int castContentType, int channelCount, int sampleRateInHz,
            int bytesPerBuffer) {
        mTag = TAG + "(" + castContentType + ":" + (sInstanceCounter++) + ")";

        // Setup throttled logs: pass the first 5, then every 1sec, reset after 5.
        mBufferLevelWarningLog = new ThrottledLog(Log::w, 5, 1000, 5000);
        mUnderrunWarningLog = new ThrottledLog(Log::w, 5, 1000, 5000);
        mTStampJitterWarningLog = new ThrottledLog(Log::w, 5, 1000, 5000);

        Log.i(mTag,
                "Init:"
                        + " channelCount=" + channelCount + " sampleRateInHz=" + sampleRateInHz
                        + " bytesPerBuffer=" + bytesPerBuffer);

        if (mIsInitialized) {
            Log.w(mTag, "Init: already initialized.");
            return;
        }

        if (sampleRateInHz <= 0) {
            Log.e(mTag, "Invalid sampleRateInHz=" + sampleRateInHz + " given!");
            return;
        }
        mSampleRateInHz = sampleRateInHz;
        mChannelCount = channelCount;

        int usageType = CAST_TYPE_TO_ANDROID_USAGE_TYPE_MAP.get(castContentType);
        int contentType = CAST_TYPE_TO_ANDROID_CONTENT_TYPE_MAP.get(castContentType);

        int sessionId = AudioManager.ERROR;
        if (castContentType == AudioContentType.MEDIA) {
            sessionId = getSessionIdMedia();
        } else if (castContentType == AudioContentType.COMMUNICATION) {
            sessionId = getSessionIdCommunication();
        }
        // AudioContentType.ALARM doesn't get a sessionId.

        int channelConfig = getChannelConfig(mChannelCount);
        int bufferSizeInBytes = MIN_BUFFER_SIZE_MULTIPLIER
                * AudioTrack.getMinBufferSize(mSampleRateInHz, channelConfig, AUDIO_FORMAT);
        int bufferSizeInMs =
                1000 * bufferSizeInBytes / (BYTES_PER_SAMPLE * mChannelCount * mSampleRateInHz);
        Log.i(mTag,
                "Init: create an AudioTrack of size=" + bufferSizeInBytes + " (" + bufferSizeInMs
                        + "ms) usageType=" + usageType + " contentType=" + contentType
                        + " with session-id=" + sessionId);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Retry if AudioTrack creation fails.
            int retries = 0;
            do {
                AudioTrack.Builder builder = new AudioTrack.Builder();
                builder.setBufferSizeInBytes(bufferSizeInBytes)
                        .setTransferMode(AUDIO_MODE)
                        .setAudioAttributes(new AudioAttributes.Builder()
                                                    .setUsage(usageType)
                                                    .setContentType(contentType)
                                                    .build())
                        .setAudioFormat(new AudioFormat.Builder()
                                                .setEncoding(AUDIO_FORMAT)
                                                .setSampleRate(mSampleRateInHz)
                                                .setChannelMask(channelConfig)
                                                .build());
                if (sessionId != AudioManager.ERROR) builder.setSessionId(sessionId);
                mAudioTrack = builder.build();
            } while (mAudioTrack == null && retries++ < MAX_RETRIES_FOR_AUDIO_TRACKS);
        } else {
            // Using pre-M API.
            if (sessionId == AudioManager.ERROR) {
                mAudioTrack = new AudioTrack(CAST_TYPE_TO_ANDROID_STREAM_TYPE.get(castContentType),
                        mSampleRateInHz, channelConfig, AUDIO_FORMAT, bufferSizeInBytes,
                        AudioTrack.MODE_STREAM);
            } else {
                mAudioTrack = new AudioTrack(CAST_TYPE_TO_ANDROID_STREAM_TYPE.get(castContentType),
                        mSampleRateInHz, channelConfig, AUDIO_FORMAT, bufferSizeInBytes,
                        AudioTrack.MODE_STREAM, sessionId);
            }
        }

        // Allocated shared buffers.
        mPcmBuffer = ByteBuffer.allocateDirect(bytesPerBuffer);
        mPcmBuffer.order(ByteOrder.nativeOrder());

        mRenderingDelayBuffer = ByteBuffer.allocateDirect(2 * 8); // 2 long
        mRenderingDelayBuffer.order(ByteOrder.nativeOrder());

        AudioSinkAudioTrackImplJni.get().cacheDirectBufferAddress(mNativeAudioSinkAudioTrackImpl,
                AudioSinkAudioTrackImpl.this, mPcmBuffer, mRenderingDelayBuffer);

        mIsInitialized = true;
    }

    @CalledByNative
    private void play() {
        Log.i(mTag, "Start playback");
        mSRWindowFramesWritten = 0;
        mAudioTrack.play();
        mTriggerTimestampUpdateNow = true; // Get a fresh timestamp asap.
    }

    @CalledByNative
    private void pause() {
        Log.i(mTag, "Pausing playback");
        mAudioTrack.pause();
        resyncTimestamp(ReferenceTimestampState.RESYNCING_AFTER_PAUSE);
    }

    @CalledByNative
    private void setVolume(float volume) {
        Log.i(mTag, "Setting volume to " + volume);
        int ret = mAudioTrack.setVolume(volume);
        if (ret != AudioTrack.SUCCESS) {
            Log.e(mTag, "Cannot set volume: ret=" + ret);
        }
    }

    private boolean isStopped() {
        return mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_STOPPED;
    }

    private boolean isPlaying() {
        return mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING;
    }

    private boolean isPaused() {
        return mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_PAUSED;
    }

    /** Stops the AudioTrack and returns an estimate of the time it takes for the remaining data
     * left in the internal queue to be played out (in usecs). */
    @CalledByNative
    private long prepareForShutdown() {
        long playtimeLeftNsecs;

        // Stop the AudioTrack. This will put it into STOPPED mode and audio will stop playing after
        // the last buffer that was written has been played.
        mAudioTrack.stop();

        // Estimate how much playing time is left based on the most recent reference point.
        updateRefPointTimestamp();
        if (haveValidRefPoint()) {
            long lastPlayoutTimeNsecs = getInterpolatedTStampNsecs(mTotalFramesWritten);
            long now = System.nanoTime();
            playtimeLeftNsecs = lastPlayoutTimeNsecs - now;
        } else {
            // We have no timestamp to estimate how much is left to play, so assume the worst case.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                long most_frames_left =
                        Math.min(mTotalFramesWritten, mAudioTrack.getBufferSizeInFrames());
                playtimeLeftNsecs = convertFramesToNanoTime(most_frames_left);
            } else {
                // Using pre-M API. Don't know how many frames there are, so assume the worst case.
                playtimeLeftNsecs = 0;
            }
        }
        return (playtimeLeftNsecs < 0) ? 0 : playtimeLeftNsecs / 1000; // return usecs
    }

    @CalledByNative
    /** Closes the instance by stopping playback and releasing the AudioTrack
     * object. */
    private void close() {
        Log.i(mTag, "Close AudioSinkAudioTrackImpl!");
        if (!mIsInitialized) {
            Log.w(mTag, "Close: not initialized.");
            return;
        }
        if (!isStopped()) mAudioTrack.stop();
        mAudioTrack.release();
        reset();
    }

    private String getPlayStateString() {
        switch (mAudioTrack.getPlayState()) {
            case AudioTrack.PLAYSTATE_PAUSED:
                return "PAUSED";
            case AudioTrack.PLAYSTATE_STOPPED:
                return "STOPPED";
            case AudioTrack.PLAYSTATE_PLAYING:
                return "PLAYING";
            default:
                return "UNKNOWN";
        }
    }

    int getUnderrunCount() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return mAudioTrack.getUnderrunCount();
        }
        // Using pre-N API.
        return 0;
    }

    /** Writes the PCM data of the given size into the AudioTrack object. The
     * PCM data is provided through the memory-mapped ByteBuffer.
     *
     * Returns the number of bytes written into the AudioTrack object, -1 for
     * error.
     */
    @CalledByNative
    private int writePcm(int sizeInBytes) {
        if (DEBUG_LEVEL >= 3) {
            Log.i(mTag,
                    "Writing new PCM data:"
                            + " sizeInBytes=" + sizeInBytes + " state=" + getPlayStateString()
                            + " underruns=" + mLastUnderrunCount);
        }

        if (!mIsInitialized) {
            Log.e(mTag, "not initialized!");
            return -1;
        }

        // Check buffer level before feeding in new data.
        if (haveValidRefPoint()) checkBufferLevel();

        // Setup the PCM ByteBuffer correctly.
        mPcmBuffer.limit(sizeInBytes);
        mPcmBuffer.position(0);

        // Feed into AudioTrack - blocking call.
        long beforeMsecs = SystemClock.elapsedRealtime();
        int bytesWritten = mAudioTrack.write(mPcmBuffer, sizeInBytes, AudioTrack.WRITE_BLOCKING);

        if (bytesWritten < 0) {
            int error = bytesWritten;
            Log.e(mTag, "Couldn't write into AudioTrack (" + error + ")");
            return error;
        }

        if (isStopped()) {
            // Data was written, start playing now.
            play();

            // If not all data fit on the previous write() call (since we were not in PLAYING state
            // it didn't block), do a second (now blocking) call to write().
            int bytesLeft = sizeInBytes - bytesWritten;
            if (bytesLeft > 0) {
                mPcmBuffer.position(bytesWritten);
                int moreBytesWritten =
                        mAudioTrack.write(mPcmBuffer, bytesLeft, AudioTrack.WRITE_BLOCKING);
                if (moreBytesWritten < 0) {
                    int error = moreBytesWritten;
                    Log.e(mTag, "Couldn't write into AudioTrack (" + error + ")");
                    return error;
                }
                bytesWritten += moreBytesWritten;
            }
        }

        int framesWritten = bytesWritten / (BYTES_PER_SAMPLE * mChannelCount);
        mTotalFramesWritten += framesWritten;

        if (DEBUG_LEVEL >= 3) {
            Log.i(mTag,
                    "  wrote " + bytesWritten + "/" + sizeInBytes + " total_bytes_written="
                            + (mTotalFramesWritten * BYTES_PER_SAMPLE * mChannelCount)
                            + " took:" + (SystemClock.elapsedRealtime() - beforeMsecs) + "ms");
        }

        if (bytesWritten < sizeInBytes && isPaused()) {
            // We are in PAUSED state, in which case the write() is non-blocking. If not all data
            // was written, we will come back here once we transition back into PLAYING state.
            return bytesWritten;
        }

        updateSampleRateMeasure(framesWritten);

        updateRenderingDelay();

        // TODO(ckuiper): Log key statistics (SR and underruns, e.g.) in regular intervals

        return bytesWritten;
    }

    /** Returns the elapsed time from the given start_time until now, in nsec. */
    private long elapsedNsec(long startTimeNsec) {
        return System.nanoTime() - startTimeNsec;
    }

    private void checkBufferLevel() {
        long bufferLevel = mTotalFramesWritten - mAudioTrack.getPlaybackHeadPosition();
        long bufferLevelUsec = convertNsecsToUsecs(convertFramesToNanoTime(bufferLevel));
        if (bufferLevelUsec <= VERY_LOW_BUFFER_LEVEL) {
            long lastRenderingDelayUsec =
                    (mLastRenderingDelayUsecs == NO_TIMESTAMP) ? -1 : mLastRenderingDelayUsecs;
            boolean hitUnderrun = (getUnderrunCount() != mLastUnderrunCount);
            mBufferLevelWarningLog.log(mTag,
                    "Low buffer level=" + bufferLevelUsec + "us "
                            + " RD=" + lastRenderingDelayUsec + (hitUnderrun ? "us *" : "us"));
        }
    }

    private void updateSampleRateMeasure(long framesWritten) {
        if (mSRWindowFramesWritten == 0) {
            // Start new window.
            mSRWindowStartTimeNsec = System.nanoTime();
            mSRWindowFramesWritten = framesWritten;
            return;
        }
        mSRWindowFramesWritten += framesWritten;
        long periodNsec = elapsedNsec(mSRWindowStartTimeNsec);
        float sampleRate = 1e9f * mSRWindowFramesWritten / periodNsec;
        if (DEBUG_LEVEL >= 3) {
            Log.i(mTag,
                    "SR=" + mSRWindowFramesWritten + "/" + (periodNsec / 1000)
                            + "us = " + sampleRate);
        }
    }

    private void updateRenderingDelay() {
        checkForUnderruns();
        updateRefPointTimestamp();
        if (!haveValidRefPoint()) {
            // No timestamp available yet, just put dummy values and return.
            mRenderingDelayBuffer.putLong(0, 0);
            mRenderingDelayBuffer.putLong(8, NO_TIMESTAMP);
            mLastRenderingDelayUsecs = NO_TIMESTAMP;
            return;
        }

        // Interpolate to get proper Rendering delay.
        long playoutTimeNsecs = getInterpolatedTStampNsecs(mTotalFramesWritten);
        long playoutTimeUsecs = convertNsecsToUsecs(playoutTimeNsecs);
        long nowUsecs = convertNsecsToUsecs(System.nanoTime());
        long delayUsecs = playoutTimeUsecs - nowUsecs;

        // Populate RenderingDelay return value for native land.
        mRenderingDelayBuffer.putLong(0, delayUsecs);
        mRenderingDelayBuffer.putLong(8, nowUsecs);
        mLastRenderingDelayUsecs = delayUsecs;

        if (DEBUG_LEVEL >= 3) {
            Log.i(mTag, "RenderingDelay: delay=" + delayUsecs + " play=" + nowUsecs);
        }
    }

    /**
     * Returns a new nanoTime timestamp for framePosition=0. This is done by reading an
     * AudioTimeStamp {nanoTime, framePosition} object from the AudioTrack and transforming it to
     * its {nanoTime', 0} equivalent by taking advantage of the fact that
     *      (nanoTime - nanoTime') / (framePosition - 0) = 1 / sampleRate.
     * The nanoTime' value is returned as the timestamp. If no new timestamp is available,
     * NO_TIMESTAMP is returned.
     */
    private long getNewFramePos0Timestamp() {
        AudioTimestamp ts = new AudioTimestamp();
        if (!mAudioTrack.getTimestamp(ts)) {
            return NO_TIMESTAMP;
        }
        // Check for duplicates, i.e., AudioTrack returned the same Timestamp object as last time.
        if (mOriginalFramePosOfLastTimestamp != NO_FRAME_POSITION
                && ts.framePosition == mOriginalFramePosOfLastTimestamp) {
            // Not a new timestamp, skip this one.
            return NO_TIMESTAMP;
        }
        mOriginalFramePosOfLastTimestamp = ts.framePosition;
        return ts.nanoTime - convertFramesToNanoTime(ts.framePosition);
    }

    /**
     * Returns a timestamp for the given frame position, interpolated from the reference timestamp.
     */
    private long getInterpolatedTStampNsecs(long framePosition) {
        return mRefNanoTimeAtFramePos0 + convertFramesToNanoTime(framePosition);
    }

    /** Checks for underruns and if detected invalidates the reference point timestamp. */
    private void checkForUnderruns() {
        int underruns = getUnderrunCount();
        if (underruns != mLastUnderrunCount) {
            mUnderrunWarningLog.log(mTag,
                    "Underrun detected (" + mLastUnderrunCount + "->" + underruns
                            + ")! Resetting rendering delay logic.");
            // Invalidate timestamp (resets RenderingDelay).
            mLastUnderrunCount = underruns;
            resyncTimestamp(ReferenceTimestampState.RESYNCING_AFTER_UNDERRUN);
        }
    }

    private void resyncTimestamp(@ReferenceTimestampState int reason) {
        mLastTimestampUpdateNsec = NO_TIMESTAMP;
        mTimestampStabilityCounter = 0;
        mReferenceTimestampState = reason;
    }

    /**
     * Returns true if the given timestamp is stable. A timestamp is considered stable if it and
     * its two predecessors do not deviate significantly from each other.
     */
    private boolean isTimestampStable(long newNanoTimeAtFramePos0) {
        if (mTimestampStabilityCounter == 0) {
            mRefNanoTimeAtFramePos0Candidate = newNanoTimeAtFramePos0;
            mTimestampStabilityCounter = 1;
            mTimestampStabilityStartTimeNsec = System.nanoTime();
            return false;
        }

        long deviation = mRefNanoTimeAtFramePos0Candidate - newNanoTimeAtFramePos0;
        if (Math.abs(deviation) > MAX_STABLE_TIMESTAMP_DEVIATION_NSEC) {
            // not stable
            Log.i(mTag,
                    "Timestamp [" + mTimestampStabilityCounter + "/"
                            + elapsedNsec(mTimestampStabilityStartTimeNsec) / 1000000
                            + "ms] is not stable (deviation:" + deviation / 1000 + "us)");
            // Use this as the new starting point.
            mRefNanoTimeAtFramePos0Candidate = newNanoTimeAtFramePos0;
            mTimestampStabilityCounter = 1;
            mTimestampStabilityStartTimeNsec = System.nanoTime();
            return false;
        }

        if ((elapsedNsec(mTimestampStabilityStartTimeNsec) > MIN_TIMESTAMP_STABILITY_TIME_NSEC)
                && ++mTimestampStabilityCounter >= MIN_TIMESTAMP_STABILITY_CNT) {
            return true;
        }

        return false;
    }

    /**
     * Update the reference timestamp used for interpolation.
     */
    private void updateRefPointTimestamp() {
        if (!mTriggerTimestampUpdateNow && haveValidRefPoint()
                && elapsedNsec(mLastTimestampUpdateNsec) <= TIMESTAMP_UPDATE_PERIOD) {
            // not time for an update yet
            return;
        }

        long newNanoTimeAtFramePos0 = getNewFramePos0Timestamp();
        if (newNanoTimeAtFramePos0 == NO_TIMESTAMP) {
            return; // no timestamp available
        }

        long prevRefNanoTimeAtFramePos0 = mRefNanoTimeAtFramePos0;
        switch (mReferenceTimestampState) {
            case ReferenceTimestampState.STARTING_UP:
                // The Audiotrack produces a few timestamps at the beginning of time that are widely
                // inaccurate. Hence, we require several stable timestamps before setting a
                // reference point.
                if (!isTimestampStable(newNanoTimeAtFramePos0)) {
                    return;
                }
                // First stable timestamp.
                mRefNanoTimeAtFramePos0 = prevRefNanoTimeAtFramePos0 = newNanoTimeAtFramePos0;
                mReferenceTimestampState = ReferenceTimestampState.STABLE;
                Log.i(mTag,
                        "First stable timestamp [" + mTimestampStabilityCounter + "/"
                                + elapsedNsec(mTimestampStabilityStartTimeNsec) / 1000000 + "ms]");
                break;
            case ReferenceTimestampState.RESYNCING_AFTER_PAUSE:
            // fall-through
            case ReferenceTimestampState.RESYNCING_AFTER_EXCESSIVE_TIMESTAMP_DRIFT:
            // fall-through
            case ReferenceTimestampState.RESYNCING_AFTER_UNDERRUN:
                // Resyncing happens after we hit a pause, underrun or excessive drift in the
                // AudioTrack. This causes the Android Audio stack to insert additional samples,
                // which increases the reference timestamp (at framePosition=0) by thousands of
                // usecs. Hence we need to find a new initial reference timestamp. Unfortunately,
                // even though the underrun already happened, the timestamps returned by the
                // AudioTrack may still be located *before* the underrun, and there is no way to
                // query the AudioTrack about at which framePosition the underrun occurred and where
                // and how much additional data was inserted.
                //
                // At this point we just do the same as when in STARTING_UP, but eventually there
                // should be a more refined way to figure out when the timestamps returned from the
                // AudioTrack are usable again.
                if (!isTimestampStable(newNanoTimeAtFramePos0)) {
                    return;
                }
                // Found a new stable timestamp.
                mRefNanoTimeAtFramePos0 = newNanoTimeAtFramePos0;
                mReferenceTimestampState = ReferenceTimestampState.STABLE;
                Log.i(mTag,
                        "New stable timestamp after pause, underrun or excessive drift ["
                                + mTimestampStabilityCounter + "/"
                                + elapsedNsec(mTimestampStabilityStartTimeNsec) / 1000000 + "ms]");
                break;

            case ReferenceTimestampState.STABLE:
                // Timestamps can be jittery, and on some systems they are occasionally off by
                // hundreds of usecs. Filter out timestamps that are too jittery and use a low-pass
                // filter on the smaller ones.
                // Note that the low-pass filter approach does not work well when the media clock
                // rate does not match the system clock rate, and the timestamp drifts as a result.
                // Currently none of the devices using this code do this.
                long devNsec = mRefNanoTimeAtFramePos0 - newNanoTimeAtFramePos0;
                if (Math.abs(devNsec) > TSTAMP_DEV_THRESHOLD_TO_IGNORE_NSEC) {
                    mTStampJitterWarningLog.log(
                            mTag, "Too jittery timestamp (" + convertNsecsToUsecs(devNsec) + ")");
                    long timeSinceLastGoodTstamp = elapsedNsec(mLastTimestampUpdateNsec);
                    if (timeSinceLastGoodTstamp <= MAX_TIME_IGNORING_TSTAMPS_NSECS) {
                        return; // Ignore this one.
                    }
                    // We ignored jittery timestamps for too long, restart sync logic.
                    Log.i(mTag, "Too many jittery timestamps ignored!");
                    mLastTimestampUpdateNsec = NO_TIMESTAMP;
                    mTimestampStabilityCounter = 0;
                    mReferenceTimestampState =
                            ReferenceTimestampState.RESYNCING_AFTER_EXCESSIVE_TIMESTAMP_DRIFT;
                }
                // Low-pass filter: 0.10*New + 0.90*Ref. Do integer math with proper rounding.
                mRefNanoTimeAtFramePos0 =
                        (10 * newNanoTimeAtFramePos0 + 90 * mRefNanoTimeAtFramePos0 + 50) / 100;
                break;
        }

        // Got a new value.
        if (DEBUG_LEVEL >= 1) {
            long dev1 = convertNsecsToUsecs(prevRefNanoTimeAtFramePos0 - newNanoTimeAtFramePos0);
            long dev2 = convertNsecsToUsecs(prevRefNanoTimeAtFramePos0 - mRefNanoTimeAtFramePos0);
            Log.i(mTag,
                    "Updated mRefNanoTimeAtFramePos0=" + mRefNanoTimeAtFramePos0 / 1000 + " us ("
                            + dev1 + "/" + dev2 + ")");
        }

        mLastTimestampUpdateNsec = System.nanoTime();
        mTriggerTimestampUpdateNow = false;
    }

    @NativeMethods
    interface Natives {
        void cacheDirectBufferAddress(long nativeAudioSinkAndroidAudioTrackImpl,
                AudioSinkAudioTrackImpl caller, ByteBuffer mPcmBuffer,
                ByteBuffer mRenderingDelayBuffer);
    }
}
