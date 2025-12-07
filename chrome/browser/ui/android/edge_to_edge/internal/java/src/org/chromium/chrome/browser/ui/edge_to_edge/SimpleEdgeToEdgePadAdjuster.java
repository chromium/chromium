// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;
import android.widget.ScrollView;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

/**
 * A simple implementation of {@link EdgeToEdgePadAdjuster} which can add a padding when e2e is on
 * and then remove when it is off.
 */
@NullMarked
public class SimpleEdgeToEdgePadAdjuster implements EdgeToEdgePadAdjuster {

    private final View mViewToPad;
    private final int mDefaultBottomPadding;
    private final boolean mEnableClipToPadding;
    private final @Nullable ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private @Nullable EdgeToEdgeController mEdgeToEdgeController;
    private final Callback<EdgeToEdgeController> mControllerChangedCallback =
            this::updateEdgeToEdgeController;

    SimpleEdgeToEdgePadAdjuster(View view, boolean enableClipToPadding) {
        this(view, (EdgeToEdgeController) null, enableClipToPadding);
    }

    /**
     * @param view The view that needs padding at the bottom.
     * @param edgeToEdgeController The supplier to the edge to edge controller.
     * @param enableClipToPadding Whether enable #setClipToPadding for compatible views (e.g
     *     ScrollView).
     */
    public SimpleEdgeToEdgePadAdjuster(
            View view,
            @Nullable EdgeToEdgeController edgeToEdgeController,
            boolean enableClipToPadding) {
        mViewToPad = view;
        mEnableClipToPadding = enableClipToPadding;
        mDefaultBottomPadding = mViewToPad.getPaddingBottom();
        mEdgeToEdgeControllerSupplier = null;
        updateEdgeToEdgeController(edgeToEdgeController);
    }

    /**
     * @param view The view that needs padding at the bottom.
     * @param edgeToEdgeControllerSupplier The supplier to the edge to edge controller.
     * @param enableClipToPadding Whether enable #setClipToPadding for compatible views (e.g
     *     ScrollView).
     */
    public SimpleEdgeToEdgePadAdjuster(
            View view,
            @Nullable ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            boolean enableClipToPadding) {
        mViewToPad = view;
        mEnableClipToPadding = enableClipToPadding;
        mDefaultBottomPadding = mViewToPad.getPaddingBottom();
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;

        if (mEdgeToEdgeControllerSupplier != null) {
            mEdgeToEdgeControllerSupplier.addObserver(mControllerChangedCallback);
        }
    }

    @Override
    public void destroy() {
        // Reset the bottom insets for the view.
        overrideBottomInset(0);

        updateEdgeToEdgeController(null);
        if (mEdgeToEdgeControllerSupplier != null) {
            mEdgeToEdgeControllerSupplier.removeObserver(mControllerChangedCallback);
        }
    }

    @Override
    public void overrideBottomInset(int inset) {
        if (mEnableClipToPadding) {
            maybeSetViewClipToPadding(inset == 0);
        }
        mViewToPad.setPadding(
                mViewToPad.getPaddingLeft(),
                mViewToPad.getPaddingTop(),
                mViewToPad.getPaddingRight(),
                mDefaultBottomPadding + inset);
    }

    // Set the view clip to padding if the view is supported.
    private void maybeSetViewClipToPadding(boolean clipToPadding) {
        if (!(mViewToPad instanceof ViewGroup)) return;

        if (mViewToPad instanceof ScrollView
                || mViewToPad instanceof RecyclerView
                || mViewToPad instanceof ListView) {
            ((ViewGroup) mViewToPad).setClipToPadding(clipToPadding);
        }
    }

    private void updateEdgeToEdgeController(@Nullable EdgeToEdgeController newController) {
        if (mEdgeToEdgeController != null) {
            mEdgeToEdgeController.unregisterAdjuster(this);
        }
        mEdgeToEdgeController = newController;
        if (mEdgeToEdgeController != null) {
            mEdgeToEdgeController.registerAdjuster(this);
        }
    }
}
