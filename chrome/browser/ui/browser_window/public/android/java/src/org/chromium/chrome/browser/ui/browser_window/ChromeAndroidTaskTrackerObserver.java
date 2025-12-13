// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;

/**
 * Observes {@link ChromeAndroidTaskTracker}s, and gets called into upon addition or removal of a
 * task.
 */
@NullMarked
public interface ChromeAndroidTaskTrackerObserver {
    /**
     * Called when a task is added.
     *
     * @param task The added task.
     */
    void onTaskAdded(ChromeAndroidTask task);

    /**
     * Called when a task is removed.
     *
     * @param task The removed task.
     */
    void onTaskRemoved(ChromeAndroidTask task);
}
