// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

/** Triggered when the edge-to-edge state is updated. */
public interface EdgeToEdgePadAdjuster {

    /**
     * @param toEdge Whether edge-to-edge is on.
     * @param inset The bottom inset which is supposed to be padded to or removed from the bottom
     *     view.
     */
    void adjustToEdge(boolean toEdge, int inset);
}
