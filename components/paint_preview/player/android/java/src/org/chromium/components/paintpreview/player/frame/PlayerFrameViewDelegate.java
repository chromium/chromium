// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

/** Used by {@link PlayerFrameView} to delegate view events to {@link PlayerFrameMediator}. */
interface PlayerFrameViewDelegate {
    /** Called on layout with the attributed width and height. */
    void setLayoutDimensions(int width, int height);

    /**
     * Called when a single tap gesture is performed.
     * @param x X coordinate of the point clicked.
     * @param y Y coordinate of the point clicked.
     */
    void onTap(int x, int y, boolean isAbsolute);

    /**
     * Called when a long press gesture is performed.
     * @param x X coordinate of the point clicked.
     * @param y Y coordinate of the point clicked.
     */
    void onLongPress(int x, int y);
}
