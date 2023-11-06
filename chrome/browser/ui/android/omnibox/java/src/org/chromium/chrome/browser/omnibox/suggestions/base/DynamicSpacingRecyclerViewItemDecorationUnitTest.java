// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/** Tests for {@link DynamicSpacingRecyclerViewItemDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
public class DynamicSpacingRecyclerViewItemDecorationUnitTest {
    private static final int CONTAINER_SIZE = 1000;
    private static final int LEAD_IN_SPACE = 10;
    private static final int MIN_ELEMENT_SPACE = 50;

    public @Rule TestRule mFeatures = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock RecyclerView mRecyclerView;
    private @Mock View mFirstView;
    private @Mock View mSecondView;

    private DynamicSpacingRecyclerViewItemDecoration mDecoration;
    private Rect mOffsets;
    private int mMinimumSpace;

    @Before
    public void setUp() {
        mOffsets = new Rect();

        lenient().doReturn(0).when(mRecyclerView).getChildAdapterPosition(mFirstView);
        lenient().doReturn(1).when(mRecyclerView).getChildAdapterPosition(mSecondView);
        lenient().doReturn(ContextUtils.getApplicationContext()).when(mRecyclerView).getContext();

        doReturn(CONTAINER_SIZE).when(mRecyclerView).getMeasuredWidth();

        mDecoration =
                spy(
                        new DynamicSpacingRecyclerViewItemDecoration(
                                mRecyclerView, LEAD_IN_SPACE, MIN_ELEMENT_SPACE));
    }

    /**
     * Simulate horizontal view resize.
     *
     * @param newWidth new measured width to be reported by the container.
     */
    void resizeContainer(int newWidth) {
        doReturn(newWidth).when(mRecyclerView).getMeasuredWidth();
        mDecoration.notifyViewMeasuredSizeChanged();
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
        mDecoration.getItemOffsets(mOffsets, mSecondView, mRecyclerView, null);
        assertEquals(expectedSpacing / 2, mOffsets.left);
        assertEquals(expectedSpacing / 2, mOffsets.right);
    }

    @Test
    @Config(qualifiers = "sw480dp-port")
    public void computeItemOffsets_reportsDefaultsWhenItemWidthNotKnown() {
        verifyItemSpacing(MIN_ELEMENT_SPACE);

        // Simulate container resize. This should have no unintended side effects.
        resizeContainer(1234);
        verifyItemSpacing(MIN_ELEMENT_SPACE);

        // Same should apply if item width is set to 0.
        mDecoration.setItemWidth(0);
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

        // Notify the ItemDecoration of a new item size, and check the spacing is exactly same as
        // minimum spacing.
        mDecoration.setItemWidth(singleTileSize);
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

        mDecoration.setItemWidth(singleTileSize);
        verifyItemSpacing(expectedPadding);
    }

    @Test
    public void computeItemOffsets_portrait_impossibleFit() {
        // No way to fit in 1.5 tiles on screen.
        mDecoration.setItemWidth(CONTAINER_SIZE);
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

        resizeContainer(oldContainerSize);
        mDecoration.setItemWidth(itemWidth);
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

        resizeContainer(containerSize);
        mDecoration.setItemWidth(itemWidth);
        verifyItemSpacing(itemSpace);

        clearInvocations(mDecoration);

        // Notify container has resized, observe no update.
        mDecoration.notifyViewMeasuredSizeChanged();
        verify(mDecoration, times(0)).computeElementSpacingPx();
    }

    @Test
    @Config(qualifiers = "land")
    public void formFactor_itemSpacingPhone_landscape() {
        // Set the item width to be 1/3 of the carousel.
        int itemWidth = CONTAINER_SIZE / 3;
        mDecoration.setItemWidth(itemWidth);
        // It's unlikely that the minimum spacing would guarantee 2.5 items to be shown, but we can
        // verify this fast.
        assertNotEquals(CONTAINER_SIZE, LEAD_IN_SPACE + itemWidth * 2.5 + MIN_ELEMENT_SPACE * 2);

        // However, we don't permit dynamic spacing in landscape mode, so this should fall back to
        // MIN_ELEMENT_SPACE.
        verifyItemSpacing(MIN_ELEMENT_SPACE);
    }
}
