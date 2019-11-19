// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.Context;

import androidx.annotation.MainThread;

/**
 * A BackgroundTaskScheduler is used to schedule jobs that run in the background.
 * It is backed by system APIs ({@link android.app.job.JobScheduler}) on newer platforms
 * and by GCM ({@link com.google.android.gms.gcm.GcmNetworkManager}) on older platforms.
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

    /**
     * Checks whether OS was upgraded and triggers rescheduling if it is necessary.
     * Rescheduling is necessary if type of background task scheduler delegate is different for a
     * new version of the OS.
     *
     * @param context the current context.
     */
    @MainThread
    void checkForOSUpgrade(Context context);

    /**
     * Reschedules all the tasks currently scheduler through BackgroundTaskSheduler.
     * @param context the current context.
     */
    @MainThread
    void reschedule(Context context);
}
