// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.Context;

import androidx.annotation.MainThread;

/**
 * A BackgroundTaskScheduler is used to schedule jobs that run in the background.
 * It is backed by the system API ({@link android.app.job.JobScheduler}).
 *
 * To get an instance of this class, use {@link BackgroundTaskSchedulerFactory#getScheduler()}.
 */
public interface BackgroundTaskScheduler {
    /**
     * Schedules a background task. See {@link TaskInfo} for information on what types of tasks that
     * can be scheduled.
     *
     * @param context the current context.
     * @param taskInfo the information about the task to be scheduled.
     * @return true if the schedule operation succeeded, and false otherwise.
     * @see TaskInfo
     */
    @MainThread
    boolean schedule(Context context, TaskInfo taskInfo);

    /**
     * Cancels the task specified by the task ID.
     *
     * @param context the current context.
     * @param taskId the ID of the task to cancel. See {@link TaskIds} for a list.
     */
    @MainThread
    void cancel(Context context, int taskId);

    /** Flushes cached UMA data. Must not be invoked until native has been loaded. */
    @MainThread
    void doMaintenance();
}
