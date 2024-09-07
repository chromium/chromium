// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.util.SparseBooleanArray;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

import java.util.List;

/**
 * Tracks multi-instance mode of Chrome browser app.
 *
 * <p>The class works by observing the lifecycle of application's various Activity objects. Chrome
 * browser tasks have {@link ChromeTabbedActivity} at the bottom of the stack and other activities
 * on top of it. Their state is tracked to keep the multi-instance state up to date.
 */
public class MultiInstanceState implements ApplicationStatus.TaskVisibilityListener {
    private static MultiInstanceState sInstance;

    /** Observer used to notify multi-instance state change. **/
    public interface MultiInstanceStateObserver {
        /**
         * Called whenever multi-instance state is flipped.
         * @param inMultiInstanceMode Whether multiple instances are visible on screen.
         */
        void onMultiInstanceStateChanged(boolean inMultiInstanceMode);
    }

    /** Predicate returning true if a given activity can be the base activity for Chrome task. */
    public interface BaseActivityName {
        boolean is(String baseActivity);
    }

    private final Supplier<List<AppTask>> mAppTaskSupplier;
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
        if (!isRelevantTaskId(taskId)) return;

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
        sInstance = null;
    }

    public static MultiInstanceState getInstanceForTesting() {
        return sInstance;
    }
}
