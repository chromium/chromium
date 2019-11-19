// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.viz.service.frame_sinks;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.VSyncMonitor;

/**
 * Provides a VSyncMonitor backed BeginFrameSource.
 */
@JNINamespace("viz")
@MainDex
public class ExternalBeginFrameSourceAndroid {
    private final long mNativeExternalBeginFrameSourceAndroid;
    private boolean mVSyncNotificationsEnabled;
    private final VSyncMonitor mVSyncMonitor;
    private final VSyncMonitor.Listener mVSyncListener = new VSyncMonitor.Listener() {
        @Override
        public void onVSync(VSyncMonitor monitor, long vsyncTimeMicros) {
            if (!mVSyncNotificationsEnabled) {
                return;
            }
            ExternalBeginFrameSourceAndroidJni.get().onVSync(mNativeExternalBeginFrameSourceAndroid,
                    ExternalBeginFrameSourceAndroid.this, vsyncTimeMicros,
                    mVSyncMonitor.getVSyncPeriodInMicroseconds());
            mVSyncMonitor.requestUpdate();
        }
    };

    @CalledByNative
    private ExternalBeginFrameSourceAndroid(
            long nativeExternalBeginFrameSourceAndroid, float refreshRate) {
        mNativeExternalBeginFrameSourceAndroid = nativeExternalBeginFrameSourceAndroid;
        mVSyncMonitor =
                new VSyncMonitor(ContextUtils.getApplicationContext(), mVSyncListener, refreshRate);
    }

    @CalledByNative
    private void setEnabled(boolean enabled) {
        if (mVSyncNotificationsEnabled == enabled) {
            return;
        }

        mVSyncNotificationsEnabled = enabled;
        if (mVSyncNotificationsEnabled) {
            mVSyncMonitor.requestUpdate();
        }
    }

    @CalledByNative
    private void updateRefreshRate(float refreshRate) {
        mVSyncMonitor.updateRefreshRate(refreshRate);
    }

    @NativeMethods
    interface Natives {
        void onVSync(long nativeExternalBeginFrameSourceAndroid,
                ExternalBeginFrameSourceAndroid caller, long vsyncTimeMicros,
                long vsyncPeriodMicros);
    }
};
