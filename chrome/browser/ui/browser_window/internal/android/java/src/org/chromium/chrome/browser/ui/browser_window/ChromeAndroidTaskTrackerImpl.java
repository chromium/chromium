// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Context;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.ArrayMap;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.PopupCreatorFactory;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.PendingTaskInfo;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
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
    private final Map<Integer, ChromeAndroidTask> mTasks = new ArrayMap<>();

    /**
     * Maps pending {@link ChromeAndroidTask} IDs to their instances. This reflects the {@link
     * ChromeAndroidTask}'s ID when it is pending, and is different from its ID in the alive state.
     */
    private final Map<Integer, ChromeAndroidTask> mPendingTasks = new ArrayMap<>();

    /** List of observers currently observing this instance. */
    private final ObserverList<ChromeAndroidTaskTrackerObserver> mObservers = new ObserverList<>();

    static ChromeAndroidTaskTrackerImpl getInstance() {
        ThreadUtils.assertOnUiThread();

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
        ThreadUtils.assertOnUiThread();
        int taskId = getTaskId(activityScopedObjects.mActivityWindowAndroid);

        var existingTask = mTasks.get(taskId);
        if (existingTask != null) {
            existingTask.addActivityScopedObjects(activityScopedObjects);
            return existingTask;
        }

        if (pendingId != null) {
            ChromeAndroidTask pendingTask = mPendingTasks.remove(pendingId);
            assert pendingTask != null : "Invalid pendingId provided.";
            pendingTask.addActivityScopedObjects(activityScopedObjects);
            mTasks.put(taskId, pendingTask);
            for (var observer : mObservers) {
                observer.onTaskAdded(pendingTask);
            }
            return pendingTask;
        }

        var newTask = new ChromeAndroidTaskImpl(activityScopedObjects);
        mTasks.put(taskId, newTask);
        for (var observer : mObservers) {
            observer.onTaskAdded(newTask);
        }
        return newTask;
    }

    @Override
    @Nullable
    public ChromeAndroidTask createPendingTask(
            AndroidBrowserWindowCreateParams createParams,
            @Nullable JniOnceCallback<Long> callback) {
        ThreadUtils.assertOnUiThread();

        Activity sourceActivity = findSourceActivityForNewWindow();
        // Only the "NORMAL" browser window requires a source Activity. See
        // MultiInstanceOrchestrator for the reason.
        if (createParams.getWindowType() == BrowserWindowType.NORMAL && sourceActivity == null) {
            if (callback != null) {
                callback.onResult(0L);
            }
            return null;
        }

        int pendingId = IdSequencer.next();
        var pendingTaskInfo = new PendingTaskInfo(pendingId, createParams, callback);
        var pendingTask = new ChromeAndroidTaskImpl(pendingTaskInfo);
        mPendingTasks.put(pendingId, pendingTask);

        // Launch the required Activity based on |createParams|.
        if (!sPausePendingTaskActivityCreationForTesting) {
            if (!createBrowserWindow(pendingId, createParams, sourceActivity)) {
                mPendingTasks.remove(pendingId);
                if (callback != null) {
                    callback.onResult(0L);
                }
                return null;
            }
        } else {
            sPendingTasksAwaitingActivityCreationForTesting.put(pendingId, pendingTaskInfo);
        }
        return pendingTask;
    }

    @Override
    @Nullable
    public ChromeAndroidTask get(int taskId) {
        ThreadUtils.assertOnUiThread();
        return mTasks.get(taskId);
    }

    @Override
    public void remove(int taskId) {
        ThreadUtils.assertOnUiThread();
        removeInternal(taskId);
    }

    @Override
    public void onActivityWindowAndroidDestroy(ActivityWindowAndroid activityWindowAndroid) {
        ThreadUtils.assertOnUiThread();
        int taskId = getTaskId(activityWindowAndroid);
        var task = mTasks.get(taskId);

        if (task == null) {
            return;
        }

        task.removeActivityScopedObjects(activityWindowAndroid);

        // Destroy the ChromeAndroidTask if there is no ActivityWindowAndroid associated with it.
        //
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
        if (task.getTopActivityWindowAndroid() == null) {
            removeInternal(taskId);
        }
    }

    @Override
    public void addObserver(ChromeAndroidTaskTrackerObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public boolean removeObserver(ChromeAndroidTaskTrackerObserver observer) {
        return mObservers.removeObserver(observer);
    }

    /** Returns an array of the native {@code BrowserWindowInterface} addresses. */
    long[] getAllNativeBrowserWindowPtrs() {
        ThreadUtils.assertOnUiThread();
        return getNativeBrowserWindowPtrs(getAllTasks());
    }

    /**
     * Returns an array of the native {@code BrowserWindowInterface} addresses, sorted by the
     * descending order of {@link ChromeAndroidTask#getLastActivatedTimeMillis()}.
     */
    long[] getNativeBrowserWindowPtrsOrderedByActivation() {
        ThreadUtils.assertOnUiThread();
        List<ChromeAndroidTask> tasks = getAllTasks();
        tasks.sort(
                Comparator.comparingLong(ChromeAndroidTask::getLastActivatedTimeMillis).reversed());

        return getNativeBrowserWindowPtrs(tasks);
    }

    /** Activates the second to last activated task, if there are at least two tasks. */
    void activatePenultimatelyActivatedTask() {
        ThreadUtils.assertOnUiThread();
        List<ChromeAndroidTask> tasks = getAllTasks();
        tasks.sort(
                Comparator.comparingLong(ChromeAndroidTask::getLastActivatedTimeMillis).reversed());

        if (tasks.size() >= 2) {
            tasks.get(1).activate();
        }
    }

    /** Count of tasks. */
    int countOfTasks() {
        return getAllTasks().size();
    }

    /** Returns all ALIVE Tasks. */
    /*package*/ List<ChromeAndroidTask> getAllTasks() {
        return new ArrayList<>(mTasks.values());
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
        List<ChromeAndroidTask> tasks = getAllTasks();
        mTasks.clear();
        for (var task : tasks) {
            task.destroy();
        }
        List<ChromeAndroidTask> pendingTasks = new ArrayList<>(mPendingTasks.values());
        mPendingTasks.clear();
        for (var task : pendingTasks) {
            task.destroy();
        }
    }

    boolean hasObserverForTesting(ChromeAndroidTaskTrackerObserver observer) {
        return mObservers.hasObserver(observer);
    }

    @Nullable ChromeAndroidTask getPendingTaskForTesting(int pendingId) {
        ThreadUtils.assertOnUiThread();
        return mPendingTasks.get(pendingId);
    }

    void pausePendingTaskActivityCreationForTesting() {
        sPausePendingTaskActivityCreationForTesting = true;
        ResettersForTesting.register(
                () -> {
                    sPausePendingTaskActivityCreationForTesting = false;
                    sPendingTasksAwaitingActivityCreationForTesting.clear();
                });
    }

    void resumePendingTaskActivityCreationForTesting(int pendingTaskId) {
        sPausePendingTaskActivityCreationForTesting = false;
        PendingTaskInfo pendingTaskInfo =
                sPendingTasksAwaitingActivityCreationForTesting.get(pendingTaskId);
        assert pendingTaskInfo != null
                : "Unable to resume Activity creation for pending task with ID: " + pendingTaskId;

        createBrowserWindow(
                pendingTaskId, pendingTaskInfo.mCreateParams, findSourceActivityForNewWindow());
    }

    private void removeInternal(int taskId) {
        var taskRemoved = mTasks.remove(taskId);
        if (taskRemoved != null) {
            for (var observer : mObservers) {
                observer.onTaskRemoved(taskRemoved);
            }
            taskRemoved.destroy();
        }
    }

    private static int getTaskId(ActivityWindowAndroid activityWindowAndroid) {
        Activity activity = activityWindowAndroid.getActivity().get();
        assert activity != null : "ActivityWindowAndroid should have an Activity.";

        return ApplicationStatus.getTaskId(activity);
    }

    /** Returns an array of the native {@code BrowserWindowInterface} addresses. */
    private long[] getNativeBrowserWindowPtrs(Collection<ChromeAndroidTask> chromeAndroidTasks) {
        List<Long> ptrs = new ArrayList<>();
        for (var task : chromeAndroidTasks) {
            ptrs.addAll(task.getAllNativeBrowserWindowPtrs());
        }

        long[] nativeBrowserWindowPtrs = new long[ptrs.size()];
        for (int i = 0; i < ptrs.size(); i++) {
            nativeBrowserWindowPtrs[i] = ptrs.get(i);
        }
        return nativeBrowserWindowPtrs;
    }

    @Nullable
    private Activity findSourceActivityForNewWindow() {
        // TODO(crbug.com/494034453) Don't just find the first activity.
        for (ChromeAndroidTask task : mTasks.values()) {
            var windowAndroid = task.getTopActivityWindowAndroid();
            if (windowAndroid == null) {
                continue;
            }

            Activity activity = windowAndroid.getActivity().get();
            if (activity != null) {
                return activity;
            }
        }
        return null;
    }

    /**
     * Creates a new browser window.
     *
     * @param pendingId The ID of the pending task.
     * @param createParams The parameters for creating the window.
     * @param sourceActivity The activity initiating the creation, if any.
     * @return False if the window creation failed immediately, true otherwise.
     */
    private static boolean createBrowserWindow(
            int pendingId,
            AndroidBrowserWindowCreateParams createParams,
            @Nullable Activity sourceActivity) {
        @BrowserWindowType int browserWindowType = createParams.getWindowType();
        switch (browserWindowType) {
            case BrowserWindowType.NORMAL:
                assumeNonNull(sourceActivity);
                return createNormalBrowserWindow(pendingId, createParams, sourceActivity);
            case BrowserWindowType.POPUP:
                return createPopupBrowserWindow(pendingId, createParams);
            default:
                throw new UnsupportedOperationException(
                        String.format(Locale.US, "Unsupported window type: %d", browserWindowType));
        }
    }

    /**
     * Creates a normal browser window.
     *
     * @param pendingId The ID of the pending task.
     * @param createParams The parameters for creating the window.
     * @param sourceActivity The activity initiating the creation.
     * @return False if the window creation failed immediately, true otherwise.
     */
    private static boolean createNormalBrowserWindow(
            int pendingId, AndroidBrowserWindowCreateParams createParams, Activity sourceActivity) {
        Bundle extrasBundle = new Bundle();
        extrasBundle.putInt(EXTRA_PENDING_BROWSER_WINDOW_TASK_ID, pendingId);
        ActivityOptions options =
                getStartActivityOptions(sourceActivity, createParams.getInitialBoundsInDp());

        if (createParams.getWebContents() != null) {
            return MultiInstanceOrchestratorFactory.getInstance()
                    .createNewWindowFromWebContents(
                            sourceActivity,
                            createParams.getProfile(),
                            createParams.getWebContents(),
                            extrasBundle,
                            options != null ? options.toBundle() : null,
                            NewWindowAppSource.BROWSER_WINDOW_CREATOR);
        }
        return MultiInstanceOrchestratorFactory.getInstance()
                .createNewWindow(
                        sourceActivity,
                        createParams.getProfile().isIncognitoBranded(),
                        extrasBundle,
                        options != null ? options.toBundle() : null,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);
    }

    /**
     * Creates a popup browser window.
     *
     * @param pendingId The ID of the pending task.
     * @param createParams The parameters for creating the window.
     * @return False if the window creation failed immediately, true otherwise.
     */
    private static boolean createPopupBrowserWindow(
            int pendingId, AndroidBrowserWindowCreateParams createParams) {
        var context = ContextUtils.getApplicationContext();
        Rect bounds = createParams.getInitialBoundsInDp();
        WindowFeatures features =
                new WindowFeatures(bounds.left, bounds.top, bounds.width(), bounds.height());

        Bundle extrasBundle = new Bundle();
        extrasBundle.putInt(EXTRA_PENDING_BROWSER_WINDOW_TASK_ID, pendingId);

        ActivityOptions options = getStartActivityOptions(context, bounds);

        if (createParams.getWebContents() != null) {
            return PopupCreatorFactory.getInstance()
                    .createNewPopupFromWebContents(
                            context,
                            createParams.getProfile(),
                            createParams.getWebContents(),
                            features,
                            extrasBundle,
                            options != null ? options.toBundle() : null);
        }
        return PopupCreatorFactory.getInstance()
                .createNewPopup(
                        context,
                        createParams.getProfile().isIncognitoBranded(),
                        features,
                        extrasBundle,
                        options != null ? options.toBundle() : null);
    }

    private static @Nullable ActivityOptions getStartActivityOptions(
            Context context, Rect initialBoundsInDp) {
        if (initialBoundsInDp.isEmpty()) return null;

        ActivityOptions options = ActivityOptions.makeBasic();
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(context);
        options.setLaunchDisplayId(display.getDisplayId());
        options.setLaunchBounds(
                new Rect(
                        DisplayUtil.dpToPx(display, initialBoundsInDp.left),
                        DisplayUtil.dpToPx(display, initialBoundsInDp.top),
                        DisplayUtil.dpToPx(display, initialBoundsInDp.right),
                        DisplayUtil.dpToPx(display, initialBoundsInDp.bottom)));
        return options;
    }
}
