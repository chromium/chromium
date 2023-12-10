// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

/** Interface for handling overscroll events in the player. */
public interface OverscrollHandler {
    /** Used to start an overscroll event. Returns true if it is able to be created/consumed. */
    boolean start();

    /**
     * Updates the overscroll amount.
     *
     * @param yDelta The change in overscroll amount. Positive values indicate more overscrolling.
     */
    void pull(float yDelta);

    /**
     * Releases the overscroll event. This will trigger a refresh if a sufficient number and
     * distance of {@link #pull} calls occurred.
     */
    void release();

    /** Resets the overscroll event if it was aborted. */
    void reset();
}
