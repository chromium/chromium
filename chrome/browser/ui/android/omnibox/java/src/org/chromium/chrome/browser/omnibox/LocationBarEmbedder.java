// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

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
}
