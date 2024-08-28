// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.text.TextUtils;
import android.util.Log;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Deals with Document-related API calls. */
public class AndroidTaskUtils {
    public static final String TAG = "DocumentUtilities";

    // Typically the number of tasks returned by getRecentTasks will be around 3 or less - the
    // Chrome Launcher Activity, a Tabbed Activity task, and the home screen on older Android
    // versions. However, theoretically this task list could be unbounded, so limit it to a number
    // that won't cause Chrome to blow up in degenerate cases.
    private static final int MAX_NUM_TASKS = 100;

    /**
     * Finishes tasks other than the one with the given ID that were started with the given data
     * in the Intent, removing those tasks from Recents and leaving a unique task with the data.
     * @param data Passed in as part of the Intent's data when starting the Activity.
     * @param canonicalTaskId ID of the task will be the only one left with the ID.
     * @return Intent of one of the tasks that were finished.
     */
    public static Intent finishOtherTasksWithData(Uri data, int canonicalTaskId) {
        if (data == null) return null;

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

    /**
     * Get all recent tasks with component name matching any of the given names.
     * @param context the Android Context
     * @param componentsAccepted the set of names accepted
     * @return all matching recent {@link AppTask} and their respective {@link RecentTaskInfo}
     */
    public static Set<Pair<AppTask, RecentTaskInfo>> getRecentAppTasksMatchingComponentNames(
            Context context, Set<String> componentsAccepted) {
        HashSet<Pair<AppTask, RecentTaskInfo>> matchingTasks = new HashSet<>();

        ActivityManager manager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);

        for (AppTask task : manager.getAppTasks()) {
            RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
            if (info == null) continue;
            String componentName = AndroidTaskUtils.getTaskComponentName(task);

            if (componentsAccepted.contains(componentName)) {
                matchingTasks.add(Pair.create(task, info));
            }
        }
        return matchingTasks;
    }

    /**
     * Get all recent tasks infos with component name matching any of the given names.
     * @param context the Android Context
     * @param componentsAccepted the set of names accepted
     * @return all matching {@link RecentTaskInfo}s
     */
    public static Set<RecentTaskInfo> getRecentTaskInfosMatchingComponentNames(
            Context context, Set<String> componentsAccepted) throws SecurityException {
        HashSet<RecentTaskInfo> matchingInfos = new HashSet<>();

        final ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);

        // getRecentTasks is deprecated, but still returns your app's tasks, and does so
        // without needing an extra IPC for each task you want to get the info for. It also
        // includes some known-safe tasks like the home screen on older Android versions, but
        // that's fine for this purpose.
        List<ActivityManager.RecentTaskInfo> tasks =
                activityManager.getRecentTasks(MAX_NUM_TASKS, 0);
        if (tasks != null) {
            for (ActivityManager.RecentTaskInfo task : tasks) {
                // Note that Android documentation lies, and TaskInfo#origActivity does not
                // actually return the target of an alias, so we have to explicitly check
                // for the target component of the base intent, which will have been set to
                // the Activity that launched, in order to make this check more robust.
                ComponentName component = task.baseIntent.getComponent();
                if (component == null) continue;
                if (componentsAccepted.contains(component.getClassName())
                        && component.getPackageName().equals(context.getPackageName())) {
                    matchingInfos.add(task);
                }
            }
        }
        return matchingInfos;
    }

    /**
     * Get the {@link AppTask} for a given taskId.
     *
     * @param context The activity context.
     * @param taskId The id of the task whose AppTask will be returned.
     * @return The {@link AppTask} for a given taskId if found, {@code null} otherwise.
     */
    public static @Nullable AppTask getAppTaskFromId(Context context, int taskId) {
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        for (var appTask : am.getAppTasks()) {
            var taskInfo = appTask.getTaskInfo();
            if (taskInfo == null) continue;
            int taskInfoId = taskInfo.id;
            if (VERSION.SDK_INT >= VERSION_CODES.Q) {
                taskInfoId = taskInfo.taskId;
            }
            if (taskInfoId == taskId) {
                return appTask;
            }
        }
        return null;
    }
}
