// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.gms.shadows.ShadowChromiumPlayServicesAvailability;

import java.util.concurrent.TimeUnit;

/** Tests for {@link BackgroundTaskSchedulerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowChromiumPlayServicesAvailability.class})
public class BackgroundTaskSchedulerImplWithMockTest {
    private static final int TEST_MINUTES = 10;

    private MockBackgroundTaskSchedulerDelegate mDelegate;
    private BackgroundTaskScheduler mTaskScheduler;

    @Before
    public void setUp() {
        mDelegate = new MockBackgroundTaskSchedulerDelegate();
        mTaskScheduler = new BackgroundTaskSchedulerImpl(mDelegate);
    }

    @Test
    public void testOneOffTaskScheduling() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowEndTimeMs(TimeUnit.MINUTES.toMillis(TEST_MINUTES))
                        .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mTaskScheduler.schedule(null, oneOffTask));
                });

        Assert.assertEquals(oneOffTask, mDelegate.getScheduledTaskInfo());
        Assert.assertEquals(0, mDelegate.getCanceledTaskId());
    }

    @Test
    public void testPeriodicTaskScheduling() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.PeriodicInfo.create()
                        .setIntervalMs(TimeUnit.MINUTES.toMillis(TEST_MINUTES))
                        .build();
        TaskInfo periodicTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mTaskScheduler.schedule(null, periodicTask));
                });

        Assert.assertEquals(periodicTask, mDelegate.getScheduledTaskInfo());
        Assert.assertEquals(0, mDelegate.getCanceledTaskId());
    }

    @Test
    public void testTaskCanceling() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowEndTimeMs(TimeUnit.MINUTES.toMillis(TEST_MINUTES))
                        .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mTaskScheduler.schedule(null, oneOffTask));
                    mTaskScheduler.cancel(null, TaskIds.TEST);
                });

        Assert.assertEquals(null, mDelegate.getScheduledTaskInfo());
        Assert.assertEquals(TaskIds.TEST, mDelegate.getCanceledTaskId());
    }
}
