// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

/** A supplier notifying of whether edge to edge is on and the value of the bottom inset. */
public interface EdgeToEdgeSupplier {
    void registerAdjuster(EdgeToEdgePadAdjuster adjuster);

    void unregisterAdjuster(EdgeToEdgePadAdjuster adjuster);
}
