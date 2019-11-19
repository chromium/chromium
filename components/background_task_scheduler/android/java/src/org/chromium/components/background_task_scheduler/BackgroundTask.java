// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.Context;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;

/**
 * Entry point for callbacks from {@link BackgroundTaskScheduler}. Any classes implementing
 * this interface must have a public constructor which takes no arguments.
 * The callbacks will be executed on the main thread, which means that execution logic must be
 * offloaded to another {@link Thread}, {@link android.os.Handler} or {@link
 * org.chromium.base.task.AsyncTask}.
 */
public interface BackgroundTask {
    /**
     * Callback to invoke whenever background processing has finished after first returning true
     * from {@link #onStartTask(Context, TaskParameters, TaskFinishedCallback)}.
     */
    interface TaskFinishedCallback {
        /**
         * Callback to inform the {@link BackgroundTaskScheduler} that the background processing
         * now has finished. When this callback is invoked, the system will stop holding a wakelock.
         *
         * @param needsReschedule whether this task must be rescheduled.
         */
        @AnyThread
        void taskFinished(boolean needsReschedule);
    }

    /**
     * Callback from {@link BackgroundTaskScheduler} when your task should start processing.
     * It is invoked on the main thread, and if your task finishes quickly, you should return false
     * from this method when you are done processing. If this is a long-running task, you should
     * return true from this method, and instead invoke the {@link TaskFinishedCallback} when the
     * processing is finished on some other {@link Thread}, {@link android.os.Handler} or
     * {@link org.chromium.base.task.AsyncTask}. While this method is running the
     * system holds a wakelock. If false is returned from this method, the wakelock is immediately
     * released, but if this method returns true, the wakelock is not released until either the
     * {@link TaskFinishedCallback} is invoked, or the system calls {@link #onStopTask(Context,
     * TaskParameters)}.
     *
     * @param context the current context.
     * @param taskParameters the data passed in as {@link TaskInfo} when the task was scheduled.
     * @param callback if the task needs to continue processing after returning from the call to
     *                 {@link #onStartTask(Context, TaskParameters, TaskFinishedCallback)}, this
     *                 callback must be invoked when the processing has finished.
     * @return true if the task needs to continue processing work. False if there is no more work.
     * @see TaskInfo
     */
    @MainThread
    boolean onStartTask(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback);

    /**
     * Callback from {@link BackgroundTaskScheduler} when the system has determined that the
     * execution of the task must stop immediately, even before the {@link TaskFinishedCallback}
     * has been invoked. This will typically happen whenever the required conditions for the task
     * are no longer met. See {@link TaskInfo}. A wakelock is held by the system while this callback
     * is invoked, and immediately released after this method returns.
     *
     * @param context the current context.
     * @param taskParameters the data passed in as {@link TaskInfo} when the task was scheduled.
     * @return true if the task needs to be rescheduled according to the rescheduling criteria
     * specified when the task was scheduled initially. False if the taskshould not be rescheduled.
     * Regardless of the value returned, your task must stop executing.
     * @see TaskInfo
     */
    @MainThread
    boolean onStopTask(Context context, TaskParameters taskParameters);

    /**
     * Callback from {@link BackgroundTaskScheduler} when it detects system conditions requiring
     * rescheduling, e.g. Google Play Services update or OS upgrade. The task should schedule itself
     * again with appropriate parameters.
     *
     * @param context the current context.
     */
    @MainThread
    void reschedule(Context context);
}
