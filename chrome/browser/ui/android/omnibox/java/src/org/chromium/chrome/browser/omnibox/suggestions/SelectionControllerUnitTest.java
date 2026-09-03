// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.SelectionController.TraversalMode;

import java.util.HashSet;
import java.util.Set;

/** Robolectric unit tests for {@link SelectionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SelectionControllerUnitTest {
    private static final int DEFAULT_NUM_ITEMS = 3;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private int mItemCount = DEFAULT_NUM_ITEMS;
    private final Set<Integer> mUnselectablePositions = new HashSet<>();

    @Before
    public void setUp() {
        mItemCount = DEFAULT_NUM_ITEMS;
        mUnselectablePositions.clear();
    }

    private SelectionController createTestController(@TraversalMode int mode) {
        return spy(
                new SelectionController(mode) {
                    @Override
                    protected void setItemState(int position, boolean isSelected) {}

                    @Override
                    protected int getItemCount() {
                        return mItemCount;
                    }

                    @Override
                    protected boolean isSelectableItem(int position) {
                        return !mUnselectablePositions.contains(position);
                    }
                });
    }

    private void verifyPositionReset(SelectionController c, int position) {
        verify(c).setItemState(position, /* isSelected= */ false);
        assertEquals(null, c.getPosition());
        assertTrue(c.isParkedAtSentinel());
        clearInvocations(c);
    }

    private void verifyPositionSet(SelectionController c, int position) {
        verify(c).setItemState(position, /* isSelected= */ true);
        assertEquals(Integer.valueOf(position), c.getPosition());
        assertFalse(c.isParkedAtSentinel());
        clearInvocations(c);
    }

    private void verifyPositionChanged(SelectionController c, int from, int to) {
        verify(c).setItemState(from, /* isSelected= */ false);
        verifyPositionSet(c, to);
    }

    @Test
    public void selectNextItem_saturating() {
        var c = createTestController(TraversalMode.SATURATING);
        c.reset();

        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 1);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 2);

        // Cannot move any further. We've reached the limit.
        assertFalse(c.selectNextItem());
        assertEquals(Integer.valueOf(2), c.getPosition());

        assertFalse(c.selectNextItem());
        assertEquals(Integer.valueOf(2), c.getPosition());
    }

    @Test
    public void selectNextItem_saturatingWithSentinel() {
        var c = createTestController(TraversalMode.SATURATING_WITH_SENTINEL);
        c.reset();

        assertTrue(c.isParkedAtSentinel());

        assertTrue(c.selectNextItem());
        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 1);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 2);

        assertFalse(c.selectNextItem());
        verifyPositionReset(c, 2);

        assertFalse(c.selectNextItem());
        assertFalse(c.selectNextItem());
    }

    @Test
    public void selectPreviousItem_saturating() {
        var c = createTestController(TraversalMode.SATURATING);
        c.reset();

        c.setPosition(DEFAULT_NUM_ITEMS);
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 2);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 2, /* to= */ 1);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 0);

        // Cannot move any further. We've reached the limit.
        assertFalse(c.selectPreviousItem());
        assertEquals(Integer.valueOf(0), c.getPosition());

        assertFalse(c.selectPreviousItem());
        assertEquals(Integer.valueOf(0), c.getPosition());
    }

    @Test
    public void selectPreviousItem_saturatingWithSentinel() {
        var c = createTestController(TraversalMode.SATURATING_WITH_SENTINEL);
        c.reset();

        c.setPosition(DEFAULT_NUM_ITEMS - 1);
        verifyPositionSet(c, 2);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 2, /* to= */ 1);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 0);

        assertFalse(c.selectPreviousItem());
        verifyPositionReset(c, 0);

        assertFalse(c.selectPreviousItem());
        assertFalse(c.selectPreviousItem());
    }

    @Test
    public void selectNextItem_skipMiddleItems_saturating() {
        var c = createTestController(TraversalMode.SATURATING);
        mUnselectablePositions.add(1);
        c.reset();

        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());

        verify(c).setItemState(/* position= */ 0, /* isSelected= */ false);
        verify(c).setItemState(/* position= */ 2, /* isSelected= */ true);
        assertEquals(Integer.valueOf(2), c.getPosition());
    }

    @Test
    public void selectPreviousItem_skipMiddleItems_saturating() {
        var c = createTestController(TraversalMode.SATURATING);
        mUnselectablePositions.add(1);
        c.reset();

        c.setPosition(2);
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 2);
        assertTrue(c.selectPreviousItem());

        // This will try to move away from position 0 twice
        // - to advance to position 1, which will fail
        // - then, to advance to position 0, which should work.
        verify(c).setItemState(/* position= */ 2, /* isSelected= */ false);
        verify(c).setItemState(/* position= */ 0, /* isSelected= */ true);
        verify(c, times(2)).setItemState(anyInt(), anyBoolean());
        assertEquals(Integer.valueOf(0), c.getPosition());
    }

    @Test
    public void selectNextItem_skipTailItems_saturating() {
        var c = createTestController(TraversalMode.SATURATING);
        mUnselectablePositions.add(1);
        mUnselectablePositions.add(2);
        c.reset();

        verifyPositionSet(c, 0);

        assertFalse(c.selectNextItem());

        // Selection never moved.
        verify(c, never()).setItemState(anyInt(), anyBoolean());

        // We shouldn't move the selection.
        assertEquals(Integer.valueOf(0), c.getPosition());
    }

    @Test
    public void selectPreviousItem_skipTailItems_saturating() {
        var c = createTestController(TraversalMode.SATURATING);
        mUnselectablePositions.add(1);
        mUnselectablePositions.add(0);

        c.setPosition(2);
        verifyPositionSet(c, 2);
        assertFalse(c.selectPreviousItem());

        // Selection never moved.
        verify(c, never()).setItemState(anyInt(), anyBoolean());

        // We shouldn't move the selection.
        assertEquals(Integer.valueOf(2), c.getPosition());
    }

    @Test
    public void selectNextItem_skipTailItems_saturatingWithSentinel() {
        var c = createTestController(TraversalMode.SATURATING_WITH_SENTINEL);
        mUnselectablePositions.add(1);
        mUnselectablePositions.add(2);
        c.reset();

        // Sentinel -> position 0:
        assertTrue(c.selectNextItem());
        verifyPositionSet(c, 0);

        // Position 0 -> (skipping 1 & 2) -> Sentinel
        assertFalse(c.selectNextItem());
        verifyPositionReset(c, 0);
        assertEquals(null, c.getPosition());
    }

    @Test
    public void selectPreviousItem_skipTailItems_saturatingWithSentinel() {
        var c = createTestController(TraversalMode.SATURATING_WITH_SENTINEL);
        mUnselectablePositions.add(1);
        mUnselectablePositions.add(0);

        c.setPosition(2);
        verifyPositionSet(c, 2);
        assertFalse(c.selectPreviousItem());

        // Selection reset.
        verifyPositionReset(c, 2);
        assertEquals(null, c.getPosition());
    }

    @Test
    public void selectNextItem_noSelectableItems_saturating() {
        var c = createTestController(TraversalMode.SATURATING);
        mUnselectablePositions.add(0);
        mUnselectablePositions.add(1);
        mUnselectablePositions.add(2);
        c.reset();

        assertTrue(c.isParkedAtSentinel());
        assertFalse(c.selectNextItem());
    }

    @Test
    public void selectPreviousItem_noSelectableItems_saturating() {
        var c = createTestController(TraversalMode.SATURATING);
        mUnselectablePositions.add(0);
        mUnselectablePositions.add(1);
        mUnselectablePositions.add(2);
        c.reset();

        assertTrue(c.isParkedAtSentinel());
        assertFalse(c.selectNextItem());
    }

    @Test
    public void selectionControllerWithNoItems() {
        var c = createTestController(TraversalMode.SATURATING);
        mItemCount = 0;
        c.reset();

        // Normally, saturating controller should start at valid range, but this is an edge case.
        assertTrue(c.isParkedAtSentinel());
        assertEquals(null, c.getPosition());

        // Simulate we now have an item. This should make the saturating controller immediately jump
        // to the first valid item.
        mItemCount = 1;
        c.reset();
        assertFalse(c.isParkedAtSentinel());
        assertEquals(Integer.valueOf(0), c.getPosition());

        // Simulate we lost all items. This should make the saturating controller revert to sentnel.
        mItemCount = 0;
        c.reset();
        assertTrue(c.isParkedAtSentinel());
        assertEquals(null, c.getPosition());
    }

    @Test
    public void reset_saturating() {
        var c = createTestController(TraversalMode.SATURATING);
        c.reset();

        verifyPositionSet(c, 0);

        c.selectNextItem(); // 1
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 1);
        c.reset(); // back to default (0)
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 0);
    }

    @Test
    public void selectionControllerWithNoItems_wrapping() {
        var c = createTestController(TraversalMode.WRAPPING);
        mItemCount = 0;
        c.reset();

        assertTrue(c.isParkedAtSentinel());
        assertEquals(null, c.getPosition());

        mItemCount = 1;
        c.reset();
        assertFalse(c.isParkedAtSentinel());
        assertEquals(Integer.valueOf(0), c.getPosition());
    }

    @Test
    public void selectionControllerWithNoItems_wrappingWithSentinel() {
        var c = createTestController(TraversalMode.WRAPPING_WITH_SENTINEL);
        mItemCount = 0;
        c.reset();

        assertTrue(c.isParkedAtSentinel());
        assertEquals(null, c.getPosition());

        mItemCount = 1;
        c.reset();
        assertTrue(c.isParkedAtSentinel());
        assertEquals(null, c.getPosition());
    }

    @Test
    public void selectNextItem_onlyOneSelectableItem_wrapping() {
        var c = createTestController(TraversalMode.WRAPPING);
        mUnselectablePositions.add(1);
        mUnselectablePositions.add(2);
        c.reset();

        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 0);
    }

    @Test
    public void reset_wrapping() {
        var c = createTestController(TraversalMode.WRAPPING);
        c.reset();

        verifyPositionSet(c, 0);

        c.selectNextItem();
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 1);
        c.reset();
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 0);
    }

    @Test
    public void reset_wrappingWithSentinel() {
        var c = createTestController(TraversalMode.WRAPPING_WITH_SENTINEL);
        c.reset();

        assertTrue(c.isParkedAtSentinel());

        c.selectNextItem();
        verifyPositionSet(c, 0);
        c.reset();
        verifyPositionReset(c, 0);
    }

    @Test
    public void selectNextItem_wrapping() {
        var c = createTestController(TraversalMode.WRAPPING);
        c.reset();

        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 1);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 2);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 2, /* to= */ 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 1);
    }

    @Test
    public void selectNextItem_wrappingWithSentinel() {
        var c = createTestController(TraversalMode.WRAPPING_WITH_SENTINEL);
        c.reset();

        assertTrue(c.isParkedAtSentinel());

        assertTrue(c.selectNextItem());
        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 1);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 2);

        assertFalse(c.selectNextItem());
        verifyPositionReset(c, 2);

        assertTrue(c.selectNextItem());
        verifyPositionSet(c, 0);
    }

    @Test
    public void selectPreviousItem_wrapping() {
        var c = createTestController(TraversalMode.WRAPPING);
        c.reset();

        verifyPositionSet(c, 0);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 2);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 2, /* to= */ 1);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 0);
    }

    @Test
    public void selectPreviousItem_wrappingWithSentinel() {
        var c = createTestController(TraversalMode.WRAPPING_WITH_SENTINEL);
        c.reset();

        assertTrue(c.isParkedAtSentinel());

        assertTrue(c.selectPreviousItem());
        verifyPositionSet(c, 2);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 2, /* to= */ 1);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 0);

        assertFalse(c.selectPreviousItem());
        verifyPositionReset(c, 0);

        assertTrue(c.selectPreviousItem());
        verifyPositionSet(c, 2);
    }

    @Test
    public void reset_sentinelThenWrapping() {
        var c = createTestController(TraversalMode.SENTINEL_THEN_WRAPPING);
        c.reset();

        assertTrue(c.isParkedAtSentinel());

        c.selectNextItem();
        verifyPositionSet(c, 0);
        c.reset();
        verifyPositionReset(c, 0);
    }

    @Test
    public void selectNextItem_sentinelThenWrapping() {
        var c = createTestController(TraversalMode.SENTINEL_THEN_WRAPPING);
        c.reset();

        assertTrue(c.isParkedAtSentinel());

        assertTrue(c.selectNextItem());
        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 1);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 2);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 2, /* to= */ 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 1);
    }

    @Test
    public void selectPreviousItem_sentinelThenWrapping() {
        var c = createTestController(TraversalMode.SENTINEL_THEN_WRAPPING);
        c.reset();

        assertTrue(c.isParkedAtSentinel());

        assertTrue(c.selectPreviousItem());
        verifyPositionSet(c, 2);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 2, /* to= */ 1);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 1, /* to= */ 0);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 2);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, /* from= */ 2, /* to= */ 1);
    }

    @Test
    public void selectNextItem_skipMiddleItems_wrapping() {
        var c = createTestController(TraversalMode.WRAPPING);
        mUnselectablePositions.add(1);
        c.reset();

        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 2);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 2, /* to= */ 0);
    }

    @Test
    public void selectNextItem_skipMiddleItems_wrappingWithSentinel() {
        var c = createTestController(TraversalMode.WRAPPING_WITH_SENTINEL);
        mUnselectablePositions.add(1);
        c.reset();

        assertTrue(c.selectNextItem());
        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, /* from= */ 0, /* to= */ 2);

        assertFalse(c.selectNextItem());
        verifyPositionReset(c, 2);

        assertTrue(c.selectNextItem());
        verifyPositionSet(c, 0);
    }

    @Test
    public void selectNextItem_noSelectableItems_wrapping() {
        var c = createTestController(TraversalMode.WRAPPING);
        mUnselectablePositions.add(0);
        mUnselectablePositions.add(1);
        mUnselectablePositions.add(2);
        c.reset();

        assertTrue(c.isParkedAtSentinel());

        assertFalse(c.selectNextItem());
        assertTrue(c.isParkedAtSentinel());

        assertFalse(c.selectPreviousItem());
        assertTrue(c.isParkedAtSentinel());
    }

    @Test
    public void selectNextItem_noSelectableItems_wrappingWithSentinel() {
        var c = createTestController(TraversalMode.WRAPPING_WITH_SENTINEL);
        mUnselectablePositions.add(0);
        mUnselectablePositions.add(1);
        mUnselectablePositions.add(2);
        c.reset();

        assertTrue(c.isParkedAtSentinel());

        assertFalse(c.selectNextItem());
        assertTrue(c.isParkedAtSentinel());

        assertFalse(c.selectPreviousItem());
        assertTrue(c.isParkedAtSentinel());
    }

    @Test
    public void selectNextItem_onlyMiddleSelectableItem_wrappingWithSentinel() {
        var c = createTestController(TraversalMode.WRAPPING_WITH_SENTINEL);
        mUnselectablePositions.add(0);
        mUnselectablePositions.add(2);
        c.reset();

        assertTrue(c.selectNextItem());
        verifyPositionSet(c, 1);

        assertFalse(c.selectNextItem());
        verifyPositionReset(c, 1);

        assertTrue(c.selectPreviousItem());
        verifyPositionSet(c, 1);

        assertFalse(c.selectPreviousItem());
        verifyPositionReset(c, 1);
    }

    @Test
    public void setSelectionMode() {
        var c = createTestController(TraversalMode.WRAPPING);
        c.reset();
        verifyPositionSet(c, 0);

        c.setSelectionMode(TraversalMode.WRAPPING_WITH_SENTINEL);
        c.reset();
        verifyPositionReset(c, 0);
        assertTrue(c.isParkedAtSentinel());

        c.setSelectionMode(TraversalMode.WRAPPING);
        c.reset();
        verifyPositionSet(c, 0);
        assertFalse(c.isParkedAtSentinel());

        c.setSelectionMode(TraversalMode.SENTINEL_THEN_WRAPPING);
        c.reset();
        verifyPositionReset(c, 0);
        assertTrue(c.isParkedAtSentinel());
    }

    @Test
    public void testSelectFirstAndLastAttachment() {
        var c = createTestController(TraversalMode.WRAPPING_WITH_SENTINEL);

        c.selectFirstItem();
        assertEquals(0, c.getPosition().intValue());

        c.selectLastItem();
        assertEquals(2, c.getPosition().intValue());
    }
}
