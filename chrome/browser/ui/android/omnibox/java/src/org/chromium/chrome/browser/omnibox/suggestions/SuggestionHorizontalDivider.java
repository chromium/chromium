// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.Region.Op;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

/**
 * Horizontal divider item decoration that clips the bottom of items with a rect of height  1dp.
 * Only clips for items with an associated property model with DropdownCommonProperties.SHOW_DIVIDER
 * == true.
 */
public class SuggestionHorizontalDivider extends ItemDecoration {
    private final Rect mBounds = new Rect();
    private final int mHeight;

    public SuggestionHorizontalDivider(@NonNull Context context) {
        mHeight = context.getResources().getDimensionPixelSize(R.dimen.divider_height);
    }

    @Override
    public void onDraw(@NonNull Canvas canvas, @NonNull RecyclerView parent, @NonNull State state) {
        int childCount = parent.getChildCount();

        for (int i = 0; i < childCount; ++i) {
            View child = parent.getChildAt(i);
            if (!shouldDrawDivider(child, parent)) continue;
            parent.getDecoratedBoundsWithMargins(child, mBounds);
            canvas.clipRect(mBounds.left, mBounds.bottom - mHeight, mBounds.right, mBounds.bottom,
                    Op.DIFFERENCE);
        }
    }

    @VisibleForTesting
    boolean shouldDrawDivider(@NonNull View view, @NonNull RecyclerView parent) {
        RecyclerView.ViewHolder viewHolder = parent.getChildViewHolder(view);
        if (!(viewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder)) {
            return false;
        }
        SimpleRecyclerViewAdapter.ViewHolder simpleRecyclerViewHolder =
                (ViewHolder) parent.getChildViewHolder(view);
        return simpleRecyclerViewHolder.model.get(DropdownCommonProperties.SHOW_DIVIDER);
    }
}
