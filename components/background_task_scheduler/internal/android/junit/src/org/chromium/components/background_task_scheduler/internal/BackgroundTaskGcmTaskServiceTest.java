// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.os.Build;
import android.os.Bundle;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.TaskParams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskGcmTaskService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundTaskGcmTaskServiceTest {
    static TestBackgroundTaskWithParams sLastTask;
    static boolean sReturnThroughCallback;
    static boolean sNeedsRescheduling;
    private static BackgroundTaskSchedulerGcmNetworkManager.Clock sClock = () -> 1415926535000L;
    private static BackgroundTaskSchedulerGcmNetworkManager.Clock sZeroClock = () -> 0L;
    @Mock
    private BackgroundTaskSchedulerDelegate mDelegate;
    @Mock
    private BackgroundTaskSchedulerDelegate mAlarmManagerDelegate;
    @Mock
    private BackgroundTaskSchedulerUma mBackgroundTaskSchedulerUma;
    @Mock
    private BackgroundTaskSchedulerImpl mBackgroundTaskSchedulerImpl;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactoryInternal.setSchedulerForTesting(
                new BackgroundTaskSchedulerImpl(mDelegate, mAlarmManagerDelegate));
        BackgroundTaskSchedulerUma.setInstanceForTesting(mBackgroundTaskSchedulerUma);
        sReturnThroughCallback = false;
        sNeedsRescheduling = false;
        sLastTask = null;
        TestBackgroundTask.reset();
    }

    private static class TestBackgroundTaskWithParams extends TestBackgroundTask {
        private TaskParameters mTaskParameters;

        public TestBackgroundTaskWithParams() {}

        @Override
        public boolean onStartTask(
                Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
            mTaskParameters = taskParameters;
            callback.taskFinished(sNeedsRescheduling);
            sLastTask = this;
            return sReturnThroughCallback;
        }

        public TaskParameters getTaskParameters() {
            return mTaskParameters;
        }
    }

    private static class TestBackgroundTaskWithParamsFactory implements BackgroundTaskFactory {
        @Override
        public BackgroundTask getBackgroundTaskFromTaskId(int taskId) {
            return new TestBackgroundTaskWithParams();
        }
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testStartsAnytimeWithoutDeadline() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());

        Bundle taskExtras = new Bundle();
        taskExtras.putString("foo", "bar");
        TaskParams taskParams = buildOneOffTaskParams(TaskIds.TEST, taskExtras, null);

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        assertEquals(GcmNetworkManager.RESULT_SUCCESS, taskService.onRunTask(taskParams));

        assertNotNull(sLastTask);
        TaskParameters parameters = sLastTask.getTaskParameters();

        assertEquals(TaskIds.TEST, parameters.getTaskId());
        assertEquals("bar", parameters.getExtras().getString("foo"));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskDoesNotStartExactlyAtDeadline() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());

        TaskParams taskParams = buildOneOffTaskParams(TaskIds.TEST, new Bundle(), new Long(0));

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        taskService.setClockForTesting(sClock);
        assertEquals(GcmNetworkManager.RESULT_FAILURE, taskService.onRunTask(taskParams));

        assertNull(sLastTask);

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskDoesNotStartAfterDeadline() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());

        TaskParams taskParams =
                buildOneOffTaskParams(TaskIds.TEST, new Bundle(), -sClock.currentTimeMillis());

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        taskService.setClockForTesting(sClock);
        assertEquals(GcmNetworkManager.RESULT_FAILURE, taskService.onRunTask(taskParams));

        assertNull(sLastTask);

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskStartsBeforeDeadline() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());

        TaskParams taskParams =
                buildOneOffTaskParams(TaskIds.TEST, new Bundle(), sClock.currentTimeMillis());

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        taskService.setClockForTesting(sZeroClock);
        assertEquals(GcmNetworkManager.RESULT_SUCCESS, taskService.onRunTask(taskParams));

        assertNotNull(sLastTask);
        TaskParameters parameters = sLastTask.getTaskParameters();

        assertEquals(TaskIds.TEST, parameters.getTaskId());

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskOnRuntaskNeedsReschedulingFromCallback() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());
        sReturnThroughCallback = true;
        sNeedsRescheduling = true;
        TaskParams taskParams = buildOneOffTaskParams(TaskIds.TEST, new Bundle(), null);

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        assertEquals(GcmNetworkManager.RESULT_RESCHEDULE, taskService.onRunTask(taskParams));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskOnRuntaskDontRescheduleFromCallback() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());

        sReturnThroughCallback = true;
        sNeedsRescheduling = false;
        TaskParams taskParams = buildOneOffTaskParams(TaskIds.TEST, new Bundle(), null);

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        assertEquals(GcmNetworkManager.RESULT_SUCCESS, taskService.onRunTask(taskParams));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskOnInitializeTasksOnPreM() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskFactory());

        ReflectionHelpers.setStaticField(
                Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.LOLLIPOP);
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TimeUnit.DAYS.toMillis(1)).build();
        TaskInfo task = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(task);
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());

        new BackgroundTaskGcmTaskService().onInitializeTasks();
        assertEquals(1, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskOnInitializeTasksOnMPlus() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskFactory());

        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.M);
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TimeUnit.DAYS.toMillis(1)).build();
        TaskInfo task = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(task);
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());

        new BackgroundTaskGcmTaskService().onInitializeTasks();
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCancelTaskIfTaskIdNotFound() {
        BackgroundTaskSchedulerFactoryInternal.setSchedulerForTesting(mBackgroundTaskSchedulerImpl);

        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskFactory());

        TaskParams taskParams = buildOneOffTaskParams(
                TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID, new Bundle(), sClock.currentTimeMillis());

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        taskService.setClockForTesting(sZeroClock);
        assertEquals(GcmNetworkManager.RESULT_FAILURE, taskService.onRunTask(taskParams));

        verify(mBackgroundTaskSchedulerImpl, times(1))
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskStartsAnytimeWithoutDeadline() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskFactory());

        TaskParams taskParams =
                buildPeriodicTaskParams(TaskIds.TEST, new Bundle(), null, null, null);

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        assertEquals(GcmNetworkManager.RESULT_SUCCESS, taskService.onRunTask(taskParams));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskStartsWithinDeadlineTimeFrame() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskFactory());

        TaskParams taskParams = buildPeriodicTaskParams(TaskIds.TEST, new Bundle(),
                sClock.currentTimeMillis() - TimeUnit.MINUTES.toMillis(14),
                TimeUnit.MINUTES.toMillis(15), null);

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        taskService.setClockForTesting(sClock);
        assertEquals(GcmNetworkManager.RESULT_SUCCESS, taskService.onRunTask(taskParams));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskDoesNotStartExactlyAtDeadline() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskFactory());

        TaskParams taskParams = buildPeriodicTaskParams(TaskIds.TEST, new Bundle(),
                sClock.currentTimeMillis(), TimeUnit.MINUTES.toMillis(15), null);

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        taskService.setClockForTesting(sClock);
        assertEquals(GcmNetworkManager.RESULT_FAILURE, taskService.onRunTask(taskParams));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskDoesNotStartAfterDeadline() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());

        TaskParams taskParams = buildPeriodicTaskParams(TaskIds.TEST, new Bundle(),
                sClock.currentTimeMillis() - TimeUnit.MINUTES.toMillis(3),
                TimeUnit.MINUTES.toMillis(15), null);

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        taskService.setClockForTesting(sClock);
        assertEquals(GcmNetworkManager.RESULT_FAILURE, taskService.onRunTask(taskParams));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
    }

    private static TaskParams buildOneOffTaskParams(
            int taskId, Bundle taskExtras, Long windowEndTimeForDeadlineMs) {
        Bundle extras = new Bundle();
        extras.putBundle(
                BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_EXTRAS_KEY, taskExtras);
        if (windowEndTimeForDeadlineMs != null) {
            extras.putLong(BackgroundTaskSchedulerJobService.BACKGROUND_TASK_SCHEDULE_TIME_KEY,
                    sClock.currentTimeMillis());
            extras.putLong(BackgroundTaskSchedulerJobService.BACKGROUND_TASK_END_TIME_KEY,
                    windowEndTimeForDeadlineMs);
        }

        return new TaskParams(Integer.toString(taskId), extras);
    }

    private static TaskParams buildPeriodicTaskParams(int taskId, Bundle taskExtras,
            Long schedulingTimeMs, Long intervalForDeadlineMs, Long flexForDeadlineMs) {
        Bundle extras = new Bundle();
        extras.putBundle(
                BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_EXTRAS_KEY, taskExtras);
        if (schedulingTimeMs != null) {
            extras.putLong(
                    BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_SCHEDULE_TIME_KEY,
                    schedulingTimeMs);
            extras.putLong(
                    BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_INTERVAL_TIME_KEY,
                    intervalForDeadlineMs);
            if (flexForDeadlineMs != null) {
                extras.putLong(
                        BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_FLEX_TIME_KEY,
                        flexForDeadlineMs);
            }
        }

        return new TaskParams(Integer.toString(taskId), extras);
    }
}
