// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.reporting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

/** Native bridge for scheduling C++ signals reporting with Java background task scheduler. */
@JNINamespace("enterprise_reporting")
@NullMarked
public final class SignalsReportingSchedulerBridge {
    private SignalsReportingSchedulerBridge() {
        // Util class, do not instantiate.
    }

    @CalledByNative
    public static void scheduleReportBackgroundTask(long scheduledTimeDeltaMs) {
        TaskInfo.TimingInfo oneOffTimingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(scheduledTimeDeltaMs).build();

        TaskInfo taskInfo =
                TaskInfo.createTask(TaskIds.CHROME_SIGNALS_REPORTING_JOB_ID, oneOffTimingInfo)
                        .setUpdateCurrent(true)
                        .setIsPersisted(true)
                        .build();

        BackgroundTaskSchedulerFactory.getScheduler()
                .schedule(ContextUtils.getApplicationContext(), taskInfo);
    }

    @CalledByNative
    public static void cancelReportBackgroundTask() {
        BackgroundTaskSchedulerFactory.getScheduler()
                .cancel(
                        ContextUtils.getApplicationContext(),
                        TaskIds.CHROME_SIGNALS_REPORTING_JOB_ID);
    }

    @NativeMethods
    interface Natives {
        void startReporting();
    }
}
