// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.component_updater;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

/** Java-side implementation of the component update scheduler using the BackgroundTaskScheduler. */
@JNINamespace("component_updater")
@NullMarked
public class UpdateScheduler {
    @JniType("::component_updater::BackgroundTaskUpdateScheduler")
    interface NativeBackgroundTaskUpdateScheduler extends JniTypeToken {}

    private static @Nullable UpdateScheduler sInstance;
    private @Nullable TaskFinishedCallback mTaskFinishedCallback;
    private @Nullable JniRawPtr<NativeBackgroundTaskUpdateScheduler> mNativeScheduler;
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
        assert mNativeScheduler != null;
        UpdateSchedulerJni.get().onStartTask(assumeNonNull(mNativeScheduler));
    }

    /* package */ void onStopTask() {
        if (mNativeScheduler != null) {
            UpdateSchedulerJni.get().onStopTask(mNativeScheduler);
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
    void setNativeScheduler(
            @Nullable JniRawPtr<NativeBackgroundTaskUpdateScheduler> nativeScheduler) {
        if (mNativeScheduler != null) {
            mNativeScheduler.release();
        }
        mNativeScheduler = nativeScheduler;
    }

    @CalledByNative
    private void cancelTask() {
        BackgroundTaskSchedulerFactory.getScheduler()
                .cancel(ContextUtils.getApplicationContext(), TaskIds.COMPONENT_UPDATE_JOB_ID);
    }

    @NativeMethods
    interface Natives {
        void onStartTask(JniPtr<NativeBackgroundTaskUpdateScheduler> self);

        void onStopTask(JniPtr<NativeBackgroundTaskUpdateScheduler> self);
    }
}
