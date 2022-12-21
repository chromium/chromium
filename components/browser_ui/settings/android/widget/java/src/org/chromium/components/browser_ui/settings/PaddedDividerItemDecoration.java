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
 * Item decoration that can draw divider between items. This is a customized divider decoration used
 * in {@link androidx.preference.PreferenceFragmentCompat}, and can be used for cases where we don't
 * want to draw divider end-to-end in {@link RecyclerView}, while avoid adding views presenting
 * dividers into the view tree.
 *
 * This class uses {@link Supplier} to set the padding start and end. This can be useful when the
 * decorated RecyclerView can change in its padding during its lifetime (e.g. When screen
 * configuration changed).
 */
public class PaddedDividerItemDecoration extends RecyclerView.ItemDecoration {
    private final Drawable mDividerDrawable;
    private final int mDividerHeight;

    private boolean mAllowDividerAfterLastItem;
    private @NonNull Supplier<Integer> mPaddingStartSupplier;
    private @NonNull Supplier<Integer> mPaddingEndSupplier;

    /**
     * Create the item decoration with a divider drawable.
     * @param divider Drawable used for the divider.
     */
    public PaddedDividerItemDecoration(Drawable divider) {
        mDividerDrawable = divider;
        mDividerHeight = divider.getIntrinsicHeight();

        mPaddingStartSupplier = () -> 0;
        mPaddingEndSupplier = () -> 0;
    }

    /**
     * Set the padding added at the start of the divider.
     */
    public void setPaddingStart(@NonNull Supplier<Integer> paddingSupplier) {
        mPaddingStartSupplier = paddingSupplier;
    }

    /**
     * Set the padding added at the end of the divider.
     */
    public void setPaddingEnd(@NonNull Supplier<Integer> paddingSupplier) {
        mPaddingEndSupplier = paddingSupplier;
    }

    /**
     * Set whether drawing divider after the last item is allowed.
     * */
    public void setAllowDividerAfterLastItem(boolean allowDividerAfterLastItem) {
        mAllowDividerAfterLastItem = allowDividerAfterLastItem;
    }

    @Override
    public void onDraw(@NonNull Canvas c, @NonNull RecyclerView parent, @NonNull State state) {
        final int childCount = parent.getChildCount();
        final int width = parent.getWidth();
        int paddingStart = getDividerPaddingStart();
        int paddingEnd = getDividerPaddingEnd();
        for (int childViewIndex = 0; childViewIndex < childCount; childViewIndex++) {
            final View view = parent.getChildAt(childViewIndex);
            if (shouldDrawDividerBelow(view, parent)) {
                int top = (int) view.getY() + view.getHeight();
                mDividerDrawable.setBounds(
                        paddingStart, top, width - paddingEnd, top + mDividerHeight);
                mDividerDrawable.draw(c);
            }
        }
    }

    @Override
    public void getItemOffsets(@NonNull Rect outRect, @NonNull View view,
            @NonNull RecyclerView parent, @NonNull State state) {
        if (shouldDrawDividerBelow(view, parent)) {
            outRect.bottom = mDividerHeight;
        }
    }

    private boolean shouldDrawDividerBelow(View view, RecyclerView parent) {
        final RecyclerView.ViewHolder holder = parent.getChildViewHolder(view);
        final boolean dividerAllowedBelow = holder instanceof PreferenceViewHolder
                && ((PreferenceViewHolder) holder).isDividerAllowedBelow();
        if (!dividerAllowedBelow) {
            return false;
        }
        boolean nextAllowed = mAllowDividerAfterLastItem;
        int index = parent.indexOfChild(view);
        if (index < parent.getChildCount() - 1) {
            final View nextView = parent.getChildAt(index + 1);
            final RecyclerView.ViewHolder nextHolder = parent.getChildViewHolder(nextView);
            nextAllowed = nextHolder instanceof PreferenceViewHolder
                    && ((PreferenceViewHolder) nextHolder).isDividerAllowedAbove();
        }
        return nextAllowed;
    }

    @VisibleForTesting
    public int getDividerPaddingStart() {
        return mPaddingStartSupplier.get() != null ? mPaddingStartSupplier.get() : 0;
    }

    @VisibleForTesting
    public int getDividerPaddingEnd() {
        return mPaddingEndSupplier.get() != null ? mPaddingEndSupplier.get() : 0;
    }
}
