// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.transition.Transition;
import android.transition.TransitionManager;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface defining capabilities provided by the view embedding the LocationBar. This allows the
 * the LocationBar to request changes to the embedding view without having direct knowledge of it.
 * setRequestFixedHeight is an instructive example; instead of attempting to change the parent's
 * layout params directly (fraught, likely to have competing changes), we use the embedder interface
 * and an ancestor view implements the sizing logic cleanly.
 */
@NullMarked
public interface LocationBarEmbedder {
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
}
