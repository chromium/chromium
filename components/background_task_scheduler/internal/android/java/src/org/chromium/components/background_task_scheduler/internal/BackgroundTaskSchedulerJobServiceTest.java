// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.app.job.JobInfo;
import android.os.PersistableBundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskInfo.NetworkType;

import java.util.concurrent.TimeUnit;

/** Tests for {@link BackgroundTaskSchedulerJobService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BackgroundTaskSchedulerJobServiceTest {
    private static final long CLOCK_TIME_MS = 1415926535000L;
    private static final long TIME_50_MIN_TO_MS = TimeUnit.MINUTES.toMillis(50);
    private static final long TIME_100_MIN_TO_MS = TimeUnit.MINUTES.toMillis(100);
    private static final long TIME_200_MIN_TO_MS = TimeUnit.MINUTES.toMillis(200);
    private static final long END_TIME_WITH_DEADLINE_MS =
            TIME_200_MIN_TO_MS + BackgroundTaskSchedulerJobService.DEADLINE_DELTA_MS;

    private BackgroundTaskSchedulerJobService.Clock mClock = () -> CLOCK_TIME_MS;

    @Before
    public void setUp() {
        BackgroundTaskSchedulerJobService.setClockForTesting(mClock);
    }

    @Test
    public void testOneOffTaskWithDeadline() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TIME_200_MIN_TO_MS).build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        JobInfo jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(), oneOffTask);
        Assert.assertEquals(oneOffTask.getTaskId(), jobInfo.getId());
        Assert.assertFalse(jobInfo.isPeriodic());
        Assert.assertEquals(TIME_200_MIN_TO_MS, jobInfo.getMaxExecutionDelayMillis());
    }

    @Test
    public void testOneOffTaskWithDeadlineAndExpiration() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowEndTimeMs(TIME_200_MIN_TO_MS)
                        .setExpiresAfterWindowEndTime(true)
                        .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        JobInfo jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(), oneOffTask);
        Assert.assertEquals(END_TIME_WITH_DEADLINE_MS, jobInfo.getMaxExecutionDelayMillis());
        Assert.assertEquals(
                CLOCK_TIME_MS,
                jobInfo.getExtras()
                        .getLong(
                                BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_SCHEDULE_TIME_KEY));
        Assert.assertEquals(
                TIME_200_MIN_TO_MS,
                jobInfo.getExtras()
                        .getLong(BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_END_TIME_KEY));
    }

    @Test
    public void testOneOffTaskWithWindow() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowStartTimeMs(TIME_100_MIN_TO_MS)
                        .setWindowEndTimeMs(TIME_200_MIN_TO_MS)
                        .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        JobInfo jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(), oneOffTask);
        Assert.assertEquals(oneOffTask.getTaskId(), jobInfo.getId());
        Assert.assertFalse(jobInfo.isPeriodic());
        Assert.assertEquals(TIME_100_MIN_TO_MS, jobInfo.getMinLatencyMillis());
        Assert.assertEquals(TIME_200_MIN_TO_MS, jobInfo.getMaxExecutionDelayMillis());
    }

    @Test
    public void testOneOffTaskWithWindowAndExpiration() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowStartTimeMs(TIME_100_MIN_TO_MS)
                        .setWindowEndTimeMs(TIME_200_MIN_TO_MS)
                        .setExpiresAfterWindowEndTime(true)
                        .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        JobInfo jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(), oneOffTask);
        Assert.assertEquals(
                oneOffTask.getOneOffInfo().getWindowStartTimeMs(), jobInfo.getMinLatencyMillis());
        Assert.assertEquals(END_TIME_WITH_DEADLINE_MS, jobInfo.getMaxExecutionDelayMillis());
        Assert.assertEquals(
                CLOCK_TIME_MS,
                jobInfo.getExtras()
                        .getLong(
                                BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_SCHEDULE_TIME_KEY));
        Assert.assertEquals(
                TIME_200_MIN_TO_MS,
                jobInfo.getExtras()
                        .getLong(BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_END_TIME_KEY));
    }

    @Test(expected = AssertionError.class)
    public void testUserInitiatedTaskWithEndTimeThrowsAssertionError() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowStartTimeMs(TIME_100_MIN_TO_MS)
                        .setWindowEndTimeMs(TIME_200_MIN_TO_MS)
                        .build();
        TaskInfo.Builder builder = TaskInfo.createTask(TaskIds.TEST, timingInfo);
        builder.setUserInitiated(true);
        builder.setRequiredNetworkType(NetworkType.ANY);
        TaskInfo oneOffTask = builder.build();
        JobInfo jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(), oneOffTask);
        Assert.assertEquals(oneOffTask.getTaskId(), jobInfo.getId());
        Assert.assertFalse(jobInfo.isPeriodic());
        Assert.assertEquals(NetworkType.ANY, jobInfo.getNetworkType());
        Assert.assertTrue(jobInfo.isUserInitiated());
        Assert.assertEquals(TIME_100_MIN_TO_MS, jobInfo.getMinLatencyMillis());
    }

    @Test
    @MinAndroidSdkLevel(34)
    public void testUserInitiatedTask() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowStartTimeMs(TIME_100_MIN_TO_MS).build();
        TaskInfo.Builder builder = TaskInfo.createTask(TaskIds.TEST, timingInfo);
        builder.setUserInitiated(true);
        builder.setRequiredNetworkType(NetworkType.ANY);
        TaskInfo oneOffTask = builder.build();
        JobInfo jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(), oneOffTask);
        Assert.assertEquals(oneOffTask.getTaskId(), jobInfo.getId());
        Assert.assertFalse(jobInfo.isPeriodic());
        Assert.assertEquals(NetworkType.ANY, jobInfo.getNetworkType());
        // Assert.assertTrue(jobInfo.isUserInitiated());
        Assert.assertEquals(TIME_100_MIN_TO_MS, jobInfo.getMinLatencyMillis());
    }

    @Test
    public void testPeriodicTaskWithoutFlex() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.PeriodicInfo.create().setIntervalMs(TIME_200_MIN_TO_MS).build();
        TaskInfo periodicTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        JobInfo jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(), periodicTask);
        Assert.assertEquals(periodicTask.getTaskId(), jobInfo.getId());
        Assert.assertTrue(jobInfo.isPeriodic());
        Assert.assertEquals(TIME_200_MIN_TO_MS, jobInfo.getIntervalMillis());
    }

    @Test
    public void testPeriodicTaskWithFlex() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.PeriodicInfo.create()
                        .setIntervalMs(TIME_200_MIN_TO_MS)
                        .setFlexMs(TIME_50_MIN_TO_MS)
                        .build();
        TaskInfo periodicTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        JobInfo jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(), periodicTask);
        Assert.assertEquals(TIME_200_MIN_TO_MS, jobInfo.getIntervalMillis());
        Assert.assertEquals(TIME_50_MIN_TO_MS, jobInfo.getFlexMillis());
    }

    @Test
    public void testTaskInfoWithExtras() {
        PersistableBundle taskExtras = new PersistableBundle();
        taskExtras.putString("foo", "bar");
        taskExtras.putBoolean("bools", true);
        taskExtras.putLong("longs", 1342543L);
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TIME_200_MIN_TO_MS).build();
        TaskInfo oneOffTask =
                TaskInfo.createTask(TaskIds.TEST, timingInfo).setExtras(taskExtras).build();
        JobInfo jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(), oneOffTask);
        Assert.assertEquals(oneOffTask.getTaskId(), jobInfo.getId());
        PersistableBundle jobExtras = jobInfo.getExtras();
        PersistableBundle persistableBundle =
                jobExtras.getPersistableBundle(
                        BackgroundTaskSchedulerJobService.BACKGROUND_TASK_EXTRAS_KEY);
        Assert.assertEquals(taskExtras.keySet().size(), persistableBundle.keySet().size());
        Assert.assertEquals(taskExtras.getString("foo"), persistableBundle.getString("foo"));
        Assert.assertEquals(taskExtras.getBoolean("bools"), persistableBundle.getBoolean("bools"));
        Assert.assertEquals(taskExtras.getLong("longs"), persistableBundle.getLong("longs"));
    }

    @Test
    public void testTaskInfoWithManyConstraints() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TIME_200_MIN_TO_MS).build();
        TaskInfo.Builder taskBuilder = TaskInfo.createTask(TaskIds.TEST, timingInfo);

        JobInfo jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(),
                        taskBuilder.setIsPersisted(true).build());
        Assert.assertTrue(jobInfo.isPersisted());

        jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(),
                        taskBuilder.setRequiredNetworkType(TaskInfo.NetworkType.UNMETERED).build());
        Assert.assertEquals(JobInfo.NETWORK_TYPE_UNMETERED, jobInfo.getNetworkType());

        jobInfo =
                BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                        RuntimeEnvironment.getApplication(),
                        taskBuilder.setRequiresCharging(true).build());
        Assert.assertTrue(jobInfo.isRequireCharging());
    }
}
