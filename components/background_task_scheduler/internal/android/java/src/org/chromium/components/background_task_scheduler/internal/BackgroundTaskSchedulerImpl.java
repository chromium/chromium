// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.content.Context;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.TaskInfo;

/**
 * This {@link BackgroundTaskScheduler} is the only one used in production code, and it is used to
 * schedule jobs that run in the background.
 *
 * To get an instance of this class, use {@link BackgroundTaskSchedulerFactory#getScheduler()}.
 */
class BackgroundTaskSchedulerImpl implements BackgroundTaskScheduler {
    private static final String SWITCH_IGNORE_BACKGROUND_TASKS = "ignore-background-tasks";

    private final BackgroundTaskSchedulerDelegate mSchedulerDelegate;

    /** Constructor only for {@link BackgroundTaskSchedulerFactory} and internal component tests. */
    BackgroundTaskSchedulerImpl(BackgroundTaskSchedulerDelegate schedulerDelegate) {
        mSchedulerDelegate = schedulerDelegate;
    }

    @Override
    public boolean schedule(Context context, TaskInfo taskInfo) {
        if (CommandLine.getInstance().hasSwitch(SWITCH_IGNORE_BACKGROUND_TASKS)) {
            // When background tasks finish executing, they leave a cached process, which
            // artificially inflates startup metrics that are based on events near to process
            // creation.
            return true;
        }
        try (TraceEvent te =
                TraceEvent.scoped(
                        "BackgroundTaskScheduler.schedule",
                        Integer.toString(taskInfo.getTaskId()))) {
            ThreadUtils.assertOnUiThread();

            SchedulingVisitor schedulingVisitor = new SchedulingVisitor(context, taskInfo);
            taskInfo.getTimingInfo().accept(schedulingVisitor);
            boolean success = schedulingVisitor.getSuccess();
            BackgroundTaskSchedulerUma.getInstance()
                    .reportTaskScheduled(taskInfo.getTaskId(), success);

            // Retain expiration metrics
            MetricsVisitor metricsVisitor = new MetricsVisitor(taskInfo.getTaskId());
            taskInfo.getTimingInfo().accept(metricsVisitor);

            return success;
        }
    }

    private class SchedulingVisitor implements TaskInfo.TimingInfoVisitor {
        private Context mContext;
        private TaskInfo mTaskInfo;
        private boolean mSuccess;

        SchedulingVisitor(Context context, TaskInfo taskInfo) {
            mContext = context;
            mTaskInfo = taskInfo;
        }

        // Only valid after a TimingInfo object was visited.
        boolean getSuccess() {
            return mSuccess;
        }

        @Override
        public void visit(TaskInfo.OneOffInfo oneOffInfo) {
            mSuccess = mSchedulerDelegate.schedule(mContext, mTaskInfo);
        }

        @Override
        public void visit(TaskInfo.PeriodicInfo periodicInfo) {
            mSuccess = mSchedulerDelegate.schedule(mContext, mTaskInfo);
        }
    }

    // TODO(crbug.com/41477414): Update the documentation for the expiration feature.
    private static class MetricsVisitor implements TaskInfo.TimingInfoVisitor {
        private final int mTaskId;

        MetricsVisitor(int taskId) {
            mTaskId = taskId;
        }

        @Override
        public void visit(TaskInfo.OneOffInfo oneOffInfo) {
            BackgroundTaskSchedulerUma.getInstance()
                    .reportTaskCreatedAndExpirationState(
                            mTaskId, oneOffInfo.expiresAfterWindowEndTime());
        }

        @Override
        public void visit(TaskInfo.PeriodicInfo periodicInfo) {
            BackgroundTaskSchedulerUma.getInstance()
                    .reportTaskCreatedAndExpirationState(
                            mTaskId, periodicInfo.expiresAfterWindowEndTime());
        }
    }

    @Override
    public void cancel(Context context, int taskId) {
        try (TraceEvent te =
                TraceEvent.scoped("BackgroundTaskScheduler.cancel", Integer.toString(taskId))) {
            ThreadUtils.assertOnUiThread();
            BackgroundTaskSchedulerUma.getInstance().reportTaskCanceled(taskId);

            mSchedulerDelegate.cancel(context, taskId);
        }
    }

    @Override
    public void doMaintenance() {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskScheduler.checkForOSUpgrade")) {
            ThreadUtils.assertOnUiThread();

            BackgroundTaskSchedulerUma.getInstance().flushStats();
        }
    }
}
