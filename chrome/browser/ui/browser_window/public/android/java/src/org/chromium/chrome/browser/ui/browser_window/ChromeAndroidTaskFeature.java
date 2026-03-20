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
     * <p>This is also the moment when the feature can associate itself with the matching native
     * {@code BrowserWindowInterface} (see documentation for the {@code nativeBrowserWindowPtr}
     * parameter below).
     *
     * <p>TODO(crbug.com/493930386): Make this method non-default. This needs a default and empty
     * implementation because downstream test code implements ChromeAndroidTaskFeature.
     *
     * @param nativeBrowserWindowPtr The native {@code BrowserWindowInterface} matching this
     *     feature's {@link ChromeAndroidTaskFeatureKey}. The value 0 will be provided if there is
     *     no matching {@code BrowserWindowInterface}. For a {@code BrowserWindowInterface} to match
     *     the {@link ChromeAndroidTaskFeatureKey}, the {@code BrowserWindowInterface} must be
     *     associated with the same {@code Profile} and {@code ActivityWindowAndroid} in the {@link
     *     ChromeAndroidTaskFeatureKey}.
     * @see ChromeAndroidTask#addFeature
     */
    default void onAddedToTask(long nativeBrowserWindowPtr) {}

    /**
     * This is an outdated version of {@link #onAddedToTask(long)}. It won't be invoked in any case.
     *
     * <p>We only keep this because downstream test code implements it.
     *
     * <p>TODO(crbug.com/493930386): Delete this.
     */
    @Deprecated
    default void onAddedToTask() {}

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
    default void onTaskBoundsChanged(Rect newBoundsInDp) {}

    /**
     * Called by a {@link ChromeAndroidTask} when the Task (window) has gained or lost focus.
     *
     * @param hasFocus True if the Task has focus.
     */
    default void onTaskFocusChanged(boolean hasFocus) {}

    /**
     * Called when the selected {@link TabModel} changes. This is also invoked when the feature is
     * added to a {@link ChromeAndroidTask}.
     *
     * @param tabModel The selected {@link TabModel}.
     */
    default void onTabModelSelected(TabModel tabModel) {}
}
