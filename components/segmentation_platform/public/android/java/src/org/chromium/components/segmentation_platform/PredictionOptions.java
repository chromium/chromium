// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

@JNINamespace("segmentation_platform")
public class PredictionOptions {
    private final boolean mOnDemandExecution;
    private final boolean mCanUpdateCacheForFutureRequests;
    private final boolean mFallbackAllowed;

    public PredictionOptions(boolean onDemandExecution) {
        mOnDemandExecution = onDemandExecution;
        mCanUpdateCacheForFutureRequests = false;
        mFallbackAllowed = false;
    }

    public PredictionOptions(
            boolean onDemandExecution,
            boolean canUpdateCacheForFutureRequests,
            boolean fallbackAllowed) {
        mOnDemandExecution = onDemandExecution;
        mCanUpdateCacheForFutureRequests = canUpdateCacheForFutureRequests;
        mFallbackAllowed = fallbackAllowed;
    }

    public static PredictionOptions forOndemand(boolean canFallbackToCache) {
        return new PredictionOptions(
                /* onDemandExecution= */ true,
                /* canUpdateCacheForFutureRequests= */ false,
                canFallbackToCache);
    }

    public static PredictionOptions forCached(boolean canFallbackToExecution) {
        return new PredictionOptions(
                /* onDemandExecution= */ false,
                /* canUpdateCacheForFutureRequests= */ true,
                canFallbackToExecution);
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) {
            return true;
        }

        if (!(other instanceof PredictionOptions)) {
            return false;
        }

        PredictionOptions that = (PredictionOptions) other;

        return this.mOnDemandExecution == that.mOnDemandExecution
                && this.mCanUpdateCacheForFutureRequests == that.mCanUpdateCacheForFutureRequests
                && this.mFallbackAllowed == that.mFallbackAllowed;
    }

    @CalledByNative
    void fillNativePredictionOptions(long target) {
        PredictionOptionsJni.get()
                .fillNative(
                        target,
                        mOnDemandExecution,
                        mCanUpdateCacheForFutureRequests,
                        mFallbackAllowed);
    }

    @NativeMethods
    interface Natives {
        void fillNative(
                long target,
                boolean onDemandExecution,
                boolean canUpdateCacheForFutureRequests,
                boolean fallbackAllowed);
    }
}
