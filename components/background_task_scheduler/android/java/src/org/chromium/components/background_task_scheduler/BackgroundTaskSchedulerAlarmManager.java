// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;

/**
 * An implementation of {@link BackgroundTaskSchedulerDelegate} that uses the system API
 * {@link AlarmManager} to schedule jobs.
 */
public class BackgroundTaskSchedulerAlarmManager implements BackgroundTaskSchedulerDelegate {
    private static final String TAG = "BkgrdTaskSchedulerAM";

    @VisibleForTesting
    static PendingIntent createPendingIntentFromTaskId(Context context, int taskId) {
        Intent intent = new Intent(context, BackgroundTaskBroadcastReceiver.class)
                                .putExtra(BACKGROUND_TASK_ID_KEY, taskId);
        return PendingIntent.getBroadcast(
                context, taskId, intent, PendingIntent.FLAG_CANCEL_CURRENT);
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

        @Override
        public void visit(TaskInfo.ExactInfo exactInfo) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                mAlarmManager.setExactAndAllowWhileIdle(
                        /*type= */ AlarmManager.RTC_WAKEUP, exactInfo.getTriggerAtMs(),
                        mPendingIntent);
                return;
            }

            mAlarmManager.setExact(
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
