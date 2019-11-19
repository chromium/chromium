// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.OneoffTask;
import com.google.android.gms.gcm.PeriodicTask;
import com.google.android.gms.gcm.Task;
import com.google.android.gms.gcm.TaskParams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.gms.Shadows;
import org.robolectric.shadows.gms.common.ShadowGoogleApiAvailability;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskSchedulerGcmNetworkManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowGcmNetworkManager.class, ShadowGoogleApiAvailability.class})
public class BackgroundTaskSchedulerGcmNetworkManagerTest {
    ShadowGcmNetworkManager mGcmNetworkManager;

    private static final long CLOCK_TIME_MS = 1415926535000L;
    private static final long TIME_100_MIN_TO_MS = TimeUnit.MINUTES.toMillis(100);
    private static final long TIME_100_MIN_TO_S = TimeUnit.MINUTES.toSeconds(100);
    private static final long TIME_200_MIN_TO_MS = TimeUnit.MINUTES.toMillis(200);
    private static final long TIME_200_MIN_TO_S = TimeUnit.MINUTES.toSeconds(200);
    private static final long TIME_24_H_TO_MS = TimeUnit.HOURS.toMillis(1);
    private static final long END_TIME_WITH_DEADLINE_S =
            (TIME_200_MIN_TO_MS + BackgroundTaskSchedulerGcmNetworkManager.DEADLINE_DELTA_MS)
            / 1000;

    private BackgroundTaskSchedulerGcmNetworkManager.Clock mClock = () -> CLOCK_TIME_MS;

    @Before
    public void setUp() {
        Shadows.shadowOf(GoogleApiAvailability.getInstance())
                .setIsGooglePlayServicesAvailable(ConnectionResult.SUCCESS);
        mGcmNetworkManager = (ShadowGcmNetworkManager) Shadow.extract(
                GcmNetworkManager.getInstance(ContextUtils.getApplicationContext()));
        BackgroundTaskSchedulerGcmNetworkManager.setClockForTesting(mClock);
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(new TestBackgroundTaskFactory());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskWithDeadline() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TIME_200_MIN_TO_MS).build();
        TaskInfo oneOffTaskInfo = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo);
        assertTrue(task instanceof OneoffTask);
        OneoffTask oneOffTask = (OneoffTask) task;
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertEquals(TIME_200_MIN_TO_S, oneOffTask.getWindowEnd());
        assertEquals(0, oneOffTask.getWindowStart());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskWithDeadlineAndExpiration() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.OneOffInfo.create()
                                                 .setWindowEndTimeMs(TIME_200_MIN_TO_MS)
                                                 .setExpiresAfterWindowEndTime(true)
                                                 .build();
        TaskInfo oneOffTaskInfo = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo);
        assertTrue(task instanceof OneoffTask);
        OneoffTask oneOffTask = (OneoffTask) task;
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertEquals(END_TIME_WITH_DEADLINE_S, oneOffTask.getWindowEnd());
        assertEquals(0, oneOffTask.getWindowStart());
        assertEquals(CLOCK_TIME_MS,
                task.getExtras().getLong(BackgroundTaskSchedulerGcmNetworkManager
                                                 .BACKGROUND_TASK_SCHEDULE_TIME_KEY));
        assertEquals(TIME_200_MIN_TO_MS,
                task.getExtras().getLong(
                        BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_END_TIME_KEY));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskWithWindow() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.OneOffInfo.create()
                                                 .setWindowStartTimeMs(TIME_100_MIN_TO_MS)
                                                 .setWindowEndTimeMs(TIME_200_MIN_TO_MS)
                                                 .build();
        TaskInfo oneOffTaskInfo = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo);
        assertTrue(task instanceof OneoffTask);
        OneoffTask oneOffTask = (OneoffTask) task;
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertEquals(TIME_200_MIN_TO_S, oneOffTask.getWindowEnd());
        assertEquals(TIME_100_MIN_TO_S, oneOffTask.getWindowStart());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskWithWindowAndExpiration() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.OneOffInfo.create()
                                                 .setWindowStartTimeMs(TIME_100_MIN_TO_MS)
                                                 .setWindowEndTimeMs(TIME_200_MIN_TO_MS)
                                                 .setExpiresAfterWindowEndTime(true)
                                                 .build();
        TaskInfo oneOffTaskInfo = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo);
        assertTrue(task instanceof OneoffTask);
        OneoffTask oneOffTask = (OneoffTask) task;
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertEquals(END_TIME_WITH_DEADLINE_S, oneOffTask.getWindowEnd());
        assertEquals(TIME_100_MIN_TO_S, oneOffTask.getWindowStart());
        assertEquals(CLOCK_TIME_MS,
                task.getExtras().getLong(BackgroundTaskSchedulerGcmNetworkManager
                                                 .BACKGROUND_TASK_SCHEDULE_TIME_KEY));
        assertEquals(TIME_200_MIN_TO_MS,
                task.getExtras().getLong(
                        BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_END_TIME_KEY));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskWithoutFlex() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.PeriodicInfo.create().setIntervalMs(TIME_200_MIN_TO_MS).build();
        TaskInfo periodicTaskInfo = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        Task task =
                BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(periodicTaskInfo);
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertTrue(task instanceof PeriodicTask);
        PeriodicTask periodicTask = (PeriodicTask) task;
        assertEquals(TIME_200_MIN_TO_S, periodicTask.getPeriod());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskWithFlex() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.PeriodicInfo.create()
                                                 .setIntervalMs(TIME_200_MIN_TO_MS)
                                                 .setFlexMs(TimeUnit.MINUTES.toMillis(50))
                                                 .build();
        TaskInfo periodicTaskInfo = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        Task task =
                BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(periodicTaskInfo);
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertTrue(task instanceof PeriodicTask);
        PeriodicTask periodicTask = (PeriodicTask) task;
        assertEquals(TIME_200_MIN_TO_S, periodicTask.getPeriod());
        assertEquals(TimeUnit.MINUTES.toSeconds(50), periodicTask.getFlex());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskInfoWithExtras() {
        Bundle userExtras = new Bundle();
        userExtras.putString("foo", "bar");
        userExtras.putBoolean("bools", true);
        userExtras.putLong("longs", 1342543L);
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TIME_200_MIN_TO_MS).build();
        TaskInfo oneOffTaskInfo =
                TaskInfo.createTask(TaskIds.TEST, timingInfo).setExtras(userExtras).build();
        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo);
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertTrue(task instanceof OneoffTask);

        Bundle taskExtras = task.getExtras();
        Bundle bundle = taskExtras.getBundle(
                BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_EXTRAS_KEY);
        assertEquals(userExtras.keySet().size(), bundle.keySet().size());
        assertEquals(userExtras.getString("foo"), bundle.getString("foo"));
        assertEquals(userExtras.getBoolean("bools"), bundle.getBoolean("bools"));
        assertEquals(userExtras.getLong("longs"), bundle.getLong("longs"));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskInfoWithExtras() {
        Bundle userExtras = new Bundle();
        userExtras.putString("foo", "bar");
        userExtras.putBoolean("bools", true);
        userExtras.putLong("longs", 1342543L);
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.PeriodicInfo.create().setIntervalMs(TIME_200_MIN_TO_MS).build();
        TaskInfo periodicTaskInfo =
                TaskInfo.createTask(TaskIds.TEST, timingInfo).setExtras(userExtras).build();
        Task task =
                BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(periodicTaskInfo);
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertTrue(task instanceof PeriodicTask);

        Bundle taskExtras = task.getExtras();
        Bundle bundle = taskExtras.getBundle(
                BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_EXTRAS_KEY);
        assertEquals(userExtras.keySet().size(), bundle.keySet().size());
        assertEquals(userExtras.getString("foo"), bundle.getString("foo"));
        assertEquals(userExtras.getBoolean("bools"), bundle.getBoolean("bools"));
        assertEquals(userExtras.getLong("longs"), bundle.getLong("longs"));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testTaskInfoWithManyConstraints() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TIME_200_MIN_TO_MS).build();
        TaskInfo.Builder taskBuilder = TaskInfo.createTask(TaskIds.TEST, timingInfo);

        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(
                taskBuilder.setIsPersisted(true).build());
        assertTrue(task.isPersisted());

        task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(
                taskBuilder.setRequiredNetworkType(TaskInfo.NetworkType.UNMETERED).build());
        assertEquals(Task.NETWORK_STATE_UNMETERED, task.getRequiredNetwork());

        task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(
                taskBuilder.setRequiresCharging(true).build());
        assertTrue(task.getRequiresCharging());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testGetTaskParametersFromTaskParams() {
        Bundle extras = new Bundle();
        Bundle taskExtras = new Bundle();
        taskExtras.putString("foo", "bar");
        extras.putBundle(
                BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_EXTRAS_KEY, taskExtras);

        TaskParams params = new TaskParams(Integer.toString(TaskIds.TEST), extras);

        TaskParameters parameters =
                BackgroundTaskSchedulerGcmNetworkManager.getTaskParametersFromTaskParams(params);
        assertEquals(TaskIds.TEST, parameters.getTaskId());
        assertEquals("bar", parameters.getExtras().getString("foo"));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testGetBackgroundTaskFromTaskParams() {
        TaskParams params = new TaskParams(Integer.toString(TaskIds.TEST), new Bundle());
        BackgroundTask backgroundTask = BackgroundTaskSchedulerFactory.getBackgroundTaskFromTaskId(
                Integer.valueOf(params.getTag()));

        assertNotNull(backgroundTask);
        assertTrue(backgroundTask instanceof TestBackgroundTask);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testSchedule() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TIME_24_H_TO_MS).build();
        TaskInfo oneOffTaskInfo = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        assertNull(mGcmNetworkManager.getScheduledTask());

        BackgroundTaskSchedulerDelegate delegate = new BackgroundTaskSchedulerGcmNetworkManager();

        assertTrue(delegate.schedule(ContextUtils.getApplicationContext(), oneOffTaskInfo));

        // Check that a task was scheduled using GCM Network Manager.
        assertNotNull(mGcmNetworkManager.getScheduledTask());

        // Verify details of the scheduled task.
        Task scheduledTask = mGcmNetworkManager.getScheduledTask();
        assertTrue(scheduledTask instanceof OneoffTask);
        OneoffTask oneOffTask = (OneoffTask) scheduledTask;

        assertEquals(Integer.toString(TaskIds.TEST), scheduledTask.getTag());
        assertEquals(TimeUnit.HOURS.toSeconds(1), oneOffTask.getWindowEnd());
        assertEquals(0, oneOffTask.getWindowStart());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testScheduleNoGooglePlayServices() {
        Shadows.shadowOf(GoogleApiAvailability.getInstance())
                .setIsGooglePlayServicesAvailable(ConnectionResult.SERVICE_MISSING);

        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TIME_24_H_TO_MS).build();
        TaskInfo oneOffTaskInfo = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        assertFalse(new BackgroundTaskSchedulerGcmNetworkManager().schedule(
                ContextUtils.getApplicationContext(), oneOffTaskInfo));
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testCancel() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TIME_24_H_TO_MS).build();
        TaskInfo oneOffTaskInfo = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        BackgroundTaskSchedulerDelegate delegate = new BackgroundTaskSchedulerGcmNetworkManager();

        assertTrue(delegate.schedule(ContextUtils.getApplicationContext(), oneOffTaskInfo));
        delegate.cancel(ContextUtils.getApplicationContext(), TaskIds.TEST);

        Task canceledTask = mGcmNetworkManager.getCanceledTask();
        assertEquals(Integer.toString(TaskIds.TEST), canceledTask.getTag());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testCancelNoGooglePlayServices() {
        // This simulates situation where Google Play Services is uninstalled.
        Shadows.shadowOf(GoogleApiAvailability.getInstance())
                .setIsGooglePlayServicesAvailable(ConnectionResult.SERVICE_MISSING);

        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TIME_24_H_TO_MS).build();
        TaskInfo oneOffTaskInfo = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        // Ensure there was a previously scheduled task.
        mGcmNetworkManager.schedule(
                BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo));

        BackgroundTaskSchedulerDelegate delegate = new BackgroundTaskSchedulerGcmNetworkManager();

        // This call is expected to have no effect on GCM Network Manager, because Play Services are
        // not available.
        delegate.cancel(ContextUtils.getApplicationContext(), TaskIds.TEST);
        assertNull(mGcmNetworkManager.getCanceledTask());
        assertNotNull(mGcmNetworkManager.getScheduledTask());
    }
}
