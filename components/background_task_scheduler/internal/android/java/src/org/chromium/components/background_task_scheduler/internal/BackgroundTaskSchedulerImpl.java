// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.content.Context;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.TaskInfo;

import java.util.Map;

/**
 * This {@link BackgroundTaskScheduler} is the only one used in production code, and it is used to
 * schedule jobs that run in the background.
 *
 * To get an instance of this class, use {@link BackgroundTaskSchedulerFactory#getScheduler()}.
 */
class BackgroundTaskSchedulerImpl implements BackgroundTaskScheduler {
    private static final String TAG = "BkgrdTaskScheduler";
    private static final String SWITCH_IGNORE_BACKGROUND_TASKS = "ignore-background-tasks";

    private final BackgroundTaskSchedulerDelegate mSchedulerDelegate;
    private final BackgroundTaskSchedulerDelegate mAlarmManagerDelegate;

    /** Constructor only for {@link BackgroundTaskSchedulerFactory} and internal component tests. */
    BackgroundTaskSchedulerImpl(BackgroundTaskSchedulerDelegate schedulerDelegate,
            BackgroundTaskSchedulerDelegate alarmManagerDelegate) {
        mSchedulerDelegate = schedulerDelegate;
        mAlarmManagerDelegate = alarmManagerDelegate;
    }

    @Override
    public boolean schedule(Context context, TaskInfo taskInfo) {
        if (CommandLine.getInstance().hasSwitch(SWITCH_IGNORE_BACKGROUND_TASKS)) {
            // When background tasks finish executing, they leave a cached process, which
            // artificially inflates startup metrics that are based on events near to process
            // creation.
            return true;
        }
        try (TraceEvent te = TraceEvent.scoped(
                     "BackgroundTaskScheduler.schedule", Integer.toString(taskInfo.getTaskId()))) {
            ThreadUtils.assertOnUiThread();

            SchedulingVisitor schedulingVisitor = new SchedulingVisitor(context, taskInfo);
            taskInfo.getTimingInfo().accept(schedulingVisitor);
            boolean success = schedulingVisitor.getSuccess();
            BackgroundTaskSchedulerUma.getInstance().reportTaskScheduled(
                    taskInfo.getTaskId(), success);

            // Retain expiration metrics
            MetricsVisitor metricsVisitor = new MetricsVisitor(taskInfo.getTaskId());
            taskInfo.getTimingInfo().accept(metricsVisitor);

            if (success) {
                BackgroundTaskSchedulerPrefs.addScheduledTask(taskInfo);
            }
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

        @Override
        public void visit(TaskInfo.ExactInfo exactInfo) {
            mSuccess = mAlarmManagerDelegate.schedule(mContext, mTaskInfo);
        }
    }

    // TODO(crbug.com/996178): Update the documentation for the expiration feature.
    private class MetricsVisitor implements TaskInfo.TimingInfoVisitor {
        private final int mTaskId;

        MetricsVisitor(int taskId) {
            mTaskId = taskId;
        }

        @Override
        public void visit(TaskInfo.OneOffInfo oneOffInfo) {
            BackgroundTaskSchedulerUma.getInstance().reportTaskCreatedAndExpirationState(
                    mTaskId, oneOffInfo.expiresAfterWindowEndTime());
        }

        @Override
        public void visit(TaskInfo.PeriodicInfo periodicInfo) {
            BackgroundTaskSchedulerUma.getInstance().reportTaskCreatedAndExpirationState(
                    mTaskId, periodicInfo.expiresAfterWindowEndTime());
        }

        @Override
        public void visit(TaskInfo.ExactInfo exactInfo) {
            BackgroundTaskSchedulerUma.getInstance().reportExactTaskCreated(mTaskId);
        }
    }

    @Override
    public void cancel(Context context, int taskId) {
        try (TraceEvent te = TraceEvent.scoped(
                     "BackgroundTaskScheduler.cancel", Integer.toString(taskId))) {
            ThreadUtils.assertOnUiThread();
            BackgroundTaskSchedulerUma.getInstance().reportTaskCanceled(taskId);

            ScheduledTaskProto.ScheduledTask scheduledTask =
                    BackgroundTaskSchedulerPrefs.getScheduledTask(taskId);
            BackgroundTaskSchedulerPrefs.removeScheduledTask(taskId);

            if (scheduledTask == null) {
                Log.e(TAG,
                        "Task cannot be canceled because no data was found in"
                                + "storage or data was invalid");
                return;
            }

            selectDelegateAndCancel(context, scheduledTask.getType(), taskId);
        }
    }

    @Override
    public boolean isScheduled(Context context, int taskId) {
        try (TraceEvent te = TraceEvent.scoped(
                     "BackgroundTaskScheduler.isScheduled", Integer.toString(taskId))) {
            ThreadUtils.assertOnUiThread();

            return (BackgroundTaskSchedulerPrefs.getScheduledTask(taskId) != null);
        }
    }

    @Override
    public void doMaintenance() {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskScheduler.checkForOSUpgrade")) {
            ThreadUtils.assertOnUiThread();

            // Update tasks stored in the old format to the proto format at Chrome Startup, if
            // tasks are found to be stored in the old format. This allows to keep only one
            // implementation of the storage methods.
            // This proto store was added in M78, and should be deleted when upgrades from that
            // of Chrome is no longer considered a core user journey.
            // TODO(crbug.com/1406114) Remove this migration and the underlying preferences.
            BackgroundTaskSchedulerPrefs.migrateStoredTasksToProto();

            BackgroundTaskSchedulerUma.getInstance().flushStats();
        }
    }

    @Override
    public void reschedule(Context context) {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskScheduler.reschedule")) {
            ThreadUtils.assertOnUiThread();
            Map<Integer, ScheduledTaskProto.ScheduledTask> scheduledTasks =
                    BackgroundTaskSchedulerPrefs.getScheduledTasks();
            BackgroundTaskSchedulerPrefs.removeAllTasks();
            for (Map.Entry<Integer, ScheduledTaskProto.ScheduledTask> entry :
                    scheduledTasks.entrySet()) {
                final BackgroundTask backgroundTask =
                        BackgroundTaskSchedulerFactoryInternal.getBackgroundTaskFromTaskId(
                                entry.getKey());
                if (backgroundTask == null) {
                    Log.w(TAG,
                            "Cannot reschedule task for task id " + entry.getKey() + ". Could not "
                                    + "instantiate BackgroundTask class.");
                    // Cancel task if the BackgroundTask class is not found anymore. We assume this
                    // means that the task has been deprecated.
                    selectDelegateAndCancel(context, entry.getValue().getType(), entry.getKey());
                    continue;
                }

                BackgroundTaskSchedulerUma.getInstance().reportTaskRescheduled();
                backgroundTask.reschedule(context);
            }
        }
    }

    private void selectDelegateAndCancel(
            Context context, ScheduledTaskProto.ScheduledTask.Type taskType, int taskId) {
        if (taskType == ScheduledTaskProto.ScheduledTask.Type.EXACT) {
            mAlarmManagerDelegate.cancel(context, taskId);
        } else {
            mSchedulerDelegate.cancel(context, taskId);
        }
    }
}
