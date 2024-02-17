// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;


import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.MockedInTests;

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
    private final @Px int mItemWidth;
    private final @Px int mMinItemExposure;
    private final @Px int mMaxItemExposure;
    private @Px int mContainerWidth;
    private boolean mIsPortraitOrientation;

    /**
     * @param leadInSpace the space before the first item in the carousel
     * @param minElementSpace the minimum spacing between two adjacent elements; the space may be
     *     greater than this
     * @param itemWidth the width of a single element
     * @param minItemExposureFraction the minimum exposure of the last visible element; range: [0.f;
     *     1.f]; value of 0.3 means at least 30% of the last element will be exposed
     * @param maxItemExposureFraction the maximum exposure of the last visible element; range: [0.f;
     *     1.f]; value of 0.7 means at most 70% of the last element will be exposed
     */
    public DynamicSpacingRecyclerViewItemDecoration(
            @Px int leadInSpace,
            @Px int minElementSpace,
            @Px int itemWidth,
            float minItemExposureFraction,
            float maxItemExposureFraction) {
        super(leadInSpace, minElementSpace);
        mMinElementSpace = minElementSpace;
        mItemWidth = itemWidth;

        assert minItemExposureFraction >= 0 && minItemExposureFraction <= 1.f;
        assert maxItemExposureFraction >= 0 && maxItemExposureFraction <= 1.f;
        assert minItemExposureFraction <= maxItemExposureFraction;
        mMinItemExposure = (int) (itemWidth * minItemExposureFraction);
        mMaxItemExposure = (int) (itemWidth * maxItemExposureFraction);
        mContainerWidth = 0;
    }

    public DynamicSpacingRecyclerViewItemDecoration(
            @Px int leadInSpace, @Px int minElementSpace, @Px int itemWidth) {
        this(leadInSpace, minElementSpace, itemWidth, 0.5f, 0.5f);
    }

    @Override
    public boolean notifyViewSizeChanged(
            boolean isPortraitOrientation, int newWidth, int newHeight) {
        if (newWidth == mContainerWidth && isPortraitOrientation == mIsPortraitOrientation) {
            return false;
        }

        mContainerWidth = newWidth;
        mIsPortraitOrientation = isPortraitOrientation;
        return setElementSpace(computeElementSpacingPx());
    }

    /**
     * Calculate the margin between tiles based on screen size.
     *
     * @return the requested item spacing, expressed in Pixels
     */
    @VisibleForTesting
    /* package */ int computeElementSpacingPx() {
        if (mIsPortraitOrientation) {
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

            // Calculate fractional item exposure, and adjust it so that the last view is partially
            // exposed within caller-supplied bounds.
            int remainingAreaSize =
                    (adjustedCarouselWidth - numberOfFullyVisibleItems * itemAndSpaceWidth);
            int lastVisibleItemExposure = remainingAreaSize;

            if (lastVisibleItemExposure > mMaxItemExposure) {
                lastVisibleItemExposure = mMaxItemExposure;
            } else if (lastVisibleItemExposure < mMinItemExposure) {
                lastVisibleItemExposure = mMaxItemExposure;
                numberOfFullyVisibleItems--;
            }

            // If tiles are too large (i.e. larger than the screen width), just return default
            // padding. There's nothing we can do.
            if (numberOfFullyVisibleItems <= 0) {
                return mMinElementSpace;
            }

            int totalPaddingAreaSize =
                    adjustedCarouselWidth
                            - (numberOfFullyVisibleItems * mItemWidth)
                            - lastVisibleItemExposure;
            int itemSpacing = totalPaddingAreaSize / numberOfFullyVisibleItems;
            return itemSpacing;
        } else {
            return mMinElementSpace;
        }
    }
}
