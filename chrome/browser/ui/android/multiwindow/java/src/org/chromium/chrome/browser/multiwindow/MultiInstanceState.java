// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.util.SparseBooleanArray;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

import java.util.List;
import java.util.function.Supplier;

/**
 * Tracks multi-instance mode of Chrome browser app.
 *
 * <p>The class works by observing the lifecycle of application's various Activity objects. Chrome
 * browser tasks have {@link ChromeTabbedActivity} at the bottom of the stack and other activities
 * on top of it. Their state is tracked to keep the multi-instance state up to date.
 */
@NullMarked
public class MultiInstanceState implements ApplicationStatus.TaskVisibilityListener {
    private static @Nullable MultiInstanceState sInstance;

    /** Observer used to notify multi-instance state change. */
    public interface MultiInstanceStateObserver {
        /**
         * Called whenever multi-instance state is flipped.
         *
         * @param inMultiInstanceMode Whether multiple instances are visible on screen.
         */
        void onMultiInstanceStateChanged(boolean inMultiInstanceMode);
    }

    /** Predicate returning true if a given activity can be the base activity for Chrome task. */
    public interface BaseActivityName {
        boolean is(String baseActivity);
    }

    @Nullable private Supplier<List<AppTask>> mAppTaskSupplier;
    private final BaseActivityName mBaseActivityName;

    // Task visibility observer list.
    private final ObserverList<MultiInstanceStateObserver> mObservers = new ObserverList<>();
    // Tracks the taskId of the currently visible tasks.
    private final SparseBooleanArray mActiveTasks = new SparseBooleanArray();

    /**
     * Create a singleton instance of {@link MultiInstanceState} object, if not already available.
     * @param appTaskSupplier Supplier of a list of the current Chrome app tasks.
     * @param baseActivityName Predicate that tells if a given string is a legitimate name of
     *     the base activity of Chrome task.
     */
    public static MultiInstanceState maybeCreate(
            Supplier<List<AppTask>> appTaskSupplier, BaseActivityName baseActivityName) {
        if (sInstance == null) {
            sInstance = new MultiInstanceState(appTaskSupplier, baseActivityName);
        }
        return sInstance;
    }

    private MultiInstanceState(
            Supplier<List<AppTask>> appTaskSupplier, BaseActivityName baseActivityName) {
        ApplicationStatus.registerTaskVisibilityListener(this);
        mAppTaskSupplier = appTaskSupplier;
        mBaseActivityName = baseActivityName;
    }

    private boolean isRelevantTaskId(int taskId) {
        assert mAppTaskSupplier != null;
        for (AppTask task : mAppTaskSupplier.get()) {
            ActivityManager.RecentTaskInfo taskInfo = AndroidTaskUtils.getTaskInfoFromTask(task);
            if (taskInfo == null || taskInfo.baseActivity == null || taskInfo.id != taskId) {
                continue;
            }
            String baseActivity = taskInfo.baseActivity.getClassName();
            if (mBaseActivityName.is(baseActivity)) return true;
        }
        return false;
    }

    public boolean isInMultiInstanceMode() {
        return mActiveTasks.size() > 1;
    }

    @Override
    public void onTaskVisibilityChanged(int taskId, boolean isVisible) {
        // Tasks are only added to mActiveTasks after passing isRelevantTaskId(), and a task's
        // baseActivity is immutable for its lifetime. Checking mActiveTasks first avoids
        // synchronous getAppTasks()/getTaskInfo() Binder IPCs when:
        // - an untracked task hides (deleting it from mActiveTasks would be a no-op),
        // - an already-tracked task hides (it was already validated when shown, and if it is
        //   hiding due to finishAndRemoveTask(), getAppTasks() may no longer report it), or
        // - an already-tracked task reports visible again.
        if (!isVisible) {
            if (mActiveTasks.indexOfKey(taskId) < 0) return;
        } else if (mActiveTasks.indexOfKey(taskId) >= 0 || !isRelevantTaskId(taskId)) {
            return;
        }

        boolean multiInstanceState = isInMultiInstanceMode();
        if (isVisible) {
            mActiveTasks.append(taskId, true);
        } else {
            mActiveTasks.delete(taskId);
        }
        boolean newMultiInstanceState = isInMultiInstanceMode();

        if (multiInstanceState != newMultiInstanceState) {
            for (MultiInstanceStateObserver o : mObservers) {
                o.onMultiInstanceStateChanged(newMultiInstanceState);
            }
        }
    }

    /**
     * Add an observer that monitors the multi-instance state of Chrome app.
     * @param o {@link MultiInstanceStateObserver} object.
     */
    public void addObserver(MultiInstanceStateObserver o) {
        mObservers.addObserver(o);
    }

    /**
     * Removes an observer that monitors the multi-instance state of Chrome app.
     * @param o {@link MultiInstanceStateObserver} object.
     */
    public void removeObserver(MultiInstanceStateObserver o) {
        mObservers.removeObserver(o);
    }

    void clear() {
        ApplicationStatus.unregisterTaskVisibilityListener(this);
        mObservers.clear();
        mAppTaskSupplier = null;
        sInstance = null;
    }

    public static @Nullable MultiInstanceState getInstanceForTesting() {
        return sInstance;
    }
}
