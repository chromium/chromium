// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
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
    @Mock private BackgroundTaskSchedulerDelegate mDelegate;
    @Mock private BackgroundTaskSchedulerUma mBackgroundTaskSchedulerUma;

    private TaskInfo mTask;
    private TaskInfo mExpirationTask;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactoryInternal.setSchedulerForTesting(
                new BackgroundTaskSchedulerImpl(mDelegate));
        BackgroundTaskSchedulerUma.setInstanceForTesting(mBackgroundTaskSchedulerUma);

        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TimeUnit.DAYS.toMillis(1)).build();
        mTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        TaskInfo.TimingInfo expirationTimingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowEndTimeMs(TimeUnit.DAYS.toMillis(1))
                        .setExpiresAfterWindowEndTime(true)
                        .build();
        mExpirationTask = TaskInfo.createTask(TaskIds.TEST, expirationTimingInfo).build();

        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskFactory());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testScheduleTaskSuccessful() {
        doReturn(true).when(mDelegate).schedule(eq(RuntimeEnvironment.application), eq(mTask));
        BackgroundTaskSchedulerFactoryInternal.getScheduler()
                .schedule(RuntimeEnvironment.application, mTask);
        verify(mDelegate, times(1)).schedule(eq(RuntimeEnvironment.application), eq(mTask));
        verify(mBackgroundTaskSchedulerUma, times(1))
                .reportTaskScheduled(eq(TaskIds.TEST), eq(true));
        verify(mBackgroundTaskSchedulerUma, times(1))
                .reportTaskCreatedAndExpirationState(eq(TaskIds.TEST), eq(false));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testScheduleTaskWithExpirationSuccessful() {
        doReturn(true)
                .when(mDelegate)
                .schedule(eq(RuntimeEnvironment.application), eq(mExpirationTask));
        BackgroundTaskSchedulerFactoryInternal.getScheduler()
                .schedule(RuntimeEnvironment.application, mExpirationTask);
        verify(mBackgroundTaskSchedulerUma, times(1))
                .reportTaskCreatedAndExpirationState(eq(TaskIds.TEST), eq(true));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testScheduleTaskFailed() {
        doReturn(false).when(mDelegate).schedule(eq(RuntimeEnvironment.application), eq(mTask));
        BackgroundTaskSchedulerFactoryInternal.getScheduler()
                .schedule(RuntimeEnvironment.application, mTask);
        verify(mDelegate, times(1)).schedule(eq(RuntimeEnvironment.application), eq(mTask));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCancel() {
        doNothing().when(mDelegate).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        BackgroundTaskSchedulerFactoryInternal.getScheduler()
                .cancel(RuntimeEnvironment.application, TaskIds.TEST);
        verify(mDelegate, times(1)).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
    }
}
