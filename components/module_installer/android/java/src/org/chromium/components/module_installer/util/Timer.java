// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.util;

import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;

import java.io.Closeable;

/**
 * Implements a timer through try-with-resources. New instances will be ignored if an existing
 * timer is already running, so this is not necessary for private methods.
 *
 * This should only be used on the UI thread to avoid race conditions.
 */
public class Timer implements Closeable {
    private static Timer sCurrentTimer;
    private static long sTotalTime;

    private final long mStartTime;

    public Timer() {
        mStartTime = SystemClock.uptimeMillis();
        if (sCurrentTimer == null) {
            sCurrentTimer = this;
        }
    }

    @Override
    public void close() {
        if (sCurrentTimer == this) {
            sCurrentTimer = null;
            sTotalTime += SystemClock.uptimeMillis() - mStartTime;
        }
    }

    public static void recordStartupTime() {
        RecordHistogram.recordTimesHistogram("Android.FeatureModules.StartupTime", sTotalTime);
    }
}
