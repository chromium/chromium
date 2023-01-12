// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskScheduler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundTaskSchedulerImplTest {
    @Mock
    private BackgroundTaskSchedulerDelegate mDelegate;
    @Mock
    private BackgroundTaskSchedulerDelegate mAlarmManagerDelegate;
    @Mock
    private BackgroundTaskSchedulerUma mBackgroundTaskSchedulerUma;

    private TaskInfo mTask;
    private TaskInfo mExpirationTask;
    private TaskInfo mExactTask;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactoryInternal.setSchedulerForTesting(
                new BackgroundTaskSchedulerImpl(mDelegate, mAlarmManagerDelegate));
        BackgroundTaskSchedulerUma.setInstanceForTesting(mBackgroundTaskSchedulerUma);
        TestBackgroundTask.reset();

        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TimeUnit.DAYS.toMillis(1)).build();
        mTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        TaskInfo.TimingInfo expirationTimingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowEndTimeMs(TimeUnit.DAYS.toMillis(1))
                        .setExpiresAfterWindowEndTime(true)
                        .build();
        mExpirationTask = TaskInfo.createTask(TaskIds.TEST, expirationTimingInfo).build();
        TaskInfo.TimingInfo exactTimingInfo =
                TaskInfo.ExactInfo.create().setTriggerAtMs(1415926535000L).build();
        mExactTask = TaskInfo.createTask(TaskIds.TEST, exactTimingInfo).build();

        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskFactory());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testScheduleTaskSuccessful() {
        doReturn(true).when(mDelegate).schedule(eq(RuntimeEnvironment.application), eq(mTask));
        BackgroundTaskSchedulerFactoryInternal.getScheduler().schedule(
                RuntimeEnvironment.application, mTask);
        verify(mDelegate, times(1)).schedule(eq(RuntimeEnvironment.application), eq(mTask));
        verify(mAlarmManagerDelegate, times(0))
                .schedule(eq(RuntimeEnvironment.application), eq(mExactTask));
        verify(mBackgroundTaskSchedulerUma, times(1))
                .reportTaskScheduled(eq(TaskIds.TEST), eq(true));
        verify(mBackgroundTaskSchedulerUma, times(1))
                .reportTaskCreatedAndExpirationState(eq(TaskIds.TEST), eq(false));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testScheduleTaskWithExpirationSuccessful() {
        doReturn(true).when(mDelegate).schedule(
                eq(RuntimeEnvironment.application), eq(mExpirationTask));
        BackgroundTaskSchedulerFactoryInternal.getScheduler().schedule(
                RuntimeEnvironment.application, mExpirationTask);
        verify(mBackgroundTaskSchedulerUma, times(1))
                .reportTaskCreatedAndExpirationState(eq(TaskIds.TEST), eq(true));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testScheduleExactTaskSuccessful() {
        doReturn(true)
                .when(mAlarmManagerDelegate)
                .schedule(eq(RuntimeEnvironment.application), eq(mExactTask));
        BackgroundTaskSchedulerFactoryInternal.getScheduler().schedule(
                RuntimeEnvironment.application, mExactTask);
        verify(mAlarmManagerDelegate, times(1))
                .schedule(eq(RuntimeEnvironment.application), eq(mExactTask));
        verify(mDelegate, times(0)).schedule(eq(RuntimeEnvironment.application), eq(mExactTask));
        verify(mBackgroundTaskSchedulerUma, times(1))
                .reportTaskScheduled(eq(TaskIds.TEST), eq(true));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testScheduleTaskFailed() {
        doReturn(false).when(mDelegate).schedule(eq(RuntimeEnvironment.application), eq(mTask));
        BackgroundTaskSchedulerFactoryInternal.getScheduler().schedule(
                RuntimeEnvironment.application, mTask);
        verify(mDelegate, times(1)).schedule(eq(RuntimeEnvironment.application), eq(mTask));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCancel() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask);

        doNothing().when(mDelegate).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        BackgroundTaskSchedulerFactoryInternal.getScheduler().cancel(
                RuntimeEnvironment.application, TaskIds.TEST);
        verify(mDelegate, times(1)).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        verify(mAlarmManagerDelegate, times(0))
                .cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testIsScheduled() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask);
        assertTrue(BackgroundTaskSchedulerFactoryInternal.getScheduler().isScheduled(
                RuntimeEnvironment.application, TaskIds.TEST));

        doNothing().when(mDelegate).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        BackgroundTaskSchedulerFactoryInternal.getScheduler().cancel(
                RuntimeEnvironment.application, TaskIds.TEST);
        verify(mDelegate, times(1)).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        assertFalse(BackgroundTaskSchedulerFactoryInternal.getScheduler().isScheduled(
                RuntimeEnvironment.application, TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCancelExactTask() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(mExactTask);
        doNothing()
                .when(mAlarmManagerDelegate)
                .cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        BackgroundTaskSchedulerFactoryInternal.getScheduler().cancel(
                RuntimeEnvironment.application, TaskIds.TEST);
        verify(mDelegate, times(0)).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        verify(mAlarmManagerDelegate, times(1))
                .cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testRescheduleTasks() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask);

        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
        assertFalse(BackgroundTaskSchedulerPrefs.getScheduledTaskIds().isEmpty());
        BackgroundTaskSchedulerFactoryInternal.getScheduler().reschedule(
                RuntimeEnvironment.application);

        assertEquals(1, TestBackgroundTask.getRescheduleCalls());
        assertTrue(BackgroundTaskSchedulerPrefs.getScheduledTaskIds().isEmpty());

        verify(mDelegate, times(0)).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        verify(mAlarmManagerDelegate, times(0))
                .cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
    }
}
