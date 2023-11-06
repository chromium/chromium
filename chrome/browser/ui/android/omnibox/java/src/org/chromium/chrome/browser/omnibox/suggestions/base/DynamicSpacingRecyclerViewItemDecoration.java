// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.res.Configuration;

import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.MockedInTests;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;

/**
 * RecyclerView ItemDecoration that dynamically calculates preferred element spacing based on
 *
 * <p>- container size, - initial (lead-in) space, - item size, - minimum item space
 *
 * <p>ensuring that the last item is exactly 50% exposed.
 *
 * <p>Note: currently dynamic spacing is activated in portrait mode only.
 */
@MockedInTests
public class DynamicSpacingRecyclerViewItemDecoration extends SpacingRecyclerViewItemDecoration {
    private final @Px int mMinElementSpace;
    private @Px int mItemWidth;
    private @Px int mContainerWidth;

    public DynamicSpacingRecyclerViewItemDecoration(
            @NonNull RecyclerView parent, @Px int leadInSpace, @Px int minElementSpace) {
        super(parent, leadInSpace, minElementSpace);
        mMinElementSpace = minElementSpace;
        mContainerWidth = parent.getMeasuredWidth();
    }

    /**
     * Set the new itemWidth to be used for computing dynamic element spacing.
     *
     * <p>Calling this method may trigger re-layout of the RecyclerView elements.
     */
    public void setItemWidth(@Px int itemWidth) {
        if (mItemWidth == itemWidth) return;
        mItemWidth = itemWidth;
        setElementSpace(computeElementSpacingPx());
    }

    /**
     * Notify that the RecyclerView container size has changed.
     *
     * <p>Calling this method may trigger re-layout of the RecyclerView elements.
     *
     * <p>Note that this call should be run either
     *
     * <ul>
     *   <li>(ideally) ahead of the Layout pass (e.g. during onMeasure()), where the spacing could
     *       be computed before items are laid out, or
     *   <li>after the Layout pass (which may trigger a secondary Layout pass to update children
     *       placement).
     * </ul>
     *
     * <p>Calling the method from within onLayout when the Layout pass nears completion may result
     * with newly computed space and the request to invalidateItemDecorations() being ignored.
     */
    public void notifyViewMeasuredSizeChanged() {
        int measuredSize = mRecyclerView.getMeasuredWidth();
        if (measuredSize == mContainerWidth) return;
        mContainerWidth = measuredSize;
        setElementSpace(computeElementSpacingPx());
    }

    /**
     * Calculate the margin between tiles based on screen size.
     *
     * @return the requested item spacing, expressed in Pixels
     */
    @VisibleForTesting
    /* package */ int computeElementSpacingPx() {
        var context = mRecyclerView.getContext();
        var resources = context.getResources();

        boolean isPortraitOrientation =
                resources.getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
        if (isPortraitOrientation && OmniboxFeatures.shouldShowModernizeVisualUpdate(context)) {
            // Compute item spacing, guaranteeing exactly 50% exposure of one item
            // given the carousel width, item width, initial spacing, and base item spacing.
            // Resulting item spacing must be no smaller than base item spacing.
            //
            // Given a carousel entry:
            //   |__XXXX...XXXX...XXXX...XX|
            // where:
            // - '|' marks boundaries of the carousel,
            // - '_' is the initial spacing,
            // - 'X' is a carousel element, and
            // - '.' is the item space
            // computes the width of item space ('.').
            int adjustedCarouselWidth = mContainerWidth - getLeadInSpace();
            int itemAndSpaceWidth = mItemWidth + mMinElementSpace;
            int numberOfFullyVisibleItems = adjustedCarouselWidth / itemAndSpaceWidth;

            // We know the number of items that will be fully visible on screen.
            // Another item may be partially exposed.
            // Now we check how much of that item is visible; if it's less than 50% exposed, we
            // reduce number of fully exposed items to show, and increase padding.
            if ((adjustedCarouselWidth - numberOfFullyVisibleItems * itemAndSpaceWidth)
                    < 0.5 * mItemWidth) {
                numberOfFullyVisibleItems--;
            }

            // If tiles are too large (i.e. larger than the screen width), just return default
            // padding. There's nothing we can do.
            if (numberOfFullyVisibleItems <= 0) {
                return mMinElementSpace;
            }

            int totalPaddingAreaSize =
                    adjustedCarouselWidth - (int) ((numberOfFullyVisibleItems + 0.5) * mItemWidth);
            int itemSpacing = totalPaddingAreaSize / numberOfFullyVisibleItems;
            // Divided by 2 - spacing is applied evenly on left and right hand side of an item.
            return itemSpacing;
        } else {
            return mMinElementSpace;
        }
    }

    @VisibleForTesting
    public int getItemWidthForTesting() {
        return mItemWidth;
    }
}
