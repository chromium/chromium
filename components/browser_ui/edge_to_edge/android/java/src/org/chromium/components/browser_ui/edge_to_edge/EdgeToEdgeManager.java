// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.ui.util.TokenHolder;

public class EdgeToEdgeManager {
    private EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;
    private int mEdgeToEdgeToken = TokenHolder.INVALID_TOKEN;
    private final @NonNull EdgeToEdgeSystemBarColorHelper mEdgeToEdgeSystemBarColorHelper;

    /**
     * Creates an EdgeToEdgeManager for managing central edge-to-edge functionality.
     *
     * @param activity The {@link Activity} hosting the current window.
     * @param edgeToEdgeStateProvider The {@link EdgeToEdgeStateProvider} for drawing edge-to-edge.
     * @param systemBarColorHelperSupplier Supplies the {@link SystemBarColorHelper} that should be
     *     used to color the system bars when edge to edge is enabled.
     * @param shouldDrawEdgeToEdge Whether the host activity intends to draw edge-to-edge by
     *     default.
     */
    public EdgeToEdgeManager(
            @NonNull Activity activity,
            @NonNull EdgeToEdgeStateProvider edgeToEdgeStateProvider,
            @NonNull OneshotSupplier<SystemBarColorHelper> systemBarColorHelperSupplier,
            boolean shouldDrawEdgeToEdge) {
        mEdgeToEdgeStateProvider = edgeToEdgeStateProvider;
        mEdgeToEdgeSystemBarColorHelper =
                new EdgeToEdgeSystemBarColorHelper(
                        activity.getWindow(),
                        mEdgeToEdgeStateProvider,
                        systemBarColorHelperSupplier);

        if (shouldDrawEdgeToEdge) {
            mEdgeToEdgeToken = mEdgeToEdgeStateProvider.acquireSetDecorFitsSystemWindowToken();
        }
    }

    /** Destroys this instance and removes its dependencies. */
    public void destroy() {
        if (mEdgeToEdgeStateProvider != null) {
            mEdgeToEdgeStateProvider.releaseSetDecorFitsSystemWindowToken(mEdgeToEdgeToken);
            mEdgeToEdgeToken = TokenHolder.INVALID_TOKEN;
            mEdgeToEdgeStateProvider = null;
        }
        mEdgeToEdgeSystemBarColorHelper.destroy();
    }

    /**
     * Returns the {@link EdgeToEdgeStateProvider} for checking and requesting changes to the
     * edge-to-edge state.
     */
    public EdgeToEdgeStateProvider getEdgeToEdgeStateProvider() {
        return mEdgeToEdgeStateProvider;
    }

    /**
     * Returns the {@link EdgeToEdgeSystemBarColorHelper} for setting the color of the system bars.
     */
    public EdgeToEdgeSystemBarColorHelper getEdgeToEdgeSystemBarColorHelper() {
        return mEdgeToEdgeSystemBarColorHelper;
    }
}
