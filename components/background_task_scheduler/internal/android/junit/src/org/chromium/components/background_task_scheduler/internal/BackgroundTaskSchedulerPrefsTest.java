// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.SharedPreferences;
import android.os.Build;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskSchedulerPrefs}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class BackgroundTaskSchedulerPrefsTest {
    private TaskInfo mTask1;
    private TaskInfo mTask2;

    private class AllValidTestBackgroundTaskFactory implements BackgroundTaskFactory {
        @Override
        public BackgroundTask getBackgroundTaskFromTaskId(int taskId) {
            return new TestBackgroundTask();
        }
    }

    @Before
    public void setUp() {
        TaskInfo.TimingInfo timingInfo1 =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TimeUnit.DAYS.toMillis(1)).build();
        mTask1 = TaskInfo.createTask(TaskIds.TEST, timingInfo1).build();
        TaskInfo.TimingInfo timingInfo2 =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TimeUnit.DAYS.toMillis(1)).build();
        mTask2 = TaskInfo.createTask(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID, timingInfo2).build();

        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new AllValidTestBackgroundTaskFactory());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testAddScheduledTask() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask1);
        assertEquals("We are expecting a single entry.", 1,
                BackgroundTaskSchedulerPrefs.getScheduledTaskIds().size());

        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask1);
        assertEquals("Still there should be only one entry, as duplicate was added.", 1,
                BackgroundTaskSchedulerPrefs.getScheduledTaskIds().size());

        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask2);
        assertEquals("There should be 2 tasks in shared prefs.", 2,
                BackgroundTaskSchedulerPrefs.getScheduledTaskIds().size());

        ScheduledTaskProto.ScheduledTask scheduledTask1 =
                BackgroundTaskSchedulerPrefs.getScheduledTask(TaskIds.TEST);
        assertEquals(ScheduledTaskProto.ScheduledTask.Type.ONE_OFF, scheduledTask1.getType());
        ScheduledTaskProto.ScheduledTask scheduledTask2 =
                BackgroundTaskSchedulerPrefs.getScheduledTask(
                        TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID);
        assertEquals(ScheduledTaskProto.ScheduledTask.Type.ONE_OFF, scheduledTask2.getType());

        Map<Integer, ScheduledTaskProto.ScheduledTask> scheduledTasks =
                BackgroundTaskSchedulerPrefs.getScheduledTasks();
        assertEquals("These should be 2 shared prefs.", 2, scheduledTasks.size());
        assertEquals(scheduledTask1, scheduledTasks.get(TaskIds.TEST));
        assertEquals(scheduledTask2, scheduledTasks.get(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID));

        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TimeUnit.DAYS.toMillis(1)).build();
        TaskInfo task3 = TaskInfo.createTask(TaskIds.OMAHA_JOB_ID, timingInfo).build();

        BackgroundTaskSchedulerPrefs.addScheduledTask(task3);
        assertEquals("There should be 3 tasks in shared prefs.", 3,
                BackgroundTaskSchedulerPrefs.getScheduledTaskIds().size());

        Set<Integer> taskIds = BackgroundTaskSchedulerPrefs.getScheduledTaskIds();
        assertTrue(taskIds.contains(mTask1.getTaskId()));
        assertTrue(taskIds.contains(mTask2.getTaskId()));
        assertTrue(taskIds.contains(task3.getTaskId()));
        assertEquals("mTask1 class name in scheduled tasks.", TestBackgroundTask.class,
                BackgroundTaskSchedulerFactoryInternal
                        .getBackgroundTaskFromTaskId(mTask1.getTaskId())
                        .getClass());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testRemoveScheduledTask() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask1);
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask2);
        BackgroundTaskSchedulerPrefs.removeScheduledTask(mTask1.getTaskId());
        assertEquals("We are expecting a single entry.", 1,
                BackgroundTaskSchedulerPrefs.getScheduledTaskIds().size());

        BackgroundTaskSchedulerPrefs.removeScheduledTask(mTask1.getTaskId());
        assertEquals("Removing a task which is not there does not affect the task set.", 1,
                BackgroundTaskSchedulerPrefs.getScheduledTaskIds().size());

        Set<Integer> taskIds = BackgroundTaskSchedulerPrefs.getScheduledTaskIds();
        assertFalse(taskIds.contains(mTask1.getTaskId()));
        assertTrue(taskIds.contains(mTask2.getTaskId()));
        assertEquals("mTask1 class name in scheduled tasks.", TestBackgroundTask.class,
                BackgroundTaskSchedulerFactoryInternal
                        .getBackgroundTaskFromTaskId(mTask1.getTaskId())
                        .getClass());

        ScheduledTaskProto.ScheduledTask scheduledTask1 =
                BackgroundTaskSchedulerPrefs.getScheduledTask(TaskIds.TEST);
        assertNull(scheduledTask1);
        ScheduledTaskProto.ScheduledTask scheduledTask2 =
                BackgroundTaskSchedulerPrefs.getScheduledTask(
                        TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID);
        assertEquals(ScheduledTaskProto.ScheduledTask.Type.ONE_OFF, scheduledTask2.getType());

        Map<Integer, ScheduledTaskProto.ScheduledTask> scheduledTasks =
                BackgroundTaskSchedulerPrefs.getScheduledTasks();
        assertEquals("We are expecting a single entry.", 1, scheduledTasks.size());
        assertEquals(scheduledTask2, scheduledTasks.get(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID));
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testUnparseableEntries() {
        HashSet<String> badEntries = new HashSet<>();
        badEntries.add(":123");
        badEntries.add("Class:");
        badEntries.add("Class:NotAnInt");
        badEntries.add("Int field missing");
        badEntries.add("Class:123:Too many fields");
        badEntries.add("");
        badEntries.add(null);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putStringSet("bts_scheduled_tasks", badEntries)
                .apply();
        assertTrue(BackgroundTaskSchedulerPrefs.getScheduledTaskIds().isEmpty());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testRemoveAllTasks() {
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask1);
        BackgroundTaskSchedulerPrefs.addScheduledTask(mTask2);
        BackgroundTaskSchedulerPrefs.removeAllTasks();
        assertTrue("We are expecting a all tasks to be gone.",
                BackgroundTaskSchedulerPrefs.getScheduledTaskIds().isEmpty());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testLastSdkVersion() {
        ReflectionHelpers.setStaticField(
                Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.KITKAT);
        assertEquals("Current SDK version should be default.", Build.VERSION_CODES.KITKAT,
                BackgroundTaskSchedulerPrefs.getLastSdkVersion());
        BackgroundTaskSchedulerPrefs.setLastSdkVersion(Build.VERSION_CODES.LOLLIPOP);
        assertEquals(
                Build.VERSION_CODES.LOLLIPOP, BackgroundTaskSchedulerPrefs.getLastSdkVersion());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testMigrationToProto() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> scheduledTasks = prefs.getStringSet(
                BackgroundTaskSchedulerPrefs.KEY_SCHEDULED_TASKS, new HashSet<String>(1));

        // Create a valid BackgroundTaskSchedulerPrefs entry corresponding to TaskIds.TEST.
        scheduledTasks.add("foo:33656");

        // Create a valid BackgroundTaskSchedulerPrefs entry corresponding to
        // TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID.
        scheduledTasks.add("foo:77");

        SharedPreferences.Editor editor = prefs.edit();
        editor.putStringSet(BackgroundTaskSchedulerPrefs.KEY_SCHEDULED_TASKS, scheduledTasks);
        editor.apply();

        BackgroundTaskSchedulerPrefs.migrateStoredTasksToProto();

        Set<Integer> taskIds = BackgroundTaskSchedulerPrefs.getScheduledTaskIds();
        assertTrue(taskIds.contains(mTask1.getTaskId()));
        assertTrue(taskIds.contains(mTask2.getTaskId()));
        assertEquals("mTask1 class name in scheduled tasks.", TestBackgroundTask.class,
                BackgroundTaskSchedulerFactoryInternal
                        .getBackgroundTaskFromTaskId(mTask1.getTaskId())
                        .getClass());

        BackgroundTaskSchedulerPrefs.migrateStoredTasksToProto();
    }
}
