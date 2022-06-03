// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.os.Bundle;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.background_task_scheduler.TaskInfo;

/**
 * Converts native task info params to Java {@link TaskInfo}.
 */
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
    private static TaskInfo.TimingInfo createExactInfo(long triggerAtMs) {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.ExactInfo.create().setTriggerAtMs(triggerAtMs).build();
        return timingInfo;
    }

    @CalledByNative
    private static TaskInfo createTaskInfo(
            int taskId, TaskInfo.TimingInfo timingInfo, String extras) {
        Bundle bundle = new Bundle();
        bundle.putString(TaskInfo.SERIALIZED_TASK_EXTRAS, extras);
        TaskInfo taskInfo = TaskInfo.createTask(taskId, timingInfo)
                                    .setRequiredNetworkType(TaskInfo.NetworkType.ANY)
                                    .setRequiresCharging(false)
                                    .setUpdateCurrent(true)
                                    .setIsPersisted(true)
                                    .setExtras(bundle)
                                    .build();
        return taskInfo;
    }
}
