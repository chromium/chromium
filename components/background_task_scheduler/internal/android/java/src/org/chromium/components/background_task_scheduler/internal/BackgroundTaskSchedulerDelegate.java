// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.content.Context;

import org.chromium.components.background_task_scheduler.TaskInfo;

/**
 * The internal representation of a {@link BackgroundTaskScheduler} to make it possible to use
 * system API ({@link android.app.job.JobScheduler}.
 */
interface BackgroundTaskSchedulerDelegate {
    String BACKGROUND_TASK_ID_KEY = "_background_task_id";
    String BACKGROUND_TASK_EXTRAS_KEY = "_background_task_extras";
    String BACKGROUND_TASK_SCHEDULE_TIME_KEY = "_background_task_schedule_time";
    String BACKGROUND_TASK_END_TIME_KEY = "_background_task_end_time";
    String BACKGROUND_TASK_INTERVAL_TIME_KEY = "_background_task_interval_time";
    String BACKGROUND_TASK_FLEX_TIME_KEY = "_background_task_flex_time";

    /**
     * Schedules a background task. See {@link TaskInfo} for information on what types of tasks that
     * can be scheduled.
     *
     * @param context the current context.
     * @param taskInfo the information about the task to be scheduled.
     * @return true if the schedule operation succeeded, and false otherwise.
     * @see TaskInfo
     */
    boolean schedule(Context context, TaskInfo taskInfo);

    /**
     * Cancels the task specified by the task ID.
     *
     * @param context the current context.
     * @param taskId the ID of the task to cancel. See {@link TaskIds} for a list.
     */
    void cancel(Context context, int taskId);
}
