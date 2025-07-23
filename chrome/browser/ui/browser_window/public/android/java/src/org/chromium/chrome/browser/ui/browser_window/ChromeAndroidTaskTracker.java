// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
     * Returns a {@link ChromeAndroidTask} with the same Task ID as that of the given {@link
     * ActivityWindowAndroid}'s {@code Activity}.
     *
     * <p>This method is usually called when a {@code ChromeActivity} is created, which is also when
     * we start caring about Tasks (windows).
     *
     * <p>As a {@link ChromeAndroidTask} is meant to track an Android Task, but an {@link
     * ActivityWindowAndroid} is associated with a {@code ChromeActivity}, it's possible that when
     * this method is called, a {@link ChromeAndroidTask} already exists, in which case the existing
     * {@link ChromeAndroidTask} will be returned.
     *
     * <p>Otherwise, this method will create a new {@link ChromeAndroidTask}, add it to the internal
     * collection, and return it.
     */
    ChromeAndroidTask obtainTask(ActivityWindowAndroid activityWindowAndroid);

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
}
