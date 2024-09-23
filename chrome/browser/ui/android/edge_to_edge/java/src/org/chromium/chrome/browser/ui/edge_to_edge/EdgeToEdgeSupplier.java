// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import androidx.annotation.Px;

/** A supplier notifying of whether edge to edge is on and the value of the bottom inset. */
public interface EdgeToEdgeSupplier {

    /**
     * Notifies that a change has occurred that may require an update, and supplies the new bottom
     * inset in pixels.
     */
    interface ChangeObserver {

        /**
         * Notifies that a change has been made in the bottom inset and supplies the new inset. Note
         * that this inset may differ from the bottom inset passed to {@link EdgeToEdgePadAdjuster}s
         * (e.g. when browser controls are present but scrolled off).
         *
         * @param bottomInset The new bottom inset. This represents the height of the bottom
         *     navigation bar, regardless of whether the page is drawing edge-to-edge.
         * @param isDrawingToEdge Whether the Chrome is drawing edge-to-edge.
         * @param isPageOptInToEdge Whether the current web page is opted in edge-to-edge.
         */
        void onToEdgeChange(
                @Px int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge);
    }

    /** Registers an automatic adjuster of padding for a view. See {@link EdgeToEdgePadAdjuster}. */
    void registerAdjuster(EdgeToEdgePadAdjuster adjuster);

    void unregisterAdjuster(EdgeToEdgePadAdjuster adjuster);

    /**
     * Registers an observer to be called any time the system changes ToEdge or back.
     *
     * @param changeObserver a {@link ChangeObserver} that provides the new bottom inset.
     */
    void registerObserver(ChangeObserver changeObserver);

    void unregisterObserver(ChangeObserver changeObserver);
}
