// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.viz.service.frame_sinks;

import android.view.Choreographer;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.TraceEvent;

/** Provides a VSyncMonitor backed BeginFrameSource. */
@JNINamespace("viz")
public class ExternalBeginFrameSourceAndroid implements Choreographer.FrameCallback {
    private static final long NANOSECONDS_PER_SECOND = 1000000000;
    private static final long NANOSECONDS_PER_MICROSECOND = 1000;

    // Conservative guess about vsync's consecutivity.
    // If true, next tick is guaranteed to be consecutive.
    private boolean mConsecutiveVSync;
    private boolean mInsideVSync;

    // Display refresh rate as reported by the system.
    private long mRefreshPeriodNano;
    private boolean mUseEstimatedRefreshRate;

    private boolean mHaveRequestInFlight;

    private final Choreographer mChoreographer;
    private long mGoodStartingPointNano;

    private final long mNativeExternalBeginFrameSourceAndroid;
    private boolean mVSyncNotificationsEnabled;

    @CalledByNative
    private ExternalBeginFrameSourceAndroid(
            long nativeExternalBeginFrameSourceAndroid, float refreshRate) {
        updateRefreshRate(refreshRate);

        mChoreographer = Choreographer.getInstance();
        mGoodStartingPointNano = getCurrentNanoTime();

        mNativeExternalBeginFrameSourceAndroid = nativeExternalBeginFrameSourceAndroid;
    }

    @CalledByNative
    private void setEnabled(boolean enabled) {
        if (mVSyncNotificationsEnabled == enabled) {
            return;
        }

        mVSyncNotificationsEnabled = enabled;
        if (mVSyncNotificationsEnabled) {
            postCallback();
        }
    }

    @CalledByNative
    private void updateRefreshRate(float refreshRate) {
        mUseEstimatedRefreshRate = refreshRate < 30;
        if (refreshRate <= 0) refreshRate = 60;
        mRefreshPeriodNano = (long) (NANOSECONDS_PER_SECOND / refreshRate);
    }

    private void postCallback() {
        if (mHaveRequestInFlight) return;
        mHaveRequestInFlight = true;
        mConsecutiveVSync = mInsideVSync;
        mChoreographer.postFrameCallback(this);
    }

    @Override
    public void doFrame(long frameTimeNanos) {
        TraceEvent.begin("VSync");
        try {
            if (mUseEstimatedRefreshRate && mConsecutiveVSync) {
                // Display.getRefreshRate() is unreliable on some platforms.
                // Adjust refresh period- initial value is based on Display.getRefreshRate()
                // after that it asymptotically approaches the real value.
                long lastRefreshDurationNano = frameTimeNanos - mGoodStartingPointNano;
                float lastRefreshDurationWeight = 0.1f;
                mRefreshPeriodNano +=
                        (long)
                                (lastRefreshDurationWeight
                                        * (lastRefreshDurationNano - mRefreshPeriodNano));
            }
            mGoodStartingPointNano = frameTimeNanos;
            mInsideVSync = true;
            assert mHaveRequestInFlight;
            mHaveRequestInFlight = false;

            if (!mVSyncNotificationsEnabled) {
                return;
            }
            ExternalBeginFrameSourceAndroidJni.get()
                    .onVSync(
                            mNativeExternalBeginFrameSourceAndroid,
                            ExternalBeginFrameSourceAndroid.this,
                            frameTimeNanos / NANOSECONDS_PER_MICROSECOND,
                            mRefreshPeriodNano / NANOSECONDS_PER_MICROSECOND);
            postCallback();
        } finally {
            mInsideVSync = false;
            TraceEvent.end("VSync");
        }
    }

    private long getCurrentNanoTime() {
        return System.nanoTime();
    }

    @NativeMethods
    interface Natives {
        void onVSync(
                long nativeExternalBeginFrameSourceAndroid,
                ExternalBeginFrameSourceAndroid caller,
                long vsyncTimeMicros,
                long vsyncPeriodMicros);
    }
}
