// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState.HALF;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;

@NullMarked
public class BottomSheetRecyclerScrollListener extends RecyclerView.OnScrollListener {
    private final BottomSheetController mBottomSheetController;

    private int mY;

    public BottomSheetRecyclerScrollListener(BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
    }

    @Override
    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
        super.onScrolled(recyclerView, dx, dy);
        mY = recyclerView.computeVerticalScrollOffset();
        boolean isLargeFormFactor =
                mBottomSheetController.isLargeFormFactorUiEnabled(
                        mBottomSheetController.getCurrentSheetContent());
        // On desktop, avoid layout suppression in HALF state to allow smooth content
        // resizing and scrolling.
        if (isScrolledToTop()
                && mBottomSheetController.getSheetState() == HALF
                && !isLargeFormFactor) {
            recyclerView.suppressLayout(/* suppress= */ true);
        }
    }

    public void reset() {
        mY = 0;
    }

    public boolean isScrolledToTop() {
        return mY == 0;
    }
}
