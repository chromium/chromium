// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Intent;
import android.graphics.Rect;
import android.util.ArrayMap;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.PopupIntentCreatorProvider;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
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
            assert existingTask.getBrowserWindowType() == browserWindowType
                    : "The browser window type of an existing task can't be changed.";
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

        var newTask = new ChromeAndroidTaskImpl(browserWindowType, activityScopedObjects);
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
        Intent newWindowIntent = createNewWindowIntent(createParams);
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
            launchNewWindowIntent(newWindowIntent, createParams.getInitialBoundsInDp());
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

    /** Returns all PENDING and ALIVE Tasks. */
    /*package*/ List<ChromeAndroidTask> getAllTasks() {
        List<ChromeAndroidTask> tasks = new ArrayList<>(mTasks.values());
        tasks.addAll(mPendingTasks.values());
        return tasks;
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
        ThreadUtils.assertOnUiThread();
        for (var task : mTasks.values()) {
            task.destroy();
        }
        mTasks.clear();
        for (var task : mPendingTasks.values()) {
            task.destroy();
        }
        mPendingTasks.clear();
    }

    boolean hasObserverForTesting(ChromeAndroidTaskTrackerObserver observer) {
        return mObservers.hasObserver(observer);
    }

    @Nullable ChromeAndroidTask getPendingTaskForTesting(int pendingId) {
        ThreadUtils.assertOnUiThread();
        return mPendingTasks.get(pendingId);
    }

    Map<Integer, ChromeAndroidTask> getPendingTasksForTesting() {
        ThreadUtils.assertOnUiThread();
        return mPendingTasks;
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
                pendingTaskInfo.mIntent, pendingTaskInfo.mCreateParams.getInitialBoundsInDp());
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

        return activity.getTaskId();
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

    private @Nullable Intent createNewWindowIntent(AndroidBrowserWindowCreateParams createParams) {
        var profile = createParams.getProfile();
        boolean isIncognito = profile.isIncognitoBranded();
        // If incognito mode is disabled, it is disallowed to create an incognito window.
        if (isIncognito && !IncognitoUtils.isIncognitoModeEnabled(profile)) {
            return null;
        }

        @BrowserWindowType int browserWindowType = createParams.getWindowType();
        switch (browserWindowType) {
            case BrowserWindowType.NORMAL:
                for (ChromeAndroidTask task : mTasks.values()) {
                    var intent = task.createIntentForNormalBrowserWindow(isIncognito);
                    if (intent != null) {
                        return intent;
                    }
                }
                return null;
            case BrowserWindowType.POPUP:
                var popupIntentCreator = assertNonNull(PopupIntentCreatorProvider.getInstance());
                Rect bounds = createParams.getInitialBoundsInDp();
                WindowFeatures features =
                        new WindowFeatures(
                                bounds.left, bounds.top, bounds.width(), bounds.height());
                Intent intent =
                        popupIntentCreator.createPopupIntent(
                                features, createParams.getProfile().isIncognitoBranded());
                IntentUtils.addTrustedIntentExtras(intent);
                return intent;
            default:
                throw new UnsupportedOperationException(
                        String.format(Locale.US, "Unsupported window type: %d", browserWindowType));
        }
    }

    private static void launchNewWindowIntent(Intent intent, Rect initialBoundsInDp) {
        var context = ContextUtils.getApplicationContext();
        if (initialBoundsInDp.isEmpty()) {
            context.startActivity(intent);
            return;
        }

        ActivityOptions options = ActivityOptions.makeBasic();
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(context);
        options.setLaunchDisplayId(display.getDisplayId());
        options.setLaunchBounds(
                new Rect(
                        DisplayUtil.dpToPx(display, initialBoundsInDp.left),
                        DisplayUtil.dpToPx(display, initialBoundsInDp.top),
                        DisplayUtil.dpToPx(display, initialBoundsInDp.right),
                        DisplayUtil.dpToPx(display, initialBoundsInDp.bottom)));
        context.startActivity(intent, options.toBundle());
    }
}
