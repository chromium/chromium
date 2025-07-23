// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.app.Activity;
import android.util.ArrayMap;

import androidx.annotation.GuardedBy;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.Map;

/** Implements {@link ChromeAndroidTaskTracker} as a singleton. */
@NullMarked
final class ChromeAndroidTaskTrackerImpl implements ChromeAndroidTaskTracker {

    private static @Nullable ChromeAndroidTaskTrackerImpl sInstance;

    /** Maps {@link ChromeAndroidTask} IDs to their instances. */
    @GuardedBy("mTasksLock")
    private final Map<Integer, ChromeAndroidTask> mTasks = new ArrayMap<>();

    private final Object mTasksLock = new Object();

    static ChromeAndroidTaskTrackerImpl getInstance() {
        if (sInstance == null) {
            sInstance = new ChromeAndroidTaskTrackerImpl();
        }
        return sInstance;
    }

    private ChromeAndroidTaskTrackerImpl() {}

    @Override
    public ChromeAndroidTask obtainTask(ActivityWindowAndroid activityWindowAndroid) {
        int taskId = getTaskId(activityWindowAndroid);

        synchronized (mTasksLock) {
            var existingTask = mTasks.get(taskId);
            if (existingTask != null) {
                existingTask.setActivityWindowAndroid(activityWindowAndroid);
                return existingTask;
            }

            var newTask = new ChromeAndroidTaskImpl(activityWindowAndroid);
            mTasks.put(taskId, newTask);
            return newTask;
        }
    }

    @Override
    @Nullable
    public ChromeAndroidTask get(int taskId) {
        synchronized (mTasksLock) {
            return mTasks.get(taskId);
        }
    }

    @Override
    public void remove(int taskId) {
        ChromeAndroidTask taskRemoved;

        synchronized (mTasksLock) {
            taskRemoved = mTasks.remove(taskId);
        }

        if (taskRemoved != null) {
            taskRemoved.destroy();
        }
    }

    /**
     * Removes all {@link ChromeAndroidTask}s.
     *
     * <p>As this class is a singleton, we need to clear all {@link ChromeAndroidTask}s after each
     * test.
     *
     * <p>This method must be called on the UI thread.
     */
    public void removeAllForTesting() {
        synchronized (mTasksLock) {
            mTasks.forEach((taskId, task) -> task.destroy());
            mTasks.clear();
        }
    }

    private static int getTaskId(ActivityWindowAndroid activityWindowAndroid) {
        Activity activity = activityWindowAndroid.getActivity().get();
        assert activity != null : "ActivityWindowAndroid should have an Activity.";

        return activity.getTaskId();
    }
}
