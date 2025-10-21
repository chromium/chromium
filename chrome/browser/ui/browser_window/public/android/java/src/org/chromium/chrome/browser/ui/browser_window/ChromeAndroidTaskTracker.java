// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.base.JniOnceCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * Tracks {@link ChromeAndroidTask}s.
 *
 * <p>The implementation of this interface should be a singleton that maintains an internal
 * collection of all {@link ChromeAndroidTask}s.
 */
@NullMarked
public interface ChromeAndroidTaskTracker {

    /**
     * The Intent extra key to store the id of a pending ChromeAndroidTask that will be adopted by
     * the Activity launched from the Intent.
     */
    String EXTRA_PENDING_BROWSER_WINDOW_TASK_ID =
            "org.chromium.chrome.browser.ui.browser_window.pending_task_id";

    /**
     * Returns a {@link ChromeAndroidTask} with the same Task ID as that of the given {@link
     * ActivityWindowAndroid}'s {@code Activity}.
     *
     * <p>This method is usually called when a {@code ChromeActivity} is created, which is also when
     * we start caring about Tasks (windows).
     *
     * <p>As a {@link ChromeAndroidTask} is meant to track an Android Task, but an {@link
     * ActivityWindowAndroid} is associated with a {@code ChromeActivity}, it's possible that when
     * this method is called, a {@link ChromeAndroidTask} already exists, in which case the {@code
     * browserWindowType} must be the same as that of the existing {@link ChromeAndroidTask}, and
     * the existing {@link ChromeAndroidTask} will be returned.
     *
     * <p>Otherwise, this method will create a new {@link ChromeAndroidTask}, add it to the internal
     * collection, and return it.
     *
     * @param browserWindowType The browser window type of the returned {@link ChromeAndroidTask}.
     *     The types are defined in the native {@code BrowserWindowInterface::Type} enum.
     * @param activityWindowAndroid The {@link ActivityWindowAndroid} to be associated with the
     *     returned {@link ChromeAndroidTask}.
     * @param tabModel The tab model associated with the returned {@link ChromeAndroidTask}.
     * @param pendingId The unique ID of the pending {@link ChromeAndroidTask} that the newly
     *     created {@code ChromeActivity} should adopt. May be {@code null} when the activity is not
     *     associated with a pending {@link ChromeAndroidTask}.
     */
    ChromeAndroidTask obtainTask(
            @BrowserWindowType int browserWindowType,
            ActivityWindowAndroid activityWindowAndroid,
            TabModel tabModel,
            @Nullable Integer pendingId);

    /**
     * Creates a pending {@link ChromeAndroidTask} that is not yet associated with an {@code
     * Activity}.
     *
     * @param createParams The {@link AndroidBrowserWindowCreateParams} that will determine the
     *     newly created {@code Activity}'s startup state.
     * @param callback The callback to be invoked with the native {@code AndroidBrowserWindow}
     *     pointer once the task becomes active. May be {@code null} for synchronous creation.
     * @return The pending {@link ChromeAndroidTask}.
     * @see BrowserWindowCreatorBridge
     */
    ChromeAndroidTask createPendingTask(
            AndroidBrowserWindowCreateParams createParams,
            @Nullable JniOnceCallback<Long> callback);

    /**
     * Returns the {@link ChromeAndroidTask} with the given {@code taskId}.
     *
     * @param taskId Same as defined by {@link android.app.TaskInfo#taskId}.
     */
    @Nullable ChromeAndroidTask get(int taskId);

    /**
     * Removes from the internal collection the {@link ChromeAndroidTask} with the given {@code
     * taskId}, and destroys all objects owned by it (via {@link ChromeAndroidTask#destroy()}).
     *
     * @param taskId Same as defined by {@link android.app.TaskInfo#taskId}.
     */
    void remove(int taskId);

    /**
     * Called when an {@link ActivityWindowAndroid} is destroyed.
     *
     * @param activityWindowAndroid The {@link ActivityWindowAndroid} that is being destroyed.
     */
    void onActivityWindowAndroidDestroy(ActivityWindowAndroid activityWindowAndroid);

    /**
     * Adds an observer which will be called on addition or removal of tasks.
     *
     * @param observer The observer to add.
     */
    void addObserver(ChromeAndroidTaskTrackerObserver observer);

    /**
     * Removes an observer, if it exists.
     *
     * @param observer The observer to remove.
     */
    boolean removeObserver(ChromeAndroidTaskTrackerObserver observer);
}
