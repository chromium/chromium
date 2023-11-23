// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.component_updater;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

/** Java-side implementation of the component update scheduler using the BackgroundTaskScheduler. */
@JNINamespace("component_updater")
public class UpdateScheduler {
    private static UpdateScheduler sInstance;
    private TaskFinishedCallback mTaskFinishedCallback;
    private long mNativeScheduler;
    private long mDelayMs;

    @CalledByNative
    /* package */ static UpdateScheduler getInstance() {
        if (sInstance == null) {
            sInstance = new UpdateScheduler();
        }
        return sInstance;
    }

    /* package */ void onStartTaskBeforeNativeLoaded(TaskFinishedCallback callback) {
        mTaskFinishedCallback = callback;
    }

    /* package */ void onStartTaskWithNative() {
        assert mNativeScheduler != 0;
        UpdateSchedulerJni.get().onStartTask(mNativeScheduler, UpdateScheduler.this);
    }

    /* package */ void onStopTask() {
        if (mNativeScheduler != 0) {
            UpdateSchedulerJni.get().onStopTask(mNativeScheduler, UpdateScheduler.this);
        }
        mTaskFinishedCallback = null;
        scheduleInternal(mDelayMs);
    }

    private UpdateScheduler() {}

    private void scheduleInternal(long delayMs) {
        // Skip re-scheduling if we are currently running the update task. Otherwise, the current
        // update tasks would be cancelled.
        if (mTaskFinishedCallback != null) return;

        TaskInfo taskInfo =
                TaskInfo.createOneOffTask(
                                TaskIds.COMPONENT_UPDATE_JOB_ID, delayMs, Integer.MAX_VALUE)
                        .setUpdateCurrent(true)
                        .setRequiredNetworkType(TaskInfo.NetworkType.UNMETERED)
                        .setIsPersisted(true)
                        .build();
        BackgroundTaskSchedulerFactory.getScheduler()
                .schedule(ContextUtils.getApplicationContext(), taskInfo);
    }

    @CalledByNative
    private void schedule(long initialDelayMs, long delayMs) {
        mDelayMs = delayMs;
        scheduleInternal(initialDelayMs);
    }

    @CalledByNative
    private void finishTask(boolean reschedule) {
        assert mTaskFinishedCallback != null;
        mTaskFinishedCallback.taskFinished(false);
        mTaskFinishedCallback = null;
        if (reschedule) {
            scheduleInternal(mDelayMs);
        }
    }

    @CalledByNative
    private void setNativeScheduler(long nativeScheduler) {
        mNativeScheduler = nativeScheduler;
    }

    @CalledByNative
    private void cancelTask() {
        BackgroundTaskSchedulerFactory.getScheduler()
                .cancel(ContextUtils.getApplicationContext(), TaskIds.COMPONENT_UPDATE_JOB_ID);
    }

    @NativeMethods
    interface Natives {
        void onStartTask(long nativeBackgroundTaskUpdateScheduler, UpdateScheduler caller);

        void onStopTask(long nativeBackgroundTaskUpdateScheduler, UpdateScheduler caller);
    }
}
