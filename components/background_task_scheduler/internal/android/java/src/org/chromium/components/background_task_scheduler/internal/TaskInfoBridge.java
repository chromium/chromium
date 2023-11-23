// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.os.PersistableBundle;

import org.jni_zero.CalledByNative;

import org.chromium.components.background_task_scheduler.TaskInfo;

/** Converts native task info params to Java {@link TaskInfo}. */
public class TaskInfoBridge {
    @CalledByNative
    private static TaskInfo.TimingInfo createPeriodicInfo(
            long intervalMs, long flexMs, boolean expiresAfterWindowEndTime) {
        TaskInfo.PeriodicInfo.Builder builder =
                TaskInfo.PeriodicInfo.create()
                        .setIntervalMs(intervalMs)
                        .setExpiresAfterWindowEndTime(expiresAfterWindowEndTime);
        if (flexMs > 0) builder.setFlexMs(flexMs);
        return builder.build();
    }

    @CalledByNative
    private static TaskInfo.TimingInfo createOneOffInfo(
            long windowStartTimeMs, long windowEndTimeMs, boolean expiresAfterWindowEndTime) {
        TaskInfo.OneOffInfo.Builder builder =
                TaskInfo.OneOffInfo.create()
                        .setWindowEndTimeMs(windowEndTimeMs)
                        .setExpiresAfterWindowEndTime(expiresAfterWindowEndTime);
        if (windowStartTimeMs > 0) builder.setWindowStartTimeMs(windowStartTimeMs);
        return builder.build();
    }

    @CalledByNative
    private static TaskInfo createTaskInfo(
            int taskId,
            TaskInfo.TimingInfo timingInfo,
            String extras,
            int networkType,
            boolean requiresCharging,
            boolean isPersisted,
            boolean updateCurrent) {
        PersistableBundle bundle = new PersistableBundle();
        bundle.putString(TaskInfo.SERIALIZED_TASK_EXTRAS, extras);
        TaskInfo taskInfo =
                TaskInfo.createTask(taskId, timingInfo)
                        .setRequiredNetworkType(networkType)
                        .setRequiresCharging(requiresCharging)
                        .setUpdateCurrent(updateCurrent)
                        .setIsPersisted(isPersisted)
                        .setExtras(bundle)
                        .build();
        return taskInfo;
    }
}
