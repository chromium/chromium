// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.util.TokenHolder;

@NullMarked
public class EdgeToEdgeManager {
    private final ObservableSupplierImpl<Boolean> mContentFitsWindowInsetsSupplier =
            new ObservableSupplierImpl<>();
    private @Nullable EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;
    private int mEdgeToEdgeToken = TokenHolder.INVALID_TOKEN;
    private final EdgeToEdgeSystemBarColorHelper mEdgeToEdgeSystemBarColorHelper;

    /**
     * Creates an EdgeToEdgeManager for managing central edge-to-edge functionality.
     *
     * @param activity The {@link Activity} hosting the current window.
     * @param edgeToEdgeStateProvider The {@link EdgeToEdgeStateProvider} for drawing edge-to-edge.
     * @param systemBarColorHelperSupplier Supplies the {@link SystemBarColorHelper} that should be
     *     used to color the system bars when edge to edge is enabled.
     * @param shouldDrawEdgeToEdge Whether the host activity intends to draw edge-to-edge by
     *     default.
     * @param canColorStatusBarColor Whether the status bar color is able to be changed.
     */
    public EdgeToEdgeManager(
            Activity activity,
            EdgeToEdgeStateProvider edgeToEdgeStateProvider,
            OneshotSupplier<SystemBarColorHelper> systemBarColorHelperSupplier,
            boolean shouldDrawEdgeToEdge,
            boolean canColorStatusBarColor) {
        mContentFitsWindowInsetsSupplier.set(!shouldDrawEdgeToEdge);

        mEdgeToEdgeStateProvider = edgeToEdgeStateProvider;
        mEdgeToEdgeSystemBarColorHelper =
                new EdgeToEdgeSystemBarColorHelper(
                        activity.getWindow(),
                        getContentFitsWindowInsetsSupplier(),
                        systemBarColorHelperSupplier,
                        canColorStatusBarColor);

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
        assert mEdgeToEdgeStateProvider != null; // Ensure not destroyed.
        return mEdgeToEdgeStateProvider;
    }

    /**
     * Returns the {@link EdgeToEdgeSystemBarColorHelper} for setting the color of the system bars.
     */
    public EdgeToEdgeSystemBarColorHelper getEdgeToEdgeSystemBarColorHelper() {
        return mEdgeToEdgeSystemBarColorHelper;
    }

    /**
     * Sets whether the content should fit within the system's window insets, or if the content
     * should be drawn edge-to-edge (into the window insets).
     */
    public void setContentFitsWindowInsets(boolean contentFitsWindow) {
        mContentFitsWindowInsetsSupplier.set(contentFitsWindow);
    }

    /**
     * Returns true if the content should fit within the system's window insets, false if the
     * content should be drawn edge-to-edge (into the window insets).
     */
    public boolean shouldContentFitsWindowInsets() {
        return assumeNonNull(mContentFitsWindowInsetsSupplier.get());
    }

    /**
     * Returns the supplier informing whether the contents fit within the system's window insets.
     */
    public ObservableSupplier<Boolean> getContentFitsWindowInsetsSupplier() {
        return mContentFitsWindowInsetsSupplier;
    }
}
