// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.graphics.Canvas;
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
 * Horizontal divider item decoration that clips the bottom of items with a rect of height 1dp. Only
 * clips for items with an associated property model with DropdownCommonProperties.SHOW_DIVIDER ==
 * true.
 */
public class SuggestionHorizontalDivider extends ItemDecoration {
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
            float bottom = child.getY() + child.getHeight();
            float left = child.getX();
            // Erase a (child.getWidth() x mHeight) strip from the bottom of the child view being
            // drawn. This creates a "divider" in the form of a mHeight tall line matching the
            // background of the enclosing recycler view. Op.DIFFERENCE means "subtract the op
            // region from the first region" i.e. erase it; see
            // https://skia-doc.commondatastorage.googleapis.com/doxygen/doxygen/html/classSkRegion.html#a2ced93c36095d876b020e20cf39f5b54
            canvas.clipRect(left, bottom - mHeight, left + child.getWidth(), bottom, Op.DIFFERENCE);
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
