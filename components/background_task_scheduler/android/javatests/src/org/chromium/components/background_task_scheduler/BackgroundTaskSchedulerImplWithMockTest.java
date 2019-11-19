// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link BackgroundTaskSchedulerImpl}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class BackgroundTaskSchedulerImplWithMockTest {
    private static final int TEST_MINUTES = 10;

    private MockBackgroundTaskSchedulerDelegate mDelegate;
    private BackgroundTaskScheduler mTaskScheduler;

    @Before
    public void setUp() {
        mDelegate = new MockBackgroundTaskSchedulerDelegate();
        mTaskScheduler = new BackgroundTaskSchedulerImpl(
                mDelegate, new BackgroundTaskSchedulerAlarmManager());
    }

    @Test
    @SmallTest
    public void testOneOffTaskScheduling() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowEndTimeMs(TimeUnit.MINUTES.toMillis(TEST_MINUTES))
                        .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(mTaskScheduler.schedule(null, oneOffTask)); });

        Assert.assertEquals(oneOffTask, mDelegate.getScheduledTaskInfo());
        Assert.assertEquals(0, mDelegate.getCanceledTaskId());
    }

    @Test
    @SmallTest
    public void testPeriodicTaskScheduling() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.PeriodicInfo.create()
                        .setIntervalMs(TimeUnit.MINUTES.toMillis(TEST_MINUTES))
                        .build();
        TaskInfo periodicTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(mTaskScheduler.schedule(null, periodicTask)); });

        Assert.assertEquals(periodicTask, mDelegate.getScheduledTaskInfo());
        Assert.assertEquals(0, mDelegate.getCanceledTaskId());
    }

    @Test
    @SmallTest
    public void testTaskCanceling() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowEndTimeMs(TimeUnit.MINUTES.toMillis(TEST_MINUTES))
                        .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mTaskScheduler.schedule(null, oneOffTask));
            mTaskScheduler.cancel(null, TaskIds.TEST);
        });

        Assert.assertEquals(null, mDelegate.getScheduledTaskInfo());
        Assert.assertEquals(TaskIds.TEST, mDelegate.getCanceledTaskId());
    }
}