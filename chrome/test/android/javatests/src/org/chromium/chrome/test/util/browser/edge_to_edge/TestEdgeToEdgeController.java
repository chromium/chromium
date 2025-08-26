// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.edge_to_edge;

import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSupplier.ChangeObserver;

/** Implementation of {@link EdgeToEdgeController} for testing. */
public class TestEdgeToEdgeController implements EdgeToEdgeController {
    public int bottomInset;

    @Override
    public int getBottomInset() {
        return bottomInset;
    }

    @Override
    public int getBottomInsetPx() {
        return bottomInset;
    }

    @Override
    public int getSystemBottomInsetPx() {
        return bottomInset;
    }

    @Override
    public boolean isPageOptedIntoEdgeToEdge() {
        return bottomInset != 0;
    }

    @Override
    public boolean isDrawingToEdge() {
        return bottomInset != 0;
    }

    @Override
    public void destroy() {}

    @Override
    public void registerAdjuster(EdgeToEdgePadAdjuster adjuster) {}

    @Override
    public void unregisterAdjuster(EdgeToEdgePadAdjuster adjuster) {}

    @Override
    public void registerObserver(ChangeObserver changeObserver) {}

    @Override
    public void unregisterObserver(ChangeObserver changeObserver) {}
}
