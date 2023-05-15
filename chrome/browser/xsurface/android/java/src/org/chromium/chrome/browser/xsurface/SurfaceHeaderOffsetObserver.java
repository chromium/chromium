// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

/**
 * Implemented internally.
 *
 * Interface to listen to the offset events from the scrollable container of the current Surface.
 * Certain layouts may simulate scrolls by translating views on and offscreen. This interface tracks
 * those changes by providing a vertical offset which represents the delta from the original
 * position that is currently rendered onscreen.
 * @see org.chromium.chrome.browser.feed.ScrollListener
 */
public interface SurfaceHeaderOffsetObserver {
    /**
     * Called when the vertical offset of the header (1st item) in the scrollable container changes.
     *
     * @param verticalOffset the new vertical offset of the header. This number is negative when
     *   simulating scrolling farther down the page, as the header is shifted up into the negative
     *   y range, eventually it is completely offscreen. For example, if the view is scrolled down
     *   50px, the header offset will be -50px.
     */
    default void onHeaderOffsetChanged(int verticalOffset) {}
}
