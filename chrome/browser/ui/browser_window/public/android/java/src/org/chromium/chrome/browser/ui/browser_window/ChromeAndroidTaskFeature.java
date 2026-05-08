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
     * Contains data for a {@link ChromeAndroidTaskFeature} to initialize itself after it's added to
     * a {@link ChromeAndroidTask}.
     *
     * @see #onAddedToTask
     */
    final class InitInfo {
        /**
         * The native {@code BrowserWindowInterface} matching the feature's {@link
         * ChromeAndroidTaskFeatureKey}. The native pointer will be 0 if there is no matching {@code
         * BrowserWindowInterface}. For a {@code BrowserWindowInterface} to match the {@link
         * ChromeAndroidTaskFeatureKey}, the {@code BrowserWindowInterface} must be associated with
         * the same {@code Profile} and {@code ActivityWindowAndroid} in the {@link
         * ChromeAndroidTaskFeatureKey}.
         */
        public final long nativeBrowserWindowPtr;

        /** Whether the Task is visible. */
        public final boolean isVisible;

        /** The Task bounds, in px. */
        public final Rect boundsInPx;

        /**
         * The logical display ID of the display in which the Task is running. See {@code
         * DisplayAndroid#getDisplayId()}.
         */
        public final int displayId;

        public InitInfo(
                long nativeBrowserWindowPtr, boolean isVisible, Rect boundsInPx, int displayId) {
            this.nativeBrowserWindowPtr = nativeBrowserWindowPtr;
            this.isVisible = isVisible;
            this.boundsInPx = boundsInPx;
            this.displayId = displayId;
        }
    }

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
     * @param nativeBrowserWindowPtr The native {@code BrowserWindowInterface} matching this
     *     feature's {@link ChromeAndroidTaskFeatureKey}. The value 0 will be provided if there is
     *     no matching {@code BrowserWindowInterface}. For a {@code BrowserWindowInterface} to match
     *     the {@link ChromeAndroidTaskFeatureKey}, the {@code BrowserWindowInterface} must be
     *     associated with the same {@code Profile} and {@code ActivityWindowAndroid} in the {@link
     *     ChromeAndroidTaskFeatureKey}.
     * @see ChromeAndroidTask#addFeature
     */
    // TODO (crbug.com/510525529): Remove API once #onAddedToTask(InitInfo) is made non-default.
    default void onAddedToTask(long nativeBrowserWindowPtr) {}

    /**
     * Called by a {@link ChromeAndroidTask} when this feature is added to it.
     *
     * <p>This is the start of the feature's lifecycle. Usually a feature would initialize objects
     * it owns in this method.
     *
     * @param initInfo The {@link InitInfo} encapsulating the state of the Task when the feature is
     *     added to it.
     * @see ChromeAndroidTask#addFeature
     */
    // TODO (crbug.com/510525529): Make non-default after downstream is updated.
    default void onAddedToTask(InitInfo initInfo) {}

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
    // TODO (crbug.com/505171896): Remove this API after downstream is updated.
    default void onTaskBoundsChanged(Rect newBoundsInDp) {}

    /**
     * Called by a {@link ChromeAndroidTask} when the Task (window) bounds are changed.
     *
     * @param displayId The ID of the display containing the task.
     * @param newBoundsInDp The new Task bounds, in dp.
     * @param newBoundsInPx The new Task bounds, in px.
     */
    default void onTaskBoundsChanged(int displayId, Rect newBoundsInDp, Rect newBoundsInPx) {}

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
