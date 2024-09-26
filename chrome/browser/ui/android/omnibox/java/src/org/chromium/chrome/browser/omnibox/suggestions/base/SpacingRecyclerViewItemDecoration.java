// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;

/**
 * Configures space before the first element of the recycler view, and between each of the elements.
 */
public class SpacingRecyclerViewItemDecoration extends ItemDecoration {
    private final @Px int mLeadInSpace;
    private @Px int mElementSpace;

    /**
     * @param leadInSpace the space before the first element
     * @param elementSpace the total space between each two elements, applied evenly to each side of
     *     every child
     */
    public SpacingRecyclerViewItemDecoration(@Px int leadInSpace, @Px int elementSpace) {
        mLeadInSpace = leadInSpace;
        mElementSpace = elementSpace;
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        outRect.left = outRect.right = mElementSpace / 2;

        int childCount = parent.getAdapter().getItemCount();
        int itemPosition = parent.getChildAdapterPosition(view);
        if (itemPosition != 0 && itemPosition != (childCount - 1)) return;

        // Apply lead-in padding ahead of the first and after the last element.
        if (parent.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL ^ itemPosition != 0) {
            outRect.right = mLeadInSpace;
        } else {
            outRect.left = mLeadInSpace;
        }
    }

    /**
     * Method to be invoked by the owning RecyclerView when its measured size is changed.
     *
     * <p>Permits any derived implementations to adjust their spaces and paddings. This call should
     * be run from `onMeasure()`, ahead of `layout()` pass.
     *
     * @param isPortraitOrientation whether screen orientation is portrait
     * @param newWidth new width of the RecyclerView
     * @param newHeight new height of the RecyclerView
     * @return true, if decorations have changed and item decorations should be invalidated
     */
    public boolean notifyViewSizeChanged(
            boolean isPortraitOrientation, int newWidth, int newHeight) {
        return false;
    }

    /**
     * Specify new element space to be used as an item decoration.
     *
     * <p>Triggers RecyclerView update if the new spacing is different from the old one.
     *
     * <p>This call is intended to be used by derived classes. Keep this call protected.
     *
     * @return true if element space has been updated
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean setElementSpace(int elementSpace) {
        if (elementSpace == mElementSpace) return false;
        mElementSpace = elementSpace;
        return true;
    }

    /** Returns the space between two consecutive elements. */
    public int getElementSpace() {
        return mElementSpace;
    }

    /** Returns the space before the first element. */
    public int getLeadInSpace() {
        return mLeadInSpace;
    }
}
