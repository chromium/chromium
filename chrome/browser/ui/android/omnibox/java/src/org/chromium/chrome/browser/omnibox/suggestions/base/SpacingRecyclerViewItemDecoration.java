// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;

/**
 * Configures space before the first element of the recycler view, and between each of the elements.
 */
public class SpacingRecyclerViewItemDecoration extends ItemDecoration {
    protected final @NonNull RecyclerView mRecyclerView;
    private @Px int mLeadInSpace;
    private @Px int mElementSpace;

    /**
     * @param parent the RecyclerView component receiving the decoration
     * @param leadInSpace the space before the first element
     * @param elementSpace the total space between each two elements, applied evenly to each side of
     *     every child
     */
    public SpacingRecyclerViewItemDecoration(
            @NonNull RecyclerView parent, @Px int leadInSpace, @Px int elementSpace) {
        mRecyclerView = parent;
        mLeadInSpace = leadInSpace;
        mElementSpace = elementSpace;
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        outRect.left = outRect.right = mElementSpace / 2;

        if (parent.getChildAdapterPosition(view) != 0) return;

        if (parent.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL) {
            outRect.right = mLeadInSpace;
        } else {
            outRect.left = mLeadInSpace;
        }
    }

    /**
     * Specify new lead in space to be used as an item decoration.
     *
     * <p>Triggers RecyclerView update if the new spacing is different from the old one.
     *
     * <p>This call is intended to be used by derived classes. Keep this call protected.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void setLeadInSpace(int leadInSpace) {
        if (leadInSpace == mLeadInSpace) return;
        mLeadInSpace = leadInSpace;
        mRecyclerView.invalidateItemDecorations();
    }

    /**
     * Specify new element space to be used as an item decoration.
     *
     * <p>Triggers RecyclerView update if the new spacing is different from the old one.
     *
     * <p>This call is intended to be used by derived classes. Keep this call protected.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void setElementSpace(int elementSpace) {
        if (elementSpace == mElementSpace) return;
        mElementSpace = elementSpace;
        mRecyclerView.invalidateItemDecorations();
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
