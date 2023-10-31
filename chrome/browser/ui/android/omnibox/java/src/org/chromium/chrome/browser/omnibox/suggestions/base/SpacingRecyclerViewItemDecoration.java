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

public class SpacingRecyclerViewItemDecoration extends ItemDecoration {
    private final @NonNull RecyclerView mRecyclerView;
    private @Px int mLeadInSpace;
    private @Px int mElementSpace;

    public SpacingRecyclerViewItemDecoration(
            @NonNull RecyclerView parent, @Px int leadInSpace, @Px int elementSpace) {
        mRecyclerView = parent;
        mLeadInSpace = leadInSpace;
        mElementSpace = elementSpace;
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        outRect.left = outRect.right = mElementSpace;

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
     */
    public void setLeadInSpace(int leadInSpace) {
        if (leadInSpace != mLeadInSpace) {
            mRecyclerView.invalidateItemDecorations();
        }
        mLeadInSpace = leadInSpace;
    }

    /**
     * Specify new element space to be used as an item decoration.
     *
     * <p>Triggers RecyclerView update if the new spacing is different from the old one.
     */
    public void setElementSpace(int elementSpace) {
        if (elementSpace != mElementSpace) {
            mRecyclerView.invalidateItemDecorations();
        }
        mElementSpace = elementSpace;
    }

    @VisibleForTesting
    public int getElementSpaceForTest() {
        return mElementSpace;
    }

    @VisibleForTesting
    public int getLeadInSpaceForTest() {
        return mLeadInSpace;
    }
}
