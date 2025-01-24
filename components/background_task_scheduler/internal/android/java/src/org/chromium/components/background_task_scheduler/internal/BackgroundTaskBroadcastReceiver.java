// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;

/**
 * Leftover implementation of an earlier BroadcastReceiver used for BackgroundTasks scheduled at
 * an exact time.
 */
@NullMarked
public class BackgroundTaskBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = "BkgrdTaskBR";

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.e(TAG, "AlarmManager based BackgroundTasks are unsupported.");
    }
}
