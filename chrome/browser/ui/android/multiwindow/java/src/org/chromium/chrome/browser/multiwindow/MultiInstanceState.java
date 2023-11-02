// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.util.SparseBooleanArray;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/**
 * Tracks multi-instance mode of Chrome browser app.
 *
 * <p>The class works by observing the lifecycle of application's various Activity objects. Chrome
 * browser tasks have {@link ChromeTabbedActivity} at the bottom of the stack and other activities
 * on top of it. Their state is tracked to keep the multi-instance state up to date.
 */
public class MultiInstanceState implements ActivityStateListener {
    private static MultiInstanceState sInstance;

    /** Observer used to notify multi-instance state change. */
    public interface MultiInstanceStateObserver {
        /**
         * Called whenever multi-instance state is flipped.
         * @param inMultiInstanceMode Whether multiple instances are visible on screen.
         */
        void onMultiInstanceStateChanged(boolean inMultiInstanceMode);
    }

    /**
     * Predicate returning true if a given activity can be the base activity for Chrome task.
     */
    public interface BaseActivityName {
        boolean is(String baseActivity);
    }

    private final Supplier<List<AppTask>> mAppTaskSupplier;
    private final BaseActivityName mBaseActivityName;

    // Task visibility observer list.
    private final ObserverList<MultiInstanceStateObserver> mObservers = new ObserverList<>();

    // Stores the current multi-instance state.
    private boolean mIsInMultiInstanceMode;

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
        ApplicationStatus.registerStateListenerForAllActivities(this);
        mAppTaskSupplier = appTaskSupplier;
        mBaseActivityName = baseActivityName;
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (newState != ActivityState.RESUMED && newState != ActivityState.PAUSED
                && newState != ActivityState.STOPPED) {
            return;
        }
        boolean isInMultiInstanceMode = isInMultiInstanceMode();
        if (isInMultiInstanceMode != mIsInMultiInstanceMode) {
            mIsInMultiInstanceMode = isInMultiInstanceMode;
            Iterator<MultiInstanceStateObserver> it = mObservers.iterator();
            while (it.hasNext()) it.next().onMultiInstanceStateChanged(mIsInMultiInstanceMode);
        }
    }

    private Set<Integer> getAllTaskIds() {
        Set<Integer> results = new HashSet<>();
        for (AppTask task : mAppTaskSupplier.get()) {
            ActivityManager.RecentTaskInfo taskInfo = AndroidTaskUtils.getTaskInfoFromTask(task);
            if (taskInfo == null || taskInfo.baseActivity == null) continue;
            String baseActivity = taskInfo.baseActivity.getClassName();
            if (mBaseActivityName.is(baseActivity)) results.add(taskInfo.id);
        }
        return results;
    }

    public boolean isInMultiInstanceMode() {
        Set<Integer> tasks = getAllTaskIds();
        if (tasks.size() < 2) return false;

        SparseBooleanArray visibleTasks = new SparseBooleanArray();
        List<Activity> activities = ApplicationStatus.getRunningActivities();
        for (Activity a : activities) {
            int taskId = a.getTaskId();
            if (!tasks.contains(taskId)) continue;
            int state = ApplicationStatus.getStateForActivity(a);
            if (state == ActivityState.RESUMED || state == ActivityState.PAUSED) {
                visibleTasks.put(taskId, true);
            }
        }
        return visibleTasks.size() > 1;
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
        // TODO(jinsukkim): Do the cleanup when the last base activity is destroyed.
        ApplicationStatus.unregisterActivityStateListener(this);
        mObservers.clear();
        sInstance = null;
    }

    public static MultiInstanceState getInstanceForTesting() {
        return sInstance;
    }
}
