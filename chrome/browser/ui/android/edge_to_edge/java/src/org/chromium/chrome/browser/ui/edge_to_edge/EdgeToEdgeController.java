// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import org.chromium.base.lifetime.Destroyable;

/**
 * Control drawing using the Android Edge to Edge Feature. This allows drawing under Android System
 * Bars.
 */
public interface EdgeToEdgeController extends Destroyable, EdgeToEdgeSupplier {
    /**
     * @return the inset in DPs needed for the bottom UI to adjust views to draw below the Bottom
     *     Nav Bar. Returns 0 when Edge To Edge is not enabled or when the controller is drawing the
     *     page ToNormal. Note that this inset may differ from the bottom inset passed to {@link
     *     EdgeToEdgePadAdjuster}s (e.g. when browser controls are present but scrolled off).
     */
    int getBottomInset();

    /**
     * @return the inset in pixels needed for the bottom UI to adjust views to draw below the Bottom
     *     Nav Bar. Returns 0 when Edge To Edge is not enabled or when the controller is drawing the
     *     page ToNormal. Note that this inset may differ from the bottom inset passed to {@link
     *     EdgeToEdgePadAdjuster}s (e.g. when browser controls are present but scrolled off).
     */
    int getBottomInsetPx();

    /**
     * @return the inset in pixels needed for the bottom UI to adjust views to draw below the Bottom
     *     Nav Bar. This value will persist even if the controller is not drawing the page ToEdge.
     */
    // TODO(crbug.com/367426935) Fold into the getBottomInset* methods
    int getSystemBottomInsetPx();

    /**
     * Whether the system is drawing "toEdge" (i.e. the edge-to-edge wrapper has no bottom padding).
     * This could be due to the current page being opted into edge-to-edge, or a partial
     * edge-to-edge with the bottom chin present.
     */
    boolean isDrawingToEdge();

    /**
     * @return Whether the current webpage (via opt-in) or native page is drawing edge to edge to on
     *     initial page load. Note that a page may still draw beneath the OS navigation bar without
     *     this being true if the bottom chin ({@link EdgeToEdgeBottomChinCoordinator}) is enabled
     *     and has been fully scrolled off.
     */
    boolean isPageOptedIntoEdgeToEdge();
}
