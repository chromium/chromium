// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import androidx.annotation.NonNull;

public class EdgeToEdgeManager {
    private EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;

    private int mEdgeToEdgeToken;

    /**
     * Creates an EdgeToEdgeManager for managing central edge-to-edge functionality.
     *
     * @param edgeToEdgeStateProvider The {@link EdgeToEdgeStateProvider} for drawing edge-to-edge.
     * @param shouldDrawEdgeToEdge Whether the host activity intends to draw edge-to-edge by
     *     default.
     */
    public EdgeToEdgeManager(
            @NonNull EdgeToEdgeStateProvider edgeToEdgeStateProvider,
            boolean shouldDrawEdgeToEdge) {
        mEdgeToEdgeStateProvider = edgeToEdgeStateProvider;
        if (shouldDrawEdgeToEdge) {
            mEdgeToEdgeToken = mEdgeToEdgeStateProvider.acquireSetDecorFitsSystemWindowToken();
        }

        // TODO (crbug.com/376727621) Initialize EdgeToEdgeSystemBarColorHelper
    }

    public void destroy() {
        if (mEdgeToEdgeStateProvider != null) {
            mEdgeToEdgeStateProvider.releaseSetDecorFitsSystemWindowToken(mEdgeToEdgeToken);
            mEdgeToEdgeStateProvider = null;
        }
    }

    /**
     * Returns the {@link EdgeToEdgeStateProvider} for checking and requesting changes to the
     * edge-to-edge state.
     */
    public EdgeToEdgeStateProvider getEdgeToEdgeStateProvider() {
        return mEdgeToEdgeStateProvider;
    }
}
