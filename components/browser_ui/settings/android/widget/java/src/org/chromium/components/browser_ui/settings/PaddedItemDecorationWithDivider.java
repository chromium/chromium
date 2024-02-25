// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceViewHolder;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.base.supplier.Supplier;

/**
 * Item decoration that adds padding to each item and can draw divider between items. This is a
 * customized decoration used in {@link androidx.preference.PreferenceFragmentCompat}, and can be
 * used for cases where we don't want to draw views end-to-end in {@link RecyclerView}, while avoid
 * adding views presenting dividers into the view tree.
 *
 * <p>This class uses {@link Supplier} to set the item and divider paddings. This can be useful when
 * the decorated RecyclerView can change in its padding during its lifetime (e.g. When screen
 * configuration changed).
 */
public class PaddedItemDecorationWithDivider extends RecyclerView.ItemDecoration {
    private Drawable mDividerDrawable;
    private int mDividerHeight;
    private boolean mAllowDividerAfterLastItem;
    private @NonNull Supplier<Integer> mDividerPaddingStartSupplier;
    private @NonNull Supplier<Integer> mDividerPaddingEndSupplier;
    private @NonNull Supplier<Integer> mItemOffsetSupplier;

    /**
     * Create the item decoration with padding.
     *
     * @param itemOffsetSupplier Supplier for offset to apply to start and end of each item.
     */
    public PaddedItemDecorationWithDivider(Supplier<Integer> itemOffsetSupplier) {
        mDividerPaddingStartSupplier = () -> 0;
        mDividerPaddingEndSupplier = () -> 0;
        mItemOffsetSupplier = itemOffsetSupplier;
    }

    /** Set whether drawing divider after the last item is allowed. */
    public void setAllowDividerAfterLastItem(boolean allowDividerAfterLastItem) {
        mAllowDividerAfterLastItem = allowDividerAfterLastItem;
    }

    /**
     * Set divider properties for item decoration.
     *
     * @param divider Drawable used for the divider.
     * @param dividerStartPaddingSupplier Start padding used for the divider.
     * @param dividerEndPaddingSupplier End padding used for the divider.
     */
    public void setDividerWithPadding(
            @NonNull Drawable divider,
            @NonNull Supplier<Integer> dividerStartPaddingSupplier,
            @NonNull Supplier<Integer> dividerEndPaddingSupplier) {
        mDividerDrawable = divider;
        mDividerHeight = divider.getIntrinsicHeight();
        mDividerPaddingStartSupplier = dividerStartPaddingSupplier;
        mDividerPaddingEndSupplier = dividerEndPaddingSupplier;
    }

    @Override
    public void onDraw(@NonNull Canvas c, @NonNull RecyclerView parent, @NonNull State state) {
        final int childCount = parent.getChildCount();
        final int width = parent.getWidth();
        int dividerStartPadding = getDividerPaddingStart();
        int dividerEndPadding = getDividerPaddingEnd();
        int itemOffset = mItemOffsetSupplier.get();
        for (int childViewIndex = 0; childViewIndex < childCount; childViewIndex++) {
            final View view = parent.getChildAt(childViewIndex);
            if (shouldDrawDividerBelow(view, parent)) {
                int top = (int) view.getY() + view.getHeight();
                mDividerDrawable.setBounds(
                        itemOffset + dividerStartPadding,
                        top,
                        width - (itemOffset + dividerEndPadding),
                        top + mDividerHeight);
                mDividerDrawable.draw(c);
            }
        }
    }

    @Override
    public void getItemOffsets(
            @NonNull Rect outRect,
            @NonNull View view,
            @NonNull RecyclerView parent,
            @NonNull State state) {
        int itemOffset = mItemOffsetSupplier.get();
        outRect.left = itemOffset;
        outRect.right = itemOffset;
        if (shouldDrawDividerBelow(view, parent)) {
            outRect.bottom = mDividerHeight;
        }
    }

    private boolean shouldDrawDividerBelow(View view, RecyclerView parent) {
        if (mDividerDrawable == null) return false;
        final RecyclerView.ViewHolder holder = parent.getChildViewHolder(view);
        final boolean dividerAllowedBelow =
                holder instanceof PreferenceViewHolder
                        && ((PreferenceViewHolder) holder).isDividerAllowedBelow();
        if (!dividerAllowedBelow) {
            return false;
        }
        boolean nextAllowed = mAllowDividerAfterLastItem;
        int index = parent.indexOfChild(view);
        if (index < parent.getChildCount() - 1) {
            final View nextView = parent.getChildAt(index + 1);
            final RecyclerView.ViewHolder nextHolder = parent.getChildViewHolder(nextView);
            nextAllowed =
                    nextHolder instanceof PreferenceViewHolder
                            && ((PreferenceViewHolder) nextHolder).isDividerAllowedAbove();
        }
        return nextAllowed;
    }

    @VisibleForTesting
    public int getDividerPaddingStart() {
        return mDividerPaddingStartSupplier.get() != null ? mDividerPaddingStartSupplier.get() : 0;
    }

    @VisibleForTesting
    public int getDividerPaddingEnd() {
        return mDividerPaddingEndSupplier.get() != null ? mDividerPaddingEndSupplier.get() : 0;
    }

    public int getItemOffsetForTesting() {
        return mItemOffsetSupplier.get();
    }
}
