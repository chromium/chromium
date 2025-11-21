// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Intent;
import android.graphics.Rect;
import android.util.ArrayMap;

import androidx.annotation.GuardedBy;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.PendingTaskInfo;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

/** Implements {@link ChromeAndroidTaskTracker} as a singleton. */
@NullMarked
final class ChromeAndroidTaskTrackerImpl implements ChromeAndroidTaskTracker {

    private static @Nullable ChromeAndroidTaskTrackerImpl sInstance;

    private static boolean sPausePendingTaskActivityCreationForTesting;

    /**
     * Maps pending {@link ChromeAndroidTask} IDs to their {@link PendingTaskInfo}s.
     *
     * <p>Used only for testing and for when {@link #sPausePendingTaskActivityCreationForTesting} is
     * {@code true}.
     */
    private static final Map<Integer, PendingTaskInfo>
            sPendingTasksAwaitingActivityCreationForTesting = new ArrayMap<>();

    /**
     * Maps {@link ChromeAndroidTask} IDs to their instances. This reflects the {@link
     * ChromeAndroidTask}'s ID when it is alive, and is different from its ID in the pending state.
     */
    @GuardedBy("mTasksLock")
    private final Map<Integer, ChromeAndroidTask> mTasks = new ArrayMap<>();

    /**
     * Maps pending {@link ChromeAndroidTask} IDs to their instances. This reflects the {@link
     * ChromeAndroidTask}'s ID when it is pending, and is different from its ID in the alive state.
     */
    @GuardedBy("mTasksLock")
    private final Map<Integer, ChromeAndroidTask> mPendingTasks = new ArrayMap<>();

    /** List of observers currently observing this instance. */
    @GuardedBy("mTasksLock")
    private final List<ChromeAndroidTaskTrackerObserver> mObservers = new ArrayList();

    private final Object mTasksLock = new Object();

    static ChromeAndroidTaskTrackerImpl getInstance() {
        if (sInstance == null) {
            sInstance = new ChromeAndroidTaskTrackerImpl();
        }
        return sInstance;
    }

    private ChromeAndroidTaskTrackerImpl() {}

    @Override
    public ChromeAndroidTask obtainTask(
            @BrowserWindowType int browserWindowType,
            ChromeAndroidTask.ActivityScopedObjects activityScopedObjects,
            @Nullable Integer pendingId) {
        int taskId = getTaskId(activityScopedObjects.mActivityWindowAndroid);

        synchronized (mTasksLock) {
            var existingTask = mTasks.get(taskId);
            if (existingTask != null) {
                assert existingTask.getBrowserWindowType() == browserWindowType
                        : "The browser window type of an existing task can't be changed.";
                existingTask.setActivityScopedObjects(activityScopedObjects);
                return existingTask;
            }

            if (pendingId != null) {
                ChromeAndroidTask pendingTask = mPendingTasks.remove(pendingId);
                assert pendingTask != null : "Invalid pendingId provided.";
                pendingTask.setActivityScopedObjects(activityScopedObjects);
                mTasks.put(taskId, pendingTask);
                return pendingTask;
            }

            var newTask = new ChromeAndroidTaskImpl(browserWindowType, activityScopedObjects);
            mTasks.put(taskId, newTask);
            mObservers.forEach((observer) -> observer.onTaskAdded(newTask));
            return newTask;
        }
    }

    @Override
    @Nullable
    public ChromeAndroidTask createPendingTask(
            AndroidBrowserWindowCreateParams createParams,
            @Nullable JniOnceCallback<Long> callback) {
        synchronized (mTasksLock) {
            Intent newWindowIntent = createNewWindowIntentLocked(createParams);
            if (newWindowIntent == null) {
                if (callback != null) {
                    callback.onResult(0L);
                }
                return null;
            }

            int pendingId = IdSequencer.next();
            newWindowIntent.putExtra(EXTRA_PENDING_BROWSER_WINDOW_TASK_ID, pendingId);

            var pendingTaskInfo =
                    new PendingTaskInfo(pendingId, createParams, newWindowIntent, callback);
            var pendingTask = new ChromeAndroidTaskImpl(pendingTaskInfo);
            mPendingTasks.put(pendingId, pendingTask);

            // Launch the required Activity based on |createParams|.
            if (!sPausePendingTaskActivityCreationForTesting) {
                launchNewWindowIntent(newWindowIntent, createParams.getInitialBounds());
            } else {
                sPendingTasksAwaitingActivityCreationForTesting.put(pendingId, pendingTaskInfo);
            }
            return pendingTask;
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
        synchronized (mTasksLock) {
            removeInternalLocked(taskId);
        }
    }

    @Override
    public void onActivityWindowAndroidDestroy(ActivityWindowAndroid activityWindowAndroid) {
        synchronized (mTasksLock) {
            int taskId = getTaskId(activityWindowAndroid);
            var task = mTasks.get(taskId);

            if (task == null) {
                return;
            }

            // If the ActivityWindowAndroid that's passed in isn't the ActivityWindowAndroid held by
            // this ChromeAndroidTask, don't do anything.
            //
            // This scenario can happen if one Android Task contains more than one Activity with
            // ActivityWindowAndroid. An example Task stack:
            //
            // [
            //   CustomTabActivity    (top Activity)
            //   ChromeTabbedActivity (root Activity that created ChromeAndroidTask)
            // ]
            //
            // In the example, each of the two Activities has an ActivityWindowAndroid. When the
            // user leaves CustomTabActivity, its ActivityWindowAndroid will call
            // onActivityWindowAndroidDestroy() on the ChromeAndroidTask created by
            // ChromeTabbedActivity as the two Activities are in the same Task. However, this isn't
            // the right time to clear and destroy the ActivityWindowAndroid held by the
            // ChromeAndroidTask.
            if (task.getActivityWindowAndroid() != activityWindowAndroid) {
                return;
            }

            task.clearActivityScopedObjects();

            // It's not 100% correct to destroy the ChromeAndroidTask here as a ChromeAndroidTask
            // is meant to track an Android Task, but an ActivityWindowAndroid is associated with a
            // ChromeActivity.
            //
            // However, as of July 22, 2025, Android framework doesn't provide an API that listens
            // for Task removal, so we need to destroy the ChromeAndroidTask along with
            // ActivityWindowAndroid as a workaround.
            //
            // In the future, we can register a Task listener when a ChromeAndroidTask is created,
            // then destroy it when notified of the Task removal.
            removeInternalLocked(taskId);
        }
    }

    @Override
    public void addObserver(ChromeAndroidTaskTrackerObserver observer) {
        synchronized (mTasksLock) {
            mObservers.add(observer);
        }
    }

    @Override
    public boolean removeObserver(ChromeAndroidTaskTrackerObserver observer) {
        synchronized (mTasksLock) {
            return mObservers.remove(observer);
        }
    }

    /** Returns an array of the native {@code BrowserWindowInterface} addresses. */
    long[] getAllNativeBrowserWindowPtrs() {
        synchronized (mTasksLock) {
            return getNativeBrowserWindowPtrsLocked(getAllTasksLocked());
        }
    }

    /**
     * Returns an array of the native {@code BrowserWindowInterface} addresses, sorted by the
     * descending order of {@link ChromeAndroidTask#getLastActivatedTimeMillis()}.
     */
    long[] getNativeBrowserWindowPtrsOrderedByActivation() {
        synchronized (mTasksLock) {
            List<ChromeAndroidTask> tasks = getAllTasksLocked();
            tasks.sort(
                    Comparator.comparingLong(ChromeAndroidTask::getLastActivatedTimeMillis)
                            .reversed());

            return getNativeBrowserWindowPtrsLocked(tasks);
        }
    }

    /** Activates the second to last activated task, if there are at least two tasks. */
    void activatePenultimatelyActivatedTask() {
        synchronized (mTasksLock) {
            List<ChromeAndroidTask> tasks = getAllTasksLocked();
            tasks.sort(
                    Comparator.comparingLong(ChromeAndroidTask::getLastActivatedTimeMillis)
                            .reversed());

            if (tasks.size() >= 2) {
                tasks.get(1).activate();
            }
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
    void removeAllForTesting() {
        synchronized (mTasksLock) {
            mTasks.forEach((taskId, task) -> task.destroy());
            mTasks.clear();
            mPendingTasks.forEach((taskId, task) -> task.destroy());
            mPendingTasks.clear();
        }
    }

    @Nullable ChromeAndroidTask getPendingTaskForTesting(int pendingId) {
        synchronized (mTasksLock) {
            return mPendingTasks.get(pendingId);
        }
    }

    Map<Integer, ChromeAndroidTask> getPendingTasksForTesting() {
        synchronized (mTasksLock) {
            return mPendingTasks;
        }
    }

    static void pausePendingTaskActivityCreationForTesting() {
        sPausePendingTaskActivityCreationForTesting = true;
        ResettersForTesting.register(
                () -> {
                    sPausePendingTaskActivityCreationForTesting = false;
                    sPendingTasksAwaitingActivityCreationForTesting.clear();
                });
    }

    static void resumePendingTaskActivityCreationForTesting(int pendingTaskId) {
        sPausePendingTaskActivityCreationForTesting = false;
        PendingTaskInfo pendingTaskInfo =
                sPendingTasksAwaitingActivityCreationForTesting.get(pendingTaskId);
        assert pendingTaskInfo != null
                : "Unable to resume Activity creation for pending task with ID: " + pendingTaskId;

        launchNewWindowIntent(
                pendingTaskInfo.mIntent, pendingTaskInfo.mCreateParams.getInitialBounds());
    }

    @GuardedBy("mTasksLock")
    private void removeInternalLocked(int taskId) {
        var taskRemoved = mTasks.remove(taskId);
        if (taskRemoved != null) {
            mObservers.forEach((observer) -> observer.onTaskRemoved(taskRemoved));
            taskRemoved.destroy();
        }
    }

    private static int getTaskId(ActivityWindowAndroid activityWindowAndroid) {
        Activity activity = activityWindowAndroid.getActivity().get();
        assert activity != null : "ActivityWindowAndroid should have an Activity.";

        return activity.getTaskId();
    }

    /**
     * Returns an array of the native {@code BrowserWindowInterface} addresses.
     *
     * <p>This method requires {@link #mTasksLock} as the parameter can be a view of {@link
     * #mTasks}.
     */
    @GuardedBy("mTasksLock")
    private long[] getNativeBrowserWindowPtrsLocked(
            Collection<ChromeAndroidTask> chromeAndroidTasks) {
        long[] nativeBrowserWindowPtrs = new long[chromeAndroidTasks.size()];

        int index = 0;
        for (var task : chromeAndroidTasks) {
            nativeBrowserWindowPtrs[index] = task.getOrCreateNativeBrowserWindowPtr();
            index++;
        }

        return nativeBrowserWindowPtrs;
    }

    @GuardedBy("mTasksLock")
    private @Nullable Intent createNewWindowIntentLocked(
            AndroidBrowserWindowCreateParams createParams) {
        switch (createParams.getWindowType()) {
            case BrowserWindowType.NORMAL:
                for (ChromeAndroidTask task : mTasks.values()) {
                    boolean isIncognito = createParams.getProfile().isIncognitoBranded();
                    var intent = task.createIntentForNormalBrowserWindow(isIncognito);
                    if (intent != null) {
                        return intent;
                    }
                }
                return null;
            case BrowserWindowType.POPUP:
                if (createParams.getProfile().isIncognitoBranded()) {
                    return null;
                }
                // The following code is a copy of
                // PopupCreator#initializePopupIntent().
                //
                // As of Nov 17, 2025, we couldn't use PopupCreator directly
                // as it was at the "glue" (top) layer, but this class is in
                // a modular target.
                //
                // TODO(crbug.com/461576965) Refactor away the hardcoded values.
                try {
                    var intent =
                            new Intent(
                                    ContextUtils.getApplicationContext(),
                                    Class.forName(
                                            "org.chromium.chrome.browser.customtabs.CustomTabActivity"));
                    intent.setFlags(
                            Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
                    intent.putExtra(
                            "org.chromium.chrome.browser.customtabs.EXTRA_UI_TYPE",
                            9 /* CustomTabsUiType.POPUP */);
                    // TODO: Use AndroidBrowserWindowCreateParams#getInitialBounds() create a
                    // WindowFeatures and set this extra.
                    // intent.putExtra(
                    //      "chrome.browser.app.tab_activity_glue.PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES",
                    //      new WindowFeatures().toBundle()
                    // );
                    IntentUtils.addTrustedIntentExtras(intent);
                    return intent;
                } catch (ClassNotFoundException e) {
                    return null;
                }
            default:
                return null;
        }
    }

    private static void launchNewWindowIntent(Intent intent, Rect initialBounds) {
        MultiInstanceManager.onMultiInstanceModeStarted();

        var context = ContextUtils.getApplicationContext();
        if (initialBounds.isEmpty()) {
            context.startActivity(intent);
            return;
        }

        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchBounds(initialBounds);
        context.startActivity(intent, options.toBundle());
    }

    /** Returns all PENDING and ALIVE Tasks. */
    @GuardedBy("mTasksLock")
    private List<ChromeAndroidTask> getAllTasksLocked() {
        List<ChromeAndroidTask> tasks = new ArrayList<>(mTasks.values());
        tasks.addAll(mPendingTasks.values());
        return tasks;
    }
}
