// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabstrip;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;

/**
 * Instance that holds the tab strip scene layer. TabStripTopControlLayer will direct the browser
 * control movement, and tab strip transition information to the instance.
 */
@NullMarked
public interface TabStripSceneLayerHolder extends TabStripTransitionDelegate {

    /**
     * Update the offset tags info to control scene layer movement when the browser control offsets
     * are driven by the render.
     *
     * @param offsetTagsInfo The {@link BrowserControlsOffsetTagsInfo} instance.
     */
    default void updateOffsetTagsInfo(@Nullable BrowserControlsOffsetTagsInfo offsetTagsInfo) {}

    /**
     * Update the layer's exact yOffset in the top controls.
     *
     * @param yOffsetPx The layer's yOffset in Px.
     * @param visibleHeightPx The tab strip's visible portion in Px.
     */
    default void onLayerYOffsetChanged(int yOffsetPx, int visibleHeightPx) {}
}
