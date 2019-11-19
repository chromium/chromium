// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Deals with Document-related API calls.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class AndroidTaskUtils {
    public static final String TAG = "DocumentUtilities";

    /**
     * Finishes tasks other than the one with the given ID that were started with the given data
     * in the Intent, removing those tasks from Recents and leaving a unique task with the data.
     * @param data Passed in as part of the Intent's data when starting the Activity.
     * @param canonicalTaskId ID of the task will be the only one left with the ID.
     * @return Intent of one of the tasks that were finished.
     */
    public static Intent finishOtherTasksWithData(Uri data, int canonicalTaskId) {
        if (data == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return null;

        String dataString = data.toString();
        Context context = ContextUtils.getApplicationContext();

        ActivityManager manager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.AppTask> tasksToFinish = new ArrayList<ActivityManager.AppTask>();
        for (ActivityManager.AppTask task : manager.getAppTasks()) {
            RecentTaskInfo taskInfo = getTaskInfoFromTask(task);
            if (taskInfo == null) continue;
            int taskId = taskInfo.id;

            Intent baseIntent = taskInfo.baseIntent;
            String taskData = baseIntent == null ? null : taskInfo.baseIntent.getDataString();

            if (TextUtils.equals(dataString, taskData)
                    && (taskId == -1 || taskId != canonicalTaskId)) {
                tasksToFinish.add(task);
            }
        }
        return finishAndRemoveTasks(tasksToFinish);
    }

    private static Intent finishAndRemoveTasks(List<ActivityManager.AppTask> tasksToFinish) {
        Intent removedIntent = null;
        for (ActivityManager.AppTask task : tasksToFinish) {
            Log.d(TAG, "Removing task with duplicated data: " + task);
            removedIntent = getBaseIntentFromTask(task);
            task.finishAndRemoveTask();
        }
        return removedIntent;
    }

    /**
     * Returns the RecentTaskInfo for the task, if the ActivityManager succeeds in finding the task.
     * @param task AppTask containing information about a task.
     * @return The RecentTaskInfo associated with the task, or null if it couldn't be found.
     */
    public static RecentTaskInfo getTaskInfoFromTask(AppTask task) {
        RecentTaskInfo info = null;
        try {
            info = task.getTaskInfo();
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Failed to retrieve task info: ", e);
        }
        return info;
    }

    /**
     * Returns the baseIntent of the RecentTaskInfo associated with the given task.
     * @param task Task to get the baseIntent for.
     * @return The baseIntent, or null if it couldn't be retrieved.
     */
    public static Intent getBaseIntentFromTask(AppTask task) {
        RecentTaskInfo info = getTaskInfoFromTask(task);
        return info == null ? null : info.baseIntent;
    }

    /**
     * Given an AppTask retrieves the task component name.
     * @param task The app task to use.
     * @return Fully qualified component name name or null if we were not able to
     * determine it.
     */
    public static String getTaskComponentName(AppTask task) {
        RecentTaskInfo info = getTaskInfoFromTask(task);
        if (info == null) return null;

        Intent baseIntent = info.baseIntent;
        if (baseIntent == null) {
            return null;
        } else if (baseIntent.getComponent() != null) {
            return baseIntent.getComponent().getClassName();
        } else {
            ResolveInfo resolveInfo = PackageManagerUtils.resolveActivity(baseIntent, 0);
            if (resolveInfo == null) return null;
            return resolveInfo.activityInfo.name;
        }
    }
}
