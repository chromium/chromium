// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.graphics.Rect;

import org.chromium.build.annotations.NullMarked;

/** Represents a Chrome feature whose lifecycle should be in sync with {@link ChromeAndroidTask}. */
@NullMarked
public interface ChromeAndroidTaskFeature {

    /**
     * Called by a {@link ChromeAndroidTask} when this feature is added to it.
     *
     * <p>This is the start of the feature's lifecycle. Usually a feature would initialize objects
     * it owns in this method.
     *
     * @see ChromeAndroidTask#addFeature(ChromeAndroidTaskFeature)
     */
    void onAddedToTask();

    /**
     * Called by a {@link ChromeAndroidTask} when the Task is removed.
     *
     * <p>This is the end of the feature's lifecycle. A feature should destroy all objects it owns
     * in this method.
     *
     * @see ChromeAndroidTask#destroy()
     * @see ChromeAndroidTaskTracker#remove(int)
     */
    void onTaskRemoved();

    /**
     * Called by a {@link ChromeAndroidTask} when the Task (window) bounds are changed.
     *
     * @param newBoundsInDp The new Task bounds.
     */
    void onTaskBoundsChanged(Rect newBoundsInDp);

    /**
     * Called by a {@link ChromeAndroidTask} when the Task (window) has gained or lost focus.
     *
     * @param hasFocus True if the Task has focus.
     */
    void onTaskFocusChanged(boolean hasFocus);
}
