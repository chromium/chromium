// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download.internal;

import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;

/**
 * Helper Java class to query battery status.
 *
 * The class is created and owned by native side.
 */
@JNINamespace("download")
public final class BatteryStatusListenerAndroid {
    @CalledByNative
    public static int getBatteryPercentage() {
        IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent batteryStatus =
                ContextUtils.registerProtectedBroadcastReceiver(
                        ContextUtils.getApplicationContext(), null, filter);
        if (batteryStatus == null) return 0;

        int scale = batteryStatus.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (scale == 0) return 0;

        int level = batteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int percentage = (int) Math.round(100.0 * level / scale);

        assert (percentage >= 0 && percentage <= 100);
        return percentage;
    }
}
