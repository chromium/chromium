// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.job.JobParameters;
import android.content.Context;
import android.os.Build;
import android.os.PersistableBundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskJobService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.S)
public class BackgroundTaskJobServiceTest {
    private static BackgroundTaskSchedulerJobService.Clock sClock = () -> 1415926535000L;
    private static BackgroundTaskSchedulerJobService.Clock sZeroClock = () -> 0L;
    @Mock private BackgroundTaskSchedulerDelegate mDelegate;
    @Mock private BackgroundTaskSchedulerUma mBackgroundTaskSchedulerUma;
    @Mock private BackgroundTaskSchedulerImpl mBackgroundTaskSchedulerImpl;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactoryInternal.setSchedulerForTesting(
                new BackgroundTaskSchedulerImpl(mDelegate));
        BackgroundTaskSchedulerUma.setInstanceForTesting(mBackgroundTaskSchedulerUma);
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskFactory());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskStartsAnytimeWithoutDeadline() {
        JobParameters jobParameters = buildOneOffJobParameters(TaskIds.TEST, null, null);

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskDoesNotStartExactlyAtDeadline() {
        JobParameters jobParameters =
                buildOneOffJobParameters(TaskIds.TEST, sClock.currentTimeMillis(), Long.valueOf(0));

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskDoesNotStartAfterDeadline() {
        JobParameters jobParameters =
                buildOneOffJobParameters(
                        TaskIds.TEST, sZeroClock.currentTimeMillis(), Long.valueOf(0));

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskStartsBeforeDeadline() {
        JobParameters jobParameters =
                buildOneOffJobParameters(
                        TaskIds.TEST, sClock.currentTimeMillis(), sClock.currentTimeMillis());

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sZeroClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCancelOneOffTaskIfTaskIdNotFound() {
        BackgroundTaskSchedulerFactoryInternal.setSchedulerForTesting(mBackgroundTaskSchedulerImpl);

        JobParameters jobParameters =
                buildOneOffJobParameters(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID, null, null);

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sZeroClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerImpl, times(1))
                .cancel(
                        eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskStartsAnytimeWithoutDeadline() {
        JobParameters jobParameters = buildPeriodicJobParameters(TaskIds.TEST, null, null, null);

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskStartsWithinDeadlineTimeFrame() {
        JobParameters jobParameters =
                buildPeriodicJobParameters(
                        TaskIds.TEST,
                        sClock.currentTimeMillis() - TimeUnit.MINUTES.toMillis(13),
                        TimeUnit.MINUTES.toMillis(15),
                        null);

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskDoesNotStartExactlyAtDeadline() {
        JobParameters jobParameters =
                buildPeriodicJobParameters(
                        TaskIds.TEST,
                        sClock.currentTimeMillis(),
                        TimeUnit.MINUTES.toMillis(15),
                        null);

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskDoesNotStartAfterDeadline() {
        JobParameters jobParameters =
                buildPeriodicJobParameters(
                        TaskIds.TEST,
                        sClock.currentTimeMillis() - TimeUnit.MINUTES.toMillis(3),
                        TimeUnit.MINUTES.toMillis(15),
                        null);

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
    }

    private static JobParameters newJobParameters(int jobId, PersistableBundle extras) {
        JobParameters ret = mock(JobParameters.class);
        when(ret.getJobId()).thenReturn(jobId);
        when(ret.getExtras()).thenReturn(extras);
        return ret;
    }

    private static JobParameters buildOneOffJobParameters(
            int taskId, Long schedulingTimeMs, Long windowEndTimeForDeadlineMs) {
        PersistableBundle extras = new PersistableBundle();
        if (schedulingTimeMs != null) {
            extras.putLong(
                    BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_SCHEDULE_TIME_KEY,
                    schedulingTimeMs);
        }
        if (windowEndTimeForDeadlineMs != null) {
            extras.putLong(
                    BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_END_TIME_KEY,
                    windowEndTimeForDeadlineMs);
        }
        PersistableBundle taskExtras = new PersistableBundle();
        extras.putPersistableBundle(
                BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_EXTRAS_KEY, taskExtras);

        return newJobParameters(taskId, extras);
    }

    private static JobParameters buildPeriodicJobParameters(
            int taskId, Long schedulingTimeMs, Long intervalForDeadlineMs, Long flexForDeadlineMs) {
        PersistableBundle extras = new PersistableBundle();
        if (schedulingTimeMs != null) {
            extras.putLong(
                    BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_SCHEDULE_TIME_KEY,
                    schedulingTimeMs);
            extras.putLong(
                    BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_INTERVAL_TIME_KEY,
                    intervalForDeadlineMs);
            if (flexForDeadlineMs != null) {
                extras.putLong(
                        BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_FLEX_TIME_KEY,
                        flexForDeadlineMs);
            }
        }
        PersistableBundle taskExtras = new PersistableBundle();
        extras.putPersistableBundle(
                BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_EXTRAS_KEY, taskExtras);

        return newJobParameters(taskId, extras);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testSetNotification() {
        FakeBackgroundTask fakeBackgroundTask = new FakeBackgroundTask();
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new FakeBackgroundTaskFactory(fakeBackgroundTask));
        JobParameters jobParameters =
                buildOneOffJobParameters(
                        TaskIds.TEST, sClock.currentTimeMillis(), Long.valueOf(1000));

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertTrue("onStartJob() didn't return true", jobService.onStartJob(jobParameters));
        fakeBackgroundTask.mTaskFinishedCallback.setNotification(22, null);
        fakeBackgroundTask.mTaskFinishedCallback.taskFinished(true);

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
        verify(mBackgroundTaskSchedulerUma, times(1))
                .reportNotificationWasSet(eq(TaskIds.TEST), anyLong());
        verify(mBackgroundTaskSchedulerUma, times(1))
                .reportTaskFinished(eq(TaskIds.TEST), anyLong());
    }

    public static class FakeBackgroundTaskFactory implements BackgroundTaskFactory {
        private BackgroundTask mFakeBackgroundTask;

        FakeBackgroundTaskFactory(BackgroundTask fakeBackgroundTask) {
            mFakeBackgroundTask = fakeBackgroundTask;
        }

        @Override
        public BackgroundTask getBackgroundTaskFromTaskId(int taskId) {
            if (taskId == TaskIds.TEST) {
                return mFakeBackgroundTask;
            }
            return null;
        }
    }

    private static class FakeBackgroundTask implements BackgroundTask {
        private TaskFinishedCallback mTaskFinishedCallback;

        @Override
        public boolean onStartTask(
                Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
            mTaskFinishedCallback = callback;
            return true;
        }

        @Override
        public boolean onStopTask(Context context, TaskParameters taskParameters) {
            return false;
        }
    }
}
