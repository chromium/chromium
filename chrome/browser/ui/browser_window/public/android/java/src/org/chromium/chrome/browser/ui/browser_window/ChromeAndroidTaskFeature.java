// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.graphics.Rect;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabmodel.TabModel;

/**
 * Represents a Chrome feature whose lifecycle is bound by a {@link ChromeAndroidTask}.
 *
 * @see ChromeAndroidTaskFeatureKey for how the lifecycle of a feature is determined.
 */
@NullMarked
public interface ChromeAndroidTaskFeature {

    /**
     * Called by a {@link ChromeAndroidTask} when this feature is added to it.
     *
     * <p>This is the start of the feature's lifecycle. Usually a feature would initialize objects
     * it owns in this method.
     *
     * @see ChromeAndroidTask#addFeature
     */
    void onAddedToTask();

    /**
     * Called by a {@link ChromeAndroidTask} when the feature is being removed.
     *
     * <p>This is the end of the feature's lifecycle. A feature should destroy all objects it owns
     * in this method.
     *
     * <p>The end of a feature's lifecycle is determined by its {@link ChromeAndroidTaskFeatureKey}
     * (e.g., on Task destruction, Profile destruction, or Activity destruction).
     *
     * @see ChromeAndroidTask#destroy()
     * @see ChromeAndroidTaskTracker#remove(int)
     * @see ChromeAndroidTask#removeAllFeaturesForActivity
     */
    void onFeatureRemoved();

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

    /**
     * Called when the selected {@link TabModel} changes. This is also invoked when the feature is
     * added to a {@link ChromeAndroidTask}.
     *
     * @param tabModel The selected {@link TabModel}.
     */
    default void onTabModelSelected(TabModel tabModel) {}
}
