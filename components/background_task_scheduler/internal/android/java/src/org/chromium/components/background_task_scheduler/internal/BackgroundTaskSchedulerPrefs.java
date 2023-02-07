// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Base64;

import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TraceEvent;
import org.chromium.components.background_task_scheduler.TaskInfo;

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
    private static final String KEY_LAST_SDK_VERSION = "bts_last_sdk_version";
    private static final String PREF_PACKAGE = "org.chromium.components.background_task_scheduler";

    /** Adds a task to scheduler's preferences, so that it can be rescheduled with OS upgrade. */
    public static void addScheduledTask(TaskInfo taskInfo) {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskSchedulerPrefs.addScheduledTask",
                     Integer.toString(taskInfo.getTaskId()))) {
            ScheduledTaskProtoVisitor visitor = new ScheduledTaskProtoVisitor(taskInfo.getExtras(),
                    taskInfo.getRequiredNetworkType(), taskInfo.requiresCharging());
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
        @TaskInfo.NetworkType
        private final int mRequiredNetworkType;
        private final boolean mRequiresCharging;

        ScheduledTaskProtoVisitor(Bundle extras, @TaskInfo.NetworkType int requiredNetworkType,
                boolean requiresCharging) {
            mExtras = extras;
            mRequiredNetworkType = requiredNetworkType;
            mRequiresCharging = requiresCharging;
        }

        // Only valid after a TimingInfo object was visited.
        String getSerializedScheduledTask() {
            return mSerializedScheduledTask;
        }

        @Override
        public void visit(TaskInfo.OneOffInfo oneOffInfo) {
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                ScheduledTaskProto.ScheduledTask scheduledTask =
                        ScheduledTaskProto.ScheduledTask.newBuilder()
                                .setType(ScheduledTaskProto.ScheduledTask.Type.ONE_OFF)
                                .build();
                mSerializedScheduledTask =
                        Base64.encodeToString(scheduledTask.toByteArray(), Base64.DEFAULT);
            }
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
                            .setRequiredNetworkType(
                                    convertToRequiredNetworkType(mRequiredNetworkType))
                            .setRequiresCharging(mRequiresCharging)
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
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    static Map<Integer, ScheduledTaskProto.ScheduledTask> getScheduledTasks() {
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
     * Removes all scheduled tasks from shared preferences store.
     */
    public static void removeAllTasks() {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskSchedulerPrefs.removeAllTasks")) {
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

    private static ScheduledTaskProto.ScheduledTask.RequiredNetworkType
    convertToRequiredNetworkType(@TaskInfo.NetworkType int networkType) {
        switch (networkType) {
            case TaskInfo.NetworkType.NONE:
                return ScheduledTaskProto.ScheduledTask.RequiredNetworkType.NONE;
            case TaskInfo.NetworkType.ANY:
                return ScheduledTaskProto.ScheduledTask.RequiredNetworkType.ANY;
            case TaskInfo.NetworkType.UNMETERED:
                return ScheduledTaskProto.ScheduledTask.RequiredNetworkType.UNMETERED;
            default:
                assert false : "Incorrect value of TaskInfo.NetworkType";
                return ScheduledTaskProto.ScheduledTask.RequiredNetworkType.NONE;
        }
    }
}
