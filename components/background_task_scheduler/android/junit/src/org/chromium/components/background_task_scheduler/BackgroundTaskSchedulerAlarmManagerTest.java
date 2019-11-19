// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;

import android.app.PendingIntent;
import android.content.Intent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskSchedulerAlarmManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundTaskSchedulerAlarmManagerTest {
    private static final long CLOCK_TIME_MS = 1415926535000L;
    private static final long TIME_200_MIN_TO_MS = TimeUnit.MINUTES.toMillis(200);

    private BackgroundTaskSchedulerGcmNetworkManager.Clock mClock = () -> CLOCK_TIME_MS;

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testExactTaskParameters() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.ExactInfo.create()
                        .setTriggerAtMs(mClock.currentTimeMillis() + TIME_200_MIN_TO_MS)
                        .build();
        TaskInfo exactTaskInfo = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        PendingIntent pendingIntent =
                BackgroundTaskSchedulerAlarmManager.createPendingIntentFromTaskId(
                        ContextUtils.getApplicationContext(), exactTaskInfo.getTaskId());

        Intent intent =
                new Intent(
                        ContextUtils.getApplicationContext(), BackgroundTaskBroadcastReceiver.class)
                        .putExtra(BackgroundTaskSchedulerAlarmManager.BACKGROUND_TASK_ID_KEY,
                                TaskIds.TEST);
        assertEquals(PendingIntent.getBroadcast(ContextUtils.getApplicationContext(), TaskIds.TEST,
                             intent, PendingIntent.FLAG_CANCEL_CURRENT),
                pendingIntent);
    }
}
