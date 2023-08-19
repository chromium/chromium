// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.Px;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;

public class SpacingRecyclerViewItemDecoration extends ItemDecoration {
    public final @Px int leadInSpace;
    public final @Px int elementSpace;

    public SpacingRecyclerViewItemDecoration(@Px int leadInSpace, @Px int elementSpace) {
        this.leadInSpace = leadInSpace;
        this.elementSpace = elementSpace;
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        outRect.left = outRect.right = elementSpace;

        if (parent.getChildAdapterPosition(view) != 0) return;

        if (parent.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL) {
            outRect.right = leadInSpace;
        } else {
            outRect.left = leadInSpace;
        }
    }
}
