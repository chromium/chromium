// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.job.JobParameters;
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
import org.chromium.components.background_task_scheduler.TaskIds;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskJobService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundTaskJobServiceTest {
    private static BackgroundTaskSchedulerJobService.Clock sClock = () -> 1415926535000L;
    private static BackgroundTaskSchedulerJobService.Clock sZeroClock = () -> 0L;
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
        TestBackgroundTask.reset();
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
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskDoesNotStartExactlyAtDeadline() {
        JobParameters jobParameters =
                buildOneOffJobParameters(TaskIds.TEST, sClock.currentTimeMillis(), new Long(0));

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskDoesNotStartAfterDeadline() {
        JobParameters jobParameters =
                buildOneOffJobParameters(TaskIds.TEST, sZeroClock.currentTimeMillis(), new Long(0));

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskStartsBeforeDeadline() {
        JobParameters jobParameters = buildOneOffJobParameters(
                TaskIds.TEST, sClock.currentTimeMillis(), sClock.currentTimeMillis());

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sZeroClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
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
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskStartsAnytimeWithoutDeadline() {
        JobParameters jobParameters = buildPeriodicJobParameters(TaskIds.TEST, null, null, null);

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskStartsWithinDeadlineTimeFrame() {
        JobParameters jobParameters = buildPeriodicJobParameters(TaskIds.TEST,
                sClock.currentTimeMillis() - TimeUnit.MINUTES.toMillis(13),
                TimeUnit.MINUTES.toMillis(15), null);

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskDoesNotStartExactlyAtDeadline() {
        JobParameters jobParameters = buildPeriodicJobParameters(
                TaskIds.TEST, sClock.currentTimeMillis(), TimeUnit.MINUTES.toMillis(15), null);

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskDoesNotStartAfterDeadline() {
        JobParameters jobParameters = buildPeriodicJobParameters(TaskIds.TEST,
                sClock.currentTimeMillis() - TimeUnit.MINUTES.toMillis(3),
                TimeUnit.MINUTES.toMillis(15), null);

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    private static JobParameters buildOneOffJobParameters(
            int taskId, Long schedulingTimeMs, Long windowEndTimeForDeadlineMs) {
        PersistableBundle extras = new PersistableBundle();
        if (windowEndTimeForDeadlineMs != null) {
            extras.putLong(BackgroundTaskSchedulerJobService.BACKGROUND_TASK_SCHEDULE_TIME_KEY,
                    schedulingTimeMs);
            extras.putLong(BackgroundTaskSchedulerJobService.BACKGROUND_TASK_END_TIME_KEY,
                    windowEndTimeForDeadlineMs);
        }
        PersistableBundle taskExtras = new PersistableBundle();
        extras.putPersistableBundle(
                BackgroundTaskSchedulerJobService.BACKGROUND_TASK_EXTRAS_KEY, taskExtras);

        return new JobParameters(null /* callback */, taskId, extras, null /* transientExtras */,
                null /* clipData */, 0 /* clipGrantFlags */, false /* overrideDeadlineExpired */,
                null /* triggeredContentUris */, null /* triggeredContentAuthorities */,
                null /* network */);
    }

    private static JobParameters buildPeriodicJobParameters(
            int taskId, Long schedulingTimeMs, Long intervalForDeadlineMs, Long flexForDeadlineMs) {
        PersistableBundle extras = new PersistableBundle();
        if (schedulingTimeMs != null) {
            extras.putLong(BackgroundTaskSchedulerJobService.BACKGROUND_TASK_SCHEDULE_TIME_KEY,
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
        PersistableBundle taskExtras = new PersistableBundle();
        extras.putPersistableBundle(
                BackgroundTaskSchedulerJobService.BACKGROUND_TASK_EXTRAS_KEY, taskExtras);

        return new JobParameters(null /* callback */, taskId, extras, null /* transientExtras */,
                null /* clipData */, 0 /* clipGrantFlags */, false /* overrideDeadlineExpired */,
                null /* triggeredContentUris */, null /* triggeredContentAuthorities */,
                null /* network */);
    }
}