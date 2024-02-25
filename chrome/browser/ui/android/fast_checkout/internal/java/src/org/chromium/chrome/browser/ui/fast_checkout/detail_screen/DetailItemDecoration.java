// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.ui.fast_checkout.R;

/**
 * The item decoration used on the Autofill profile selection screen and the
 * credit card selection screen that adds horizontal spacing between elements
 * and chooses the correct item background depending on the position of the item
 * in the list.
 */
public class DetailItemDecoration extends RecyclerView.ItemDecoration {
    private final int mHorizontalSpacing;

    /**
     * Creates a FastCheckoutDetailItemDecoration.
     * @param horizontalSpacing The spacing between items in pixels.
     */
    public DetailItemDecoration(@Px int horizontalSpacing) {
        mHorizontalSpacing = horizontalSpacing;
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        // The first item does not get additional spacing.
        outRect.top = (parent.getChildAdapterPosition(view) != 0) ? mHorizontalSpacing : 0;
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        for (int index = 0; index < parent.getChildCount(); ++index) {
            View child = parent.getChildAt(index);
            int positionInAdapter = parent.getChildAdapterPosition(child);
            child.setBackground(
                    AppCompatResources.getDrawable(
                            parent.getContext(),
                            getBackgroundDrawable(
                                    positionInAdapter, parent.getAdapter().getItemCount())));
        }
    }

    /**
     * Returns the appropriate item background based on the position of the item in the list.
     * The first item has strongly rounded upper corners, the middle item has weakly
     * rounded corners on the top and the bottom, and the last item has strongly rounded
     * bottom corners.
     * @param position The zero-indexed position in the adapter.
     * @param itemCount The number of items in the adapter.
     * @return The resource ID of the item background.
     */
    private static @DrawableRes int getBackgroundDrawable(int position, int itemCount) {
        if (position == 0) {
            return R.drawable.fast_checkout_background_top;
        }

        if (position == itemCount - 1) {
            return R.drawable.fast_checkout_background_bottom;
        }

        return R.drawable.fast_checkout_background_middle;
    }
}
