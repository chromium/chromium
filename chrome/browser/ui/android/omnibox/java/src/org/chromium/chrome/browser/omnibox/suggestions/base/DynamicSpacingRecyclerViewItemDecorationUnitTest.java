// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;

import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link DynamicSpacingRecyclerViewItemDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DynamicSpacingRecyclerViewItemDecorationUnitTest {
    private static final int CONTAINER_SIZE = 1000;
    private static final int LEAD_IN_SPACE = 10;
    private static final int MIN_ELEMENT_SPACE = 50;
    private static final int ITEM_FIRST = 0;
    private static final int ITEM_MIDDLE = 1;
    private static final int ITEM_LAST = 2;
    private static final int ITEM_COUNT = ITEM_LAST + 1;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock RecyclerView mRecyclerView;
    private @Mock RecyclerView.Adapter mAdapter;
    private @Mock View mFirstView;
    private @Mock View mMiddleView;
    private @Mock View mLastView;

    private DynamicSpacingRecyclerViewItemDecoration mDecoration;
    private Rect mOffsets;

    @Before
    public void setUp() {
        mOffsets = new Rect();

        lenient().doReturn(ITEM_COUNT).when(mAdapter).getItemCount();
        lenient().doReturn(mAdapter).when(mRecyclerView).getAdapter();
        lenient().doReturn(ITEM_FIRST).when(mRecyclerView).getChildAdapterPosition(mFirstView);
        lenient().doReturn(ITEM_MIDDLE).when(mRecyclerView).getChildAdapterPosition(mMiddleView);
        lenient().doReturn(ITEM_LAST).when(mRecyclerView).getChildAdapterPosition(mLastView);
        lenient().doReturn(ContextUtils.getApplicationContext()).when(mRecyclerView).getContext();

        doReturn(CONTAINER_SIZE).when(mRecyclerView).getMeasuredWidth();
    }

    /**
     * Simulate horizontal view resize.
     *
     * @param newWidth new measured width to be reported by the container.
     */
    void resizeContainer(int newWidth) {
        mDecoration.notifyViewSizeChanged(true, newWidth, 100);
    }

    // Compares spacing against expected value.
    // Note that the supplied space is normally applied on both sides of every item, so the
    // totalSpacing = 2 * expectedSpacing
    void verifyItemSpacing(int expectedSpacing) {
        // First item, RTL: lead-in space on the right.
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mRecyclerView).getLayoutDirection();
        mDecoration.getItemOffsets(mOffsets, mFirstView, mRecyclerView, null);
        assertEquals(LEAD_IN_SPACE, mOffsets.right);
        assertEquals(expectedSpacing / 2, mOffsets.left);

        // First item, LTR: lead-in space on the left.
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mRecyclerView).getLayoutDirection();
        mDecoration.getItemOffsets(mOffsets, mFirstView, mRecyclerView, null);
        assertEquals(LEAD_IN_SPACE, mOffsets.left);
        assertEquals(expectedSpacing / 2, mOffsets.right);

        // Second item: same spacing on both sides.
        mDecoration.getItemOffsets(mOffsets, mMiddleView, mRecyclerView, null);
        assertEquals(expectedSpacing / 2, mOffsets.left);
        assertEquals(expectedSpacing / 2, mOffsets.right);

        // Last item, RTL: lead-in space on the left.
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mRecyclerView).getLayoutDirection();
        mDecoration.getItemOffsets(mOffsets, mLastView, mRecyclerView, null);
        assertEquals(expectedSpacing / 2, mOffsets.right);
        assertEquals(LEAD_IN_SPACE, mOffsets.left);

        // Last item, LTR: lead-in space on the right.
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mRecyclerView).getLayoutDirection();
        mDecoration.getItemOffsets(mOffsets, mLastView, mRecyclerView, null);
        assertEquals(expectedSpacing / 2, mOffsets.left);
        assertEquals(LEAD_IN_SPACE, mOffsets.right);
    }

    @Test
    public void computeItemOffsets_reportsDefaultsWhenItemWidthNotKnown() {
        mDecoration =
                new DynamicSpacingRecyclerViewItemDecoration(
                        LEAD_IN_SPACE, MIN_ELEMENT_SPACE, /* itemWidth= */ 0);
        verifyItemSpacing(MIN_ELEMENT_SPACE);
    }

    @Test
    public void computeItemOffsets_portrait_exactFit() {
        int adjustedWidth = CONTAINER_SIZE - LEAD_IN_SPACE;

        // Find out how wide can our theoretical tile be to fit 4.5 times.
        int totalTileAreaSize = adjustedWidth - (4 * MIN_ELEMENT_SPACE);
        int singleTileSize = (int) (totalTileAreaSize / 4.5);

        // Quickly verify our logic. We should not deviate by more than 4.5 pixels (rounding).
        assertEquals((int) (singleTileSize * 4.5 + MIN_ELEMENT_SPACE * 4), adjustedWidth, 4.5);

        // Create a decoration where the tile fits exactly.
        mDecoration =
                new DynamicSpacingRecyclerViewItemDecoration(
                        LEAD_IN_SPACE, MIN_ELEMENT_SPACE, singleTileSize);
        resizeContainer(CONTAINER_SIZE);
        verifyItemSpacing(MIN_ELEMENT_SPACE);
    }

    @Test
    public void computeItemOffsets_portrait_tightFit() {
        int adjustedWidth = CONTAINER_SIZE - LEAD_IN_SPACE;

        // Find out how wide can our theoretical tile be to fit 4.5 times.
        // We want to show
        // - four entire tiles, separated by
        // - four spaces, and finally
        // - a half of the fifth tile.
        // Next, we increase the size of these tiles so that we know we can't fit them.
        int totalTileAreaSize = adjustedWidth - (4 * MIN_ELEMENT_SPACE);
        int singleTileSize = (int) (totalTileAreaSize / 4.5) + 5;

        // Quickly verify our logic. We should exceed the available space, forcing the algorithm to
        // reduce number of visible items.
        assertTrue((int) (singleTileSize * 4.5 + MIN_ELEMENT_SPACE * 4) > adjustedWidth);

        // Compute expected padding in that case.
        int expectedPadding = (int) (adjustedWidth - 3.5 * singleTileSize) / 3;

        // Create a decoration where the tile fits tightly.
        mDecoration =
                new DynamicSpacingRecyclerViewItemDecoration(
                        LEAD_IN_SPACE, MIN_ELEMENT_SPACE, singleTileSize);
        resizeContainer(CONTAINER_SIZE);
        verifyItemSpacing(expectedPadding);
    }

    @Test
    public void computeItemOffsets_portrait_impossibleFit() {
        // No way to fit in 1.5 tiles on screen.
        mDecoration =
                new DynamicSpacingRecyclerViewItemDecoration(
                        LEAD_IN_SPACE, MIN_ELEMENT_SPACE, CONTAINER_SIZE);
        resizeContainer(CONTAINER_SIZE);
        verifyItemSpacing(MIN_ELEMENT_SPACE);
    }

    @Test
    public void notifyViewMeasuredSizeChanged_updatesSpacing() {
        int itemWidth = 11 * MIN_ELEMENT_SPACE;
        int oldItemSpace = 2 * MIN_ELEMENT_SPACE;
        int newItemSpace = 3 * MIN_ELEMENT_SPACE;

        // Compute hypothetical screen sizes that would require the logic to fit:
        // - 5.5 items before and
        // - 4.5 items after measure.
        int oldContainerSize = LEAD_IN_SPACE + 5 * oldItemSpace + (int) (5.5 * itemWidth);
        int newContainerSize = LEAD_IN_SPACE + 4 * newItemSpace + (int) (4.5 * itemWidth);

        mDecoration =
                new DynamicSpacingRecyclerViewItemDecoration(
                        LEAD_IN_SPACE, MIN_ELEMENT_SPACE, itemWidth);

        resizeContainer(oldContainerSize);
        verifyItemSpacing(oldItemSpace);

        // Notify container has resized.
        resizeContainer(newContainerSize);
        verifyItemSpacing(newItemSpace);
    }

    @Test
    public void notifyViewMeasuredSizeChanged_suppressComputationWhenSizeNotChanged() {
        int itemWidth = 11 * MIN_ELEMENT_SPACE;
        int itemSpace = 2 * MIN_ELEMENT_SPACE;
        int containerSize = LEAD_IN_SPACE + 5 * itemSpace + (int) (5.5 * itemWidth);

        mDecoration =
                new DynamicSpacingRecyclerViewItemDecoration(
                        LEAD_IN_SPACE, MIN_ELEMENT_SPACE, itemWidth);
        // Expect the updates here:
        assertTrue(mDecoration.notifyViewSizeChanged(true, containerSize, 100));
        verifyItemSpacing(itemSpace);

        // ... but no updates here:
        assertFalse(mDecoration.notifyViewSizeChanged(true, containerSize, 100));
    }

    @Test
    public void formFactor_itemSpacingPhone_landscape() {
        int itemWidth = CONTAINER_SIZE / 3;
        mDecoration =
                new DynamicSpacingRecyclerViewItemDecoration(
                        LEAD_IN_SPACE, MIN_ELEMENT_SPACE, itemWidth);

        mDecoration.notifyViewSizeChanged(false, CONTAINER_SIZE, /* height= */ 100);

        // It's unlikely that the minimum spacing would guarantee 2.5 items to be shown, but we can
        // verify this fast.
        assertNotEquals(
                CONTAINER_SIZE, (int) (LEAD_IN_SPACE + itemWidth * 2.5 + MIN_ELEMENT_SPACE * 2));

        // However, we don't permit dynamic spacing in landscape mode, so this should fall back to
        // MIN_ELEMENT_SPACE.
        verifyItemSpacing(MIN_ELEMENT_SPACE);
    }

    @Test
    public void computeElementSpacingPx_minFractionalExposure() {
        // Ignore lead-in and element spacing completely to simplify computations.
        // Each element is 100px wide, with minimum exposure fraction of 0.1
        final int itemWidth = 100;
        final float minExposureFrac = 0.1f;
        final float maxExposureFrac = 0.9f;
        final int containerWidth = /* 2.1 * itemWidth=*/ 210;
        mDecoration =
                new DynamicSpacingRecyclerViewItemDecoration(
                        0, 0, itemWidth, minExposureFrac, maxExposureFrac);

        // This should fit EXACTLY 2.1 items with zero spacing.
        resizeContainer(containerWidth);
        mDecoration.getItemOffsets(mOffsets, mFirstView, mRecyclerView, null);
        assertEquals(0, mOffsets.right);
        assertEquals(0, mOffsets.left);
    }

    @Test
    public void computeElementSpacingPx_maxFractionalExposure() {
        // Ignore lead-in and element spacing completely to simplify computations.
        // Each element is 100px wide, with minimum exposure fraction of 0.1
        final int itemWidth = 100;
        final float minExposureFrac = 0.1f;
        final float maxExposureFrac = 0.9f;
        final int containerWidth = /* 2.1 * itemWidth - 1=*/ 209;
        mDecoration =
                new DynamicSpacingRecyclerViewItemDecoration(
                        0, 0, itemWidth, minExposureFrac, maxExposureFrac);

        // This should fit EXACTLY 1.9 items.
        resizeContainer(containerWidth);
        mDecoration.getItemOffsets(mOffsets, mFirstView, mRecyclerView, null);

        // The expected space to separate elements is:
        int itemSpacing = containerWidth - (int) ((1 + maxExposureFrac) * itemWidth);
        assertEquals(itemSpacing / 2, mOffsets.right);
        assertEquals(/* lead-in */ 0, mOffsets.left);
    }

    @Test
    public void computeElementSpacingPx_mediumFractionalExposure() {
        // Ignore lead-in and element spacing completely to simplify computations.
        // Each element is 100px wide, with minimum exposure fraction of 0.1
        final int itemWidth = 100;
        final float minExposureFrac = 0.1f;
        final float maxExposureFrac = 0.9f;
        final int containerWidth = /* 2.5 * itemWidth =*/ 250;
        mDecoration =
                new DynamicSpacingRecyclerViewItemDecoration(
                        0, 0, itemWidth, minExposureFrac, maxExposureFrac);

        resizeContainer(containerWidth);
        mDecoration.getItemOffsets(mOffsets, mFirstView, mRecyclerView, null);
        assertEquals(0, mOffsets.right);
        assertEquals(0, mOffsets.left);
    }
}
