// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.Network;
import android.os.BatteryManager;
import android.os.PowerManager;
import android.os.SystemClock;
import android.text.format.DateUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.compat.ApiHelperForM;
import org.chromium.base.task.PostTask;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Starts running the BackgroundTask at the specified triggering time.
 *
 * Receives the information through a broadcast, which is synchronous in the Main thread. The
 * execution of the task will be detached to a best effort task.
 */
public class BackgroundTaskBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = "BkgrdTaskBR";
    private static final String WAKELOCK_TAG = "Chromium:" + TAG;

    // Wakelock is only held for 3 minutes, in order to be consistent with the restrictions of
    // the GcmTaskService, which was used in earlier versions of Chrome on pre-Android M versions
    // of Android:
    // https://developers.google.com/android/reference/com/google/android/gms/gcm/GcmTaskService.
    // Here the waiting is done for only 90% of this time.
    private static final long MAX_TIMEOUT_MS = 162 * DateUtils.SECOND_IN_MILLIS;

    private static class TaskExecutor implements BackgroundTask.TaskFinishedCallback {
        private final Context mContext;
        private final PowerManager.WakeLock mWakeLock;
        private final TaskParameters mTaskParams;
        private final BackgroundTask mBackgroundTask;
        private final long mTaskStartTimeMs;

        private boolean mHasExecuted;

        public TaskExecutor(Context context, PowerManager.WakeLock wakeLock,
                TaskParameters taskParams, BackgroundTask backgroundTask) {
            mContext = context;
            mWakeLock = wakeLock;
            mTaskParams = taskParams;
            mBackgroundTask = backgroundTask;
            mTaskStartTimeMs = SystemClock.uptimeMillis();
        }

        public void execute() {
            ThreadUtils.assertOnUiThread();

            boolean needsBackground = mBackgroundTask.onStartTask(mContext, mTaskParams, this);
            BackgroundTaskSchedulerUma.getInstance().reportTaskStarted(mTaskParams.getTaskId());
            if (!needsBackground) return;

            PostTask.postDelayedTask(UiThreadTaskTraits.BEST_EFFORT, this::timeout, MAX_TIMEOUT_MS);
        }

        @Override
        public void taskFinished(boolean needsReschedule) {
            PostTask.postTask(UiThreadTaskTraits.BEST_EFFORT, () -> finished(needsReschedule));
        }

        private void timeout() {
            ThreadUtils.assertOnUiThread();
            if (mHasExecuted) return;
            mHasExecuted = true;

            Log.w(TAG, "Task execution failed. Task timed out.");
            BackgroundTaskSchedulerUma.getInstance().reportTaskStopped(mTaskParams.getTaskId());

            boolean reschedule = mBackgroundTask.onStopTask(mContext, mTaskParams);
            if (reschedule) {
                BackgroundTaskSchedulerUma.getInstance().reportTaskRescheduled();
                mBackgroundTask.reschedule(mContext);
            }
        }

        private void finished(boolean reschedule) {
            ThreadUtils.assertOnUiThread();
            if (mHasExecuted) return;
            mHasExecuted = true;

            if (reschedule) {
                BackgroundTaskSchedulerUma.getInstance().reportTaskRescheduled();
                mBackgroundTask.reschedule(mContext);
            }
            BackgroundTaskSchedulerUma.getInstance().reportTaskFinished(
                    mTaskParams.getTaskId(), SystemClock.uptimeMillis() - mTaskStartTimeMs);
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        final TaskParameters taskParams =
                BackgroundTaskSchedulerAlarmManager.getTaskParametersFromIntent(intent);
        if (taskParams == null) {
            Log.w(TAG, "Failed to retrieve task parameters.");
            return;
        }

        int taskId = taskParams.getTaskId();
        ScheduledTaskProto.ScheduledTask scheduledTask =
                BackgroundTaskSchedulerPrefs.getScheduledTask(taskId);
        if (scheduledTask == null) {
            Log.e(TAG, "Cannot get information about task with task ID " + taskId);
            return;
        }

        // Only continue if network requirements match network status.
        if (!networkRequirementsMet(context, taskId,
                    convertToTaskInfoNetworkType(scheduledTask.getRequiredNetworkType()))) {
            Log.w(TAG,
                    "Failed to start task. Network requirements not satisfied for task with task ID"
                            + taskId);
            return;
        }

        // Check if battery requirements match.
        if (!batteryRequirementsMet(context, taskId, scheduledTask.getRequiresCharging())) {
            Log.w(TAG,
                    "Failed to start task. Battery requirements not satisfied for task with task ID"
                            + taskId);
            return;
        }

        final BackgroundTask backgroundTask =
                BackgroundTaskSchedulerFactoryInternal.getBackgroundTaskFromTaskId(taskId);
        if (backgroundTask == null) {
            Log.w(TAG, "Failed to start task. Could not instantiate BackgroundTask class.");
            // Cancel task if the BackgroundTask class is not found anymore. We assume this means
            // that the task has been deprecated.
            BackgroundTaskSchedulerFactoryInternal.getScheduler().cancel(
                    ContextUtils.getApplicationContext(), taskParams.getTaskId());
            return;
        }

        // Keep the CPU on through a wake lock.
        PowerManager.WakeLock wakeLock = null;
        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        if (powerManager != null) {
            wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, WAKELOCK_TAG);
            wakeLock.acquire(MAX_TIMEOUT_MS);
        }
        TaskExecutor taskExecutor = new TaskExecutor(context, wakeLock, taskParams, backgroundTask);
        PostTask.postTask(UiThreadTaskTraits.BEST_EFFORT, taskExecutor::execute);
    }

    private boolean networkRequirementsMet(Context context, int taskId,
            @Nullable @TaskInfo.NetworkType Integer requiredNetworkType) {
        if (requiredNetworkType == TaskInfo.NetworkType.NONE) return true;

        ConnectivityManager connectivityManager =
                (ConnectivityManager) context.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        Network network = ApiHelperForM.getActiveNetwork(connectivityManager);
        if (requiredNetworkType == TaskInfo.NetworkType.ANY) return (network != null);

        return (!connectivityManager.isActiveNetworkMetered());
    }

    private boolean batteryRequirementsMet(Context context, int taskId, boolean requiresCharging) {
        if (!requiresCharging) return true;
        BatteryManager batteryManager =
                (BatteryManager) context.getSystemService(Context.BATTERY_SERVICE);

        return batteryManager.isCharging();
    }

    private @Nullable @TaskInfo.NetworkType Integer convertToTaskInfoNetworkType(
            ScheduledTaskProto.ScheduledTask.RequiredNetworkType networkType) {
        switch (networkType) {
            case NONE:
                return TaskInfo.NetworkType.NONE;
            case ANY:
                return TaskInfo.NetworkType.ANY;
            case UNMETERED:
                return TaskInfo.NetworkType.UNMETERED;
            default:
                assert false : "Incorrect value of RequiredNetworkType";
                return null;
        }
    }
}
