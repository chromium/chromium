// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.transition.Transition;
import android.transition.TransitionManager;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IdRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.AsyncViewStub;

/**
 * Interface defining capabilities provided by the view embedding the LocationBar. This allows the
 * LocationBar to request changes to the embedding view or access views within the embedding
 * hierarchy without having direct knowledge of it.
 *
 * <p>setRequestFixedHeight is an instructive example of requesting changes; instead of attempting
 * to change the parent's layout params directly (fraught, likely to have competing changes), we use
 * the embedder interface and an ancestor view implements the sizing logic cleanly.
 *
 * <p>Providing access to the autocomplete result view stub is an example of accessing views; the
 * embedder knows where the suggestions container should be placed in the hierarchy and provides it
 * to the LocationBar components.
 */
@NullMarked
public interface LocationBarEmbedder {
    /** Returns the {@link AsyncViewStub} for the suggestions container, if available. */
    default @Nullable AsyncViewStub getSuggestionsContainerStub() {
        return null;
    }

    /** Returns the ID to use for the inflated suggestions container view, if available. */
    default @IdRes int getSuggestionsContainerInflatedViewId() {
        return View.NO_ID;
    }

    /**
     * Request that the embedding view remain fixed at its current height or stop fixing its height.
     */
    default void setRequestFixedHeight(boolean requestFixedHeight) {}

    /**
     * Called when the visibility of a width consumer need to change. The embedder should handle the
     * visibility changes.
     */
    default void onWidthConsumerVisibilityChanged() {}

    /**
     * Begin a delayed transition for the embedded view. Allows the embedder to kick off with any of
     * its own relevant transitions.
     *
     * @param sceneRoot The default scene root to use if the embedder does not need a different one.
     * @param transition The delayed transition that the location bar is starting.
     */
    default void beginEmbeddedDelayedTransition(ViewGroup sceneRoot, Transition transition) {
        TransitionManager.beginDelayedTransition(sceneRoot, transition);
    }

    /** Returns the back press handler type for the embedder. */
    default @BackPressHandler.Type int getBackPressHandlerType() {
        return BackPressHandler.Type.LOCATION_BAR;
    }
}
