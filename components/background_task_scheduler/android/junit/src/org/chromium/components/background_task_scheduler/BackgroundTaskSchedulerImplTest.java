// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Build;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.gcm.GcmNetworkManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.gms.Shadows;
import org.robolectric.shadows.gms.common.ShadowGoogleApiAvailability;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskScheduler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowGcmNetworkManager.class, ShadowGoogleApiAvailability.class})
public class BackgroundTaskSchedulerImplTest {
    @Mock
    private BackgroundTaskSchedulerDelegate mDelegate;
    @Mock
    private BackgroundTaskSchedulerDelegate mAlarmManagerDelegate;
    @Mock
    private BackgroundTaskSchedulerUma mBackgroundTaskSchedulerUma;
    private ShadowGcmNetworkManager mGcmNetworkManager;

    private TaskInfo mTask;
    private TaskInfo mExpirationTask;
    private TaskInfo mExactTask;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(
                new BackgroundTaskSchedulerImpl(mDelegate, mAlarmManagerDelegate));
        BackgroundTaskSchedulerUma.setInstanceForTesting(mBackgroundTaskSchedulerUma);
        TestBackgroundTask.reset();

        // Initialize Google Play Services and GCM Network Manager for upgrade testing.
        Shadows.shadowOf(GoogleApiAvailability.getInstance())
                .setIsGooglePlayServicesAvailable(ConnectionResult.SUCCESS);
        mGcmNetworkManager = (ShadowGcmNetworkManager) Shadow.extract(
                GcmNetworkManager.getInstance(ContextUtils.getApplicationContext()));

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

        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(new TestBackgroundTaskFactory());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testScheduleTaskSuccessful() {
        doReturn(true).when(mDelegate).schedule(eq(RuntimeEnvironment.application), eq(mTask));
        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                RuntimeEnvironment.application, mTask);
        assertEquals(mTask.getBackgroundTaskClass(),
                BackgroundTaskSchedulerFactory.getBackgroundTaskFromTaskId(mTask.getTaskId())
                        .getClass());
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
        BackgroundTaskSchedulerFactory.getScheduler().schedule(
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
        BackgroundTaskSchedulerFactory.getScheduler().schedule(
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
        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                RuntimeEnvironment.application, mTask);
        assertEquals(mTask.getBackgroundTaskClass(),
                BackgroundTaskSchedulerFactory.getBackgroundTaskFromTaskId(mTask.getTaskId())
                        .getClass());
        verify(mDelegate, times(1)).schedule(eq(RuntimeEnvironment.application), eq(mTask));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCancel() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask);

        doNothing().when(mDelegate).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        BackgroundTaskSchedulerFactory.getScheduler().cancel(
                RuntimeEnvironment.application, TaskIds.TEST);
        assertEquals(mTask.getBackgroundTaskClass(),
                BackgroundTaskSchedulerFactory.getBackgroundTaskFromTaskId(mTask.getTaskId())
                        .getClass());
        verify(mDelegate, times(1)).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        verify(mAlarmManagerDelegate, times(0))
                .cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCancelExactTask() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(mExactTask);

        doNothing()
                .when(mAlarmManagerDelegate)
                .cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        BackgroundTaskSchedulerFactory.getScheduler().cancel(
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
        BackgroundTaskSchedulerFactory.getScheduler().reschedule(RuntimeEnvironment.application);

        assertEquals(1, TestBackgroundTask.getRescheduleCalls());
        assertTrue(BackgroundTaskSchedulerPrefs.getScheduledTaskIds().isEmpty());

        verify(mDelegate, times(0)).cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
        verify(mAlarmManagerDelegate, times(0))
                .cancel(eq(RuntimeEnvironment.application), eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCheckForOSUpgrade_PreMToMPlus() {
        BackgroundTaskSchedulerPrefs.setLastSdkVersion(Build.VERSION_CODES.LOLLIPOP);
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask);
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.M);

        BackgroundTaskSchedulerFactory.getScheduler().checkForOSUpgrade(
                RuntimeEnvironment.application);

        assertEquals(Build.VERSION_CODES.M, BackgroundTaskSchedulerPrefs.getLastSdkVersion());
        assertTrue(mGcmNetworkManager.getCanceledTaskTags().contains(
                Integer.toString(mTask.getTaskId())));
        assertEquals(1, TestBackgroundTask.getRescheduleCalls());
    }

    /** This scenario tests upgrade from pre-M to pre-M OS, which requires no rescheduling. */
    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCheckForOSUpgrade_PreMToPreM() {
        BackgroundTaskSchedulerPrefs.setLastSdkVersion(Build.VERSION_CODES.KITKAT);
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask);
        ReflectionHelpers.setStaticField(
                Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.LOLLIPOP);

        BackgroundTaskSchedulerFactory.getScheduler().checkForOSUpgrade(
                RuntimeEnvironment.application);

        assertEquals(
                Build.VERSION_CODES.LOLLIPOP, BackgroundTaskSchedulerPrefs.getLastSdkVersion());
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    /** This scenario tests upgrade from M+ to M+ OS, which requires no rescheduling. */
    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCheckForOSUpgrade_MPlusToMPlus() {
        BackgroundTaskSchedulerPrefs.setLastSdkVersion(Build.VERSION_CODES.M);
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask);
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.N);

        BackgroundTaskSchedulerFactory.getScheduler().checkForOSUpgrade(
                RuntimeEnvironment.application);

        assertEquals(Build.VERSION_CODES.N, BackgroundTaskSchedulerPrefs.getLastSdkVersion());
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }
}
