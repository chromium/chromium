// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;

/**
 * Utility for logging UMA for permission-related operations.
 */
public class PermissionUmaUtil {
    private PermissionUmaUtil() {}

    /**
     * Log an action with the current battery level percentage.
     */
    @CalledByNative
    private static void recordWithBatteryBucket(String histogram) {
        IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent batteryStatus = ContextUtils.getApplicationContext().registerReceiver(null, ifilter);
        int current = batteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int max = batteryStatus.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (max == 0) return;
        int percentage = (int) (100.0 * current / max);

        RecordHistogram.recordPercentageHistogram(histogram, percentage);
    }
}
