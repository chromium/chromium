// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.net.impl.CronetLogger.CronetSource;

/** Helper class to record UMA histograms from native code. */
@JNINamespace("cronet")
public final class CronetUmaRecorder {
    private static final String TAG = CronetUmaRecorder.class.getSimpleName();
    private static volatile CronetUmaRecorder sInstance;

    private final CronetLogger mLogger;
    private final CronetSource mSource;

    private CronetUmaRecorder(Context context, CronetSource source) {
        mSource = source;
        mLogger = CronetLoggerFactory.createLogger(context, mSource);
    }

    // Called under sLoadLock in CronetLibraryLoader
    public static void initialize(Context context, String allowlist, CronetSource source) {
        if (sInstance != null) {
            return;
        }

        sInstance = new CronetUmaRecorder(context, source);
        CronetUmaRecorderJni.get().initNative(allowlist);
    }

    static void triggerUmaHistogramForTesting(String histogramName, int value) {
        CronetUmaRecorderJni.get().triggerUmaHistogramForTesting(histogramName, value);
    }

    @CalledByNative
    public static void logCronetUmaHistogram(long metricHash, int value) {
        CronetUmaRecorder instance = sInstance;
        if (instance == null) {
            throw new IllegalStateException("CronetUmaRecorder is not initialized!");
        }
        instance.mLogger.logCronetUmaHistogram(metricHash, value, instance.mSource);
    }

    @NativeMethods
    interface Natives {
        void initNative(@JniType("std::string") String allowlist);

        void triggerUmaHistogramForTesting(@JniType("std::string") String histogramName, int value);
    }
}
