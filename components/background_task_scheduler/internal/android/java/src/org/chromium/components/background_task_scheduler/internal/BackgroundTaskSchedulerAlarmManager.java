// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;

/**
 * An implementation of {@link BackgroundTaskSchedulerDelegate} that uses the system API
 * {@link AlarmManager} to schedule jobs.
 */
public class BackgroundTaskSchedulerAlarmManager implements BackgroundTaskSchedulerDelegate {
    private static final String TAG = "BkgrdTaskSchedulerAM";

    /**
     * Retrieves the {@link TaskParameters} from the {@link Intent}.
     *
     * @param intent the {@link Intent} to extract the {@link TaskParameters} from.
     * @return the {@link TaskParameters} for the current job.
     */
    static TaskParameters getTaskParametersFromIntent(Intent intent) {
        Bundle extras = intent.getExtras();

        int taskId = extras.getInt(BACKGROUND_TASK_ID_KEY, /* defaultValue= */ 0);
        if (taskId == 0) {
            Log.e(TAG, "Cannot not get task ID from intent extras.");
            return null;
        }

        ScheduledTaskProto.ScheduledTask scheduledTask =
                BackgroundTaskSchedulerPrefs.getScheduledTask(taskId);

        if (scheduledTask == null) {
            Log.e(TAG, "Cannot get information about task with task ID " + taskId);
            return null;
        }

        Bundle taskExtras =
                ExtrasToProtoConverter.convertProtoExtrasToExtras(scheduledTask.getExtrasList());
        if (taskExtras == null) {
            Log.e(TAG, "Cannot get extras data for task ID " + taskId);
            return null;
        }

        TaskParameters.Builder builder = TaskParameters.create(taskId);
        builder.addExtras(taskExtras);
        return builder.build();
    }

    @VisibleForTesting
    static PendingIntent createPendingIntentFromTaskId(Context context, int taskId) {
        Intent intent = new Intent(context, BackgroundTaskBroadcastReceiver.class)
                                .putExtra(BACKGROUND_TASK_ID_KEY, taskId);
        return PendingIntent.getBroadcast(context, taskId, intent,
                PendingIntent.FLAG_CANCEL_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
    }

    @Override
    public boolean schedule(Context context, TaskInfo taskInfo) {
        ThreadUtils.assertOnUiThread();

        AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        PendingIntent pendingIntent = createPendingIntentFromTaskId(context, taskInfo.getTaskId());

        AlarmManagerVisitor alarmManagerVisitor =
                new AlarmManagerVisitor(alarmManager, pendingIntent);
        taskInfo.getTimingInfo().accept(alarmManagerVisitor);

        return true;
    }

    private static class AlarmManagerVisitor implements TaskInfo.TimingInfoVisitor {
        private AlarmManager mAlarmManager;
        private PendingIntent mPendingIntent;

        AlarmManagerVisitor(AlarmManager alarmManager, PendingIntent pendingIntent) {
            mAlarmManager = alarmManager;
            mPendingIntent = pendingIntent;
        }

        @Override
        public void visit(TaskInfo.OneOffInfo oneOffInfo) {
            throw new RuntimeException("One-off tasks should not be scheduled with "
                    + "AlarmManager.");
        }

        @Override
        public void visit(TaskInfo.PeriodicInfo periodicInfo) {
            throw new RuntimeException("Periodic tasks should not be scheduled with "
                    + "AlarmManager.");
        }

        // TODO(crbug.com/1190755): Either remove this or make sure it's compatible with Android S.
        @Override
        public void visit(TaskInfo.ExactInfo exactInfo) {
            mAlarmManager.setExactAndAllowWhileIdle(
                    /*type= */ AlarmManager.RTC_WAKEUP, exactInfo.getTriggerAtMs(), mPendingIntent);
        }
    }

    @Override
    public void cancel(Context context, int taskId) {
        ThreadUtils.assertOnUiThread();

        PendingIntent pendingIntent = createPendingIntentFromTaskId(context, taskId);
        AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        alarmManager.cancel(pendingIntent);
    }
}
