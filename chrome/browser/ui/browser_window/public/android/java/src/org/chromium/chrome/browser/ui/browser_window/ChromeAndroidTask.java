// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.List;

/**
 * Represents an Android window containing Chrome.
 *
 * <p>In Android, a window is a <i>Task</i>. However, depending on Task API availability in Android
 * framework, the implementation of this interface may rely on {@code ChromeActivity} as a
 * workaround to track windows.
 *
 * <p>The main difference between Task-based window tracking and {@code ChromeActivity}-based window
 * tracking is the lifecycle of a {@link ChromeAndroidTask} object, as the OS can kill {@code
 * ChromeActivity} when it's in the background, but keep its Task.
 *
 * <p>Example 1:
 *
 * <ul>
 *   <li>The user opens {@code ChromeActivity}.
 *   <li>The user then opens {@code SettingsActivity}, or an {@code Activity} not owned by Chrome.
 *   <li>If the OS decides to kill {@code ChromeActivity}, which is now in the background, {@code
 *       ChromeActivity}-based window tracking will lose the window, but the Task (window) is still
 *       alive and visible to the user.
 * </ul>
 *
 * <p>Example 2:
 *
 * <ul>
 *   <li>The user opens {@code ChromeActivity}.
 *   <li>The user moves the Task to the background.
 *   <li>If the OS decides to kill {@code ChromeActivity}, but keep its Task, {@code
 *       ChromeActivity}-based window tracking will lose the window, but the Task (window) can still
 *       be seen in "Recents" and restored by the user.
 * </ul>
 */
@NullMarked
public interface ChromeAndroidTask {

    /**
     * Returns the ID of this {@link ChromeAndroidTask}, which is the same as defined by {@link
     * android.app.TaskInfo#taskId}.
     */
    int getId();

    /**
     * Sets the current {@link ActivityWindowAndroid} in this Task.
     *
     * <p>As a {@link ChromeAndroidTask} is meant to track an Android Task, but an {@link
     * ActivityWindowAndroid} is associated with a {@code ChromeActivity}, this method is needed to
     * support the difference in their lifecycles.
     *
     * <p>We assume there is at most one {@link ActivityWindowAndroid} associated with a {@link
     * ChromeAndroidTask} at any time. If this method is called when this {@link ChromeAndroidTask}
     * already has an {@link ActivityWindowAndroid}, an {@link AssertionError} will occur.
     *
     * @see #clearActivityWindowAndroid()
     */
    void setActivityWindowAndroid(ActivityWindowAndroid activityWindowAndroid);

    /**
     * Returns the current {@link ActivityWindowAndroid} in this Task, or {@code null} if there is
     * none.
     */
    @Nullable ActivityWindowAndroid getActivityWindowAndroid();

    /**
     * Clears the current {@link ActivityWindowAndroid} in this Task.
     *
     * <p>This method should be called when the current {@link ActivityWindowAndroid} is about to be
     * destroyed.
     *
     * @see #setActivityWindowAndroid(ActivityWindowAndroid)
     */
    void clearActivityWindowAndroid();

    /**
     * Adds a {@link ChromeAndroidTaskFeature} to this {@link ChromeAndroidTask}.
     *
     * <p>This method is the start of the {@link ChromeAndroidTaskFeature}'s lifecycle, and {@link
     * ChromeAndroidTaskFeature#onAddedToTask} will be invoked.
     */
    void addFeature(ChromeAndroidTaskFeature feature);

    /**
     * Returns the address of the native {@code BrowserWindowInterface}.
     *
     * <p>If the native object hasn't been created, this method will create it before returning its
     * address.
     */
    long getOrCreateNativeBrowserWindowPtr();

    /**
     * Destroys all objects owned by this {@link ChromeAndroidTask}, including all {@link
     * ChromeAndroidTaskFeature}s.
     *
     * @see #addFeature(ChromeAndroidTaskFeature)
     */
    void destroy();

    /** Returns whether this {@link ChromeAndroidTask} has been destroyed. */
    boolean isDestroyed();

    /** Returns all {@link ChromeAndroidTaskFeature}s for testing. */
    List<ChromeAndroidTaskFeature> getAllFeaturesForTesting();
}
