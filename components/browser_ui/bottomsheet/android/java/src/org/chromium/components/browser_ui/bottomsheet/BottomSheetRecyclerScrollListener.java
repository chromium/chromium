// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState.HALF;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;

/**
 * Listener for scroll events of the {@link RecyclerView} inside a bottom sheet to conditionally
 * suppress layout passes in the half state on non-large form factors.
 */
@NullMarked
public class BottomSheetRecyclerScrollListener extends RecyclerView.OnScrollListener {
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mSheetContent;

    private int mY;

    /**
     * @param bottomSheetController The {@link BottomSheetController} managing the bottom sheet.
     * @param sheetContent The {@link BottomSheetContent} hosting the {@link RecyclerView}.
     */
    public BottomSheetRecyclerScrollListener(
            BottomSheetController bottomSheetController, BottomSheetContent sheetContent) {
        mBottomSheetController = bottomSheetController;
        mSheetContent = sheetContent;
    }

    @Override
    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
        super.onScrolled(recyclerView, dx, dy);
        mY = recyclerView.computeVerticalScrollOffset();
        boolean isLargeFormFactor =
                mBottomSheetController.isLargeFormFactorUiEnabled(mSheetContent);
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
