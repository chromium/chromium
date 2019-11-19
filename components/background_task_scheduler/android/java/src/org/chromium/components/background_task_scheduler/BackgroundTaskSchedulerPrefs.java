// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.util.Base64;

import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Class handling shared preference entries for BackgroundTaskScheduler.
 */
public class BackgroundTaskSchedulerPrefs {
    private static final String TAG = "BTSPrefs";
    @VisibleForTesting
    static final String KEY_SCHEDULED_TASKS = "bts_scheduled_tasks";
    private static final String KEY_LAST_SDK_VERSION = "bts_last_sdk_version";
    private static final String PREF_PACKAGE = "org.chromium.components.background_task_scheduler";

    /**
     * Class abstracting conversions between a string kept in shared preferences and actual values
     * held there.
     */
    @VisibleForTesting
    static class ScheduledTaskPreferenceEntry {
        private static final String ENTRY_SEPARATOR = ":";
        private String mBackgroundTaskClass;
        private int mTaskId;

        /** Creates a scheduled task shared preference entry from task info. */
        public static ScheduledTaskPreferenceEntry createForTaskInfo(TaskInfo taskInfo) {
            return new ScheduledTaskPreferenceEntry(
                    taskInfo.getBackgroundTaskClass().getName(), taskInfo.getTaskId());
        }

        /**
         * Parses a preference entry from input string.
         *
         * @param entry An input string to parse.
         * @return A parsed entry or null if the input is not valid.
         */
        public static ScheduledTaskPreferenceEntry parseEntry(String entry) {
            if (entry == null) return null;

            String[] entryParts = entry.split(ENTRY_SEPARATOR);
            if (entryParts.length != 2 || entryParts[0].isEmpty() || entryParts[1].isEmpty()) {
                return null;
            }
            int taskId = 0;
            try {
                taskId = Integer.parseInt(entryParts[1]);
            } catch (NumberFormatException e) {
                return null;
            }
            return new ScheduledTaskPreferenceEntry(entryParts[0], taskId);
        }

        public ScheduledTaskPreferenceEntry(String className, int taskId) {
            mBackgroundTaskClass = className;
            mTaskId = taskId;
        }

        /**
         * Converts a task info to a shared preference entry in the format:
         * BACKGROUND_TASK_CLASS_NAME:TASK_ID.
         */
        @Override
        public String toString() {
            return mBackgroundTaskClass + ENTRY_SEPARATOR + mTaskId;
        }

        /** Gets the ID of the task in this entry. */
        public int getTaskId() {
            return mTaskId;
        }
    }

    static void migrateStoredTasksToProto() {
        try (TraceEvent te = TraceEvent.scoped(
                     "BackgroundTaskSchedulerPrefs.migrateStoredTasksToProto")) {
            Set<String> scheduledTasks =
                    ContextUtils.getAppSharedPreferences().getStringSet(KEY_SCHEDULED_TASKS, null);

            if (scheduledTasks == null) return;
            ContextUtils.getAppSharedPreferences().edit().remove(KEY_SCHEDULED_TASKS).apply();

            SharedPreferences.Editor editor = getSharedPreferences().edit();
            for (String entry : scheduledTasks) {
                ScheduledTaskPreferenceEntry parsed =
                        ScheduledTaskPreferenceEntry.parseEntry(entry);
                if (parsed == null) {
                    Log.w(TAG, "Scheduled task could not be parsed from storage.");
                    continue;
                }
                editor.putString(
                        String.valueOf(parsed.getTaskId()), getEmptySerializedScheduledTaskProto());
                BackgroundTaskSchedulerUma.getInstance().reportMigrationToProto(parsed.getTaskId());
            }
            editor.apply();
        }
    }

    private static String getEmptySerializedScheduledTaskProto() {
        ScheduledTaskProto.ScheduledTask scheduledTask =
                ScheduledTaskProto.ScheduledTask.newBuilder().build();
        return Base64.encodeToString(scheduledTask.toByteArray(), Base64.DEFAULT);
    }

    /** Adds a task to scheduler's preferences, so that it can be rescheduled with OS upgrade. */
    public static void addScheduledTask(TaskInfo taskInfo) {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskSchedulerPrefs.addScheduledTask",
                     Integer.toString(taskInfo.getTaskId()))) {
            ScheduledTaskProtoVisitor visitor = new ScheduledTaskProtoVisitor(taskInfo.getExtras());
            taskInfo.getTimingInfo().accept(visitor);

            getSharedPreferences()
                    .edit()
                    .putString(String.valueOf(taskInfo.getTaskId()),
                            visitor.getSerializedScheduledTask())
                    .apply();
        }
    }

    private static class ScheduledTaskProtoVisitor implements TaskInfo.TimingInfoVisitor {
        private String mSerializedScheduledTask;
        private final Bundle mExtras;

        ScheduledTaskProtoVisitor(Bundle extras) {
            mExtras = extras;
        }

        // Only valid after a TimingInfo object was visited.
        String getSerializedScheduledTask() {
            return mSerializedScheduledTask;
        }

        @Override
        public void visit(TaskInfo.OneOffInfo oneOffInfo) {
            ScheduledTaskProto.ScheduledTask scheduledTask =
                    ScheduledTaskProto.ScheduledTask.newBuilder()
                            .setType(ScheduledTaskProto.ScheduledTask.Type.ONE_OFF)
                            .build();
            mSerializedScheduledTask =
                    Base64.encodeToString(scheduledTask.toByteArray(), Base64.DEFAULT);
        }

        @Override
        public void visit(TaskInfo.PeriodicInfo periodicInfo) {
            ScheduledTaskProto.ScheduledTask scheduledTask =
                    ScheduledTaskProto.ScheduledTask.newBuilder()
                            .setType(ScheduledTaskProto.ScheduledTask.Type.PERIODIC)
                            .build();
            mSerializedScheduledTask =
                    Base64.encodeToString(scheduledTask.toByteArray(), Base64.DEFAULT);
        }

        @Override
        public void visit(TaskInfo.ExactInfo exactInfo) {
            ScheduledTaskProto.ScheduledTask scheduledTask =
                    ScheduledTaskProto.ScheduledTask.newBuilder()
                            .setType(ScheduledTaskProto.ScheduledTask.Type.EXACT)
                            .setTriggerMs(exactInfo.getTriggerAtMs())
                            .addAllExtras(
                                    ExtrasToProtoConverter.convertExtrasToProtoExtras(mExtras))
                            .build();
            mSerializedScheduledTask =
                    Base64.encodeToString(scheduledTask.toByteArray(), Base64.DEFAULT);
        }
    }

    /** Removes a task from scheduler's preferences. */
    public static void removeScheduledTask(int taskId) {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskSchedulerPrefs.removeScheduledTask",
                     Integer.toString(taskId))) {
            getSharedPreferences().edit().remove(String.valueOf(taskId)).apply();
        }
    }

    /** Gets a set of scheduled task IDs. */
    public static Set<Integer> getScheduledTaskIds() {
        try (TraceEvent te =
                        TraceEvent.scoped("BackgroundTaskSchedulerPrefs.getScheduledTaskIds")) {
            Set<Integer> result = new HashSet<>();
            for (String key : getSharedPreferences().getAll().keySet()) {
                try {
                    result.add(Integer.valueOf(key));
                } catch (NumberFormatException e) {
                    Log.e(TAG, "Incorrect task id: " + key);
                }
            }
            return result;
        }
    }

    /**
     * Gets information associated with a task id.
     * @param taskId the task id for which to retrieve data.
     * @return stored data or null if data not found or invalid.
     */
    public static ScheduledTaskProto.ScheduledTask getScheduledTask(int taskId) {
        String serialized = getSharedPreferences().getString(String.valueOf(taskId), null);
        if (serialized == null) {
            Log.e(TAG, "No data found for task id: " + taskId);
            return null;
        }

        try {
            return ScheduledTaskProto.ScheduledTask.parseFrom(Base64.decode(serialized, 0));
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG, "Invalid protocol buffer: " + e);
            removeScheduledTask(taskId);
            return null;
        }
    }

    /**
     * Gets all current scheduled task.
     * @return map of task ids associated with scheduled task protos.
     */
    public static Map<Integer, ScheduledTaskProto.ScheduledTask> getScheduledTasks() {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskSchedulerPrefs.getScheduledTasks")) {
            Map<Integer, ScheduledTaskProto.ScheduledTask> result = new HashMap<>();
            for (Map.Entry<String, ?> entry : getSharedPreferences().getAll().entrySet()) {
                try {
                    int taskId = Integer.valueOf(entry.getKey());
                    ScheduledTaskProto.ScheduledTask scheduledTask =
                            ScheduledTaskProto.ScheduledTask.parseFrom(
                                    Base64.decode(String.valueOf(entry.getValue()), 0));

                    result.put(taskId, scheduledTask);
                } catch (NumberFormatException e) {
                    Log.e(TAG, "Incorrect task id: " + entry.getKey());
                } catch (InvalidProtocolBufferException e) {
                    Log.e(TAG, "Invalid protocol buffer: " + e);
                    removeScheduledTask(Integer.valueOf(entry.getKey()));
                }
            }
            return result;
        }
    }

    /**
     * Removes all scheduled tasks from shared preferences store. Removes tasks from the old
     * storage format and from the proto format.
     */
    public static void removeAllTasks() {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskSchedulerPrefs.removeAllTasks")) {
            ContextUtils.getAppSharedPreferences().edit().remove(KEY_SCHEDULED_TASKS).apply();
            getSharedPreferences().edit().clear().apply();
        }
    }

    /**
     * Pre-load shared prefs to avoid being blocked on the disk reads in the future.
     */
    public static void warmUpSharedPrefs() {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskSchedulerPrefs.warmUpSharedPrefs")) {
            getSharedPreferences();
        }
    }

    /** Returns the BackgroundTaskScheduler SharedPreferences. */
    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext().getSharedPreferences(
                PREF_PACKAGE, Context.MODE_PRIVATE);
    }

    /** Gets the last SDK version on which this instance ran. Defaults to current SDK version. */
    public static int getLastSdkVersion() {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskSchedulerPrefs.getLastSdkVersion")) {
            int sdkInt = ContextUtils.getAppSharedPreferences().getInt(
                    KEY_LAST_SDK_VERSION, Build.VERSION.SDK_INT);
            return sdkInt;
        }
    }

    /** Gets the last SDK version on which this instance ran. */
    public static void setLastSdkVersion(int sdkVersion) {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskSchedulerPrefs.setLastSdkVersion",
                     Integer.toString(sdkVersion))) {
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putInt(KEY_LAST_SDK_VERSION, sdkVersion)
                    .apply();
        }
    }
}