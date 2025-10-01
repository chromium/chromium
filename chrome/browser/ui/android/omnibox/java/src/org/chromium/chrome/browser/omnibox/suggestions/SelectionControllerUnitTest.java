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
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.SelectionController.Mode;

/** Robolectric unit tests for {@link SelectionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SelectionControllerUnitTest {
    private static final int DEFAULT_NUM_ITEMS = 3;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private SelectionController createTestController(@Mode int mode) {
        return spy(
                new SelectionController(mode) {
                    @Override
                    protected void setItemState(int position, boolean isSelected) {}

                    @Override
                    protected int getItemCount() {
                        return DEFAULT_NUM_ITEMS;
                    }
                });
    }

    private void verifyPositionReset(SelectionController c, int position) {
        verify(c).setItemState(position, false);
        assertEquals(null, c.getPosition());
        assertTrue(c.isParkedAtSentinel());
        clearInvocations(c);
    }

    private void verifyPositionSet(SelectionController c, int position) {
        verify(c).setItemState(position, true);
        assertEquals(Integer.valueOf(position), c.getPosition());
        assertFalse(c.isParkedAtSentinel());
        clearInvocations(c);
    }

    private void verifyPositionChanged(SelectionController c, int from, int to) {
        verify(c).setItemState(from, false);
        verifyPositionSet(c, to);
    }

    @Test
    public void selectNextItem_saturating() {
        var c = createTestController(Mode.SATURATING);
        c.reset();

        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, 0, 1);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, 1, 2);

        // Cannot move any further. We've reached the limit.
        assertFalse(c.selectNextItem());
        assertEquals(Integer.valueOf(2), c.getPosition());

        assertFalse(c.selectNextItem());
        assertEquals(Integer.valueOf(2), c.getPosition());
    }

    @Test
    public void selectNextItem_saturatingWithSentinel() {
        var c = createTestController(Mode.SATURATING_WITH_SENTINEL);
        c.reset();

        assertTrue(c.isParkedAtSentinel());

        assertTrue(c.selectNextItem());
        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, 0, 1);

        assertTrue(c.selectNextItem());
        verifyPositionChanged(c, 1, 2);

        assertFalse(c.selectNextItem());
        verifyPositionReset(c, 2);

        assertFalse(c.selectNextItem());
        assertFalse(c.selectNextItem());
    }

    @Test
    public void selectPreviousItem_saturating() {
        var c = createTestController(Mode.SATURATING);
        c.reset();

        c.setPosition(DEFAULT_NUM_ITEMS);
        verifyPositionChanged(c, 0, 2);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, 2, 1);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, 1, 0);

        // Cannot move any further. We've reached the limit.
        assertFalse(c.selectPreviousItem());
        assertEquals(Integer.valueOf(0), c.getPosition());

        assertFalse(c.selectPreviousItem());
        assertEquals(Integer.valueOf(0), c.getPosition());
    }

    @Test
    public void selectPreviousItem_saturatingWithSentinel() {
        var c = createTestController(Mode.SATURATING_WITH_SENTINEL);
        c.reset();

        c.setPosition(DEFAULT_NUM_ITEMS - 1);
        verifyPositionSet(c, 2);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, 2, 1);

        assertTrue(c.selectPreviousItem());
        verifyPositionChanged(c, 1, 0);

        assertFalse(c.selectPreviousItem());
        verifyPositionReset(c, 0);

        assertFalse(c.selectPreviousItem());
        assertFalse(c.selectPreviousItem());
    }

    @Test
    public void selectNextItem_skipMiddleItems_saturating() {
        var c = createTestController(Mode.SATURATING);
        when(c.isSelectableItem(1)).thenReturn(false);
        c.reset();

        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem());

        verify(c, times(1)).setItemState(0, false);
        verify(c, times(1)).setItemState(2, true);
        assertEquals(Integer.valueOf(2), c.getPosition());
    }

    @Test
    public void selectPreviousItem_skipMiddleItems_saturating() {
        var c = createTestController(Mode.SATURATING);
        when(c.isSelectableItem(1)).thenReturn(false);
        c.reset();

        c.setPosition(2);
        verifyPositionChanged(c, 0, 2);
        assertTrue(c.selectPreviousItem());

        // This will try to move away from position 0 twice
        // - to advance to position 1, which will fail
        // - then, to advance to position 0, which should work.
        verify(c, times(1)).setItemState(2, false);
        verify(c, times(1)).setItemState(0, true);
        verify(c, times(2)).setItemState(anyInt(), anyBoolean());
        assertEquals(Integer.valueOf(0), c.getPosition());
    }

    @Test
    public void selectNextItem_skipTailItems_saturating() {
        var c = createTestController(Mode.SATURATING);
        when(c.isSelectableItem(1)).thenReturn(false);
        when(c.isSelectableItem(2)).thenReturn(false);
        c.reset();

        verifyPositionSet(c, 0);

        assertFalse(c.selectNextItem());

        // Selection never moved.
        verify(c, times(0)).setItemState(anyInt(), anyBoolean());

        // We shouldn't move the selection.
        assertEquals(Integer.valueOf(0), c.getPosition());
    }

    @Test
    public void selectPreviousItem_skipTailItems_saturating() {
        var c = createTestController(Mode.SATURATING);
        when(c.isSelectableItem(1)).thenReturn(false);
        when(c.isSelectableItem(0)).thenReturn(false);

        c.setPosition(2);
        verifyPositionSet(c, 2);
        assertFalse(c.selectPreviousItem());

        // Selection never moved.
        verify(c, times(0)).setItemState(anyInt(), anyBoolean());

        // We shouldn't move the selection.
        assertEquals(Integer.valueOf(2), c.getPosition());
    }

    @Test
    public void selectNextItem_skipTailItems_saturatingWithSentinel() {
        var c = createTestController(Mode.SATURATING_WITH_SENTINEL);
        when(c.isSelectableItem(1)).thenReturn(false);
        when(c.isSelectableItem(2)).thenReturn(false);
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
        var c = createTestController(Mode.SATURATING_WITH_SENTINEL);
        when(c.isSelectableItem(1)).thenReturn(false);
        when(c.isSelectableItem(0)).thenReturn(false);

        c.setPosition(2);
        verifyPositionSet(c, 2);
        assertFalse(c.selectPreviousItem());

        // Selection reset.
        verifyPositionReset(c, 2);
        assertEquals(null, c.getPosition());
    }

    @Test
    public void selectNextItem_noSelectableItems_saturating() {
        var c = createTestController(Mode.SATURATING);
        when(c.isSelectableItem(0)).thenReturn(false);
        when(c.isSelectableItem(1)).thenReturn(false);
        when(c.isSelectableItem(2)).thenReturn(false);
        c.reset();

        assertTrue(c.isParkedAtSentinel());
        assertFalse(c.selectNextItem());
    }

    @Test
    public void selectPreviousItem_noSelectableItems_saturating() {
        var c = createTestController(Mode.SATURATING);
        when(c.isSelectableItem(0)).thenReturn(false);
        when(c.isSelectableItem(1)).thenReturn(false);
        when(c.isSelectableItem(2)).thenReturn(false);
        c.reset();

        assertTrue(c.isParkedAtSentinel());
        assertFalse(c.selectNextItem());
    }

    @Test
    public void selectionControllerWithNoItems() {
        var c = createTestController(Mode.SATURATING);
        when(c.getItemCount()).thenReturn(0);
        c.reset();

        // Normally, saturating controller should start at valid range, but this is an edge case.
        assertTrue(c.isParkedAtSentinel());
        assertEquals(null, c.getPosition());

        // Simulate we now have an item. This should make the saturating controller immediately jump
        // to the first valid item.
        when(c.getItemCount()).thenReturn(1);
        c.reset();
        assertFalse(c.isParkedAtSentinel());
        assertEquals(Integer.valueOf(0), c.getPosition());

        // Simulate we lost all items. This should make the saturating controller revert to sentnel.
        when(c.getItemCount()).thenReturn(0);
        c.reset();
        assertTrue(c.isParkedAtSentinel());
        assertEquals(null, c.getPosition());
    }

    @Test
    public void reset_saturating() {
        var c = createTestController(Mode.SATURATING);
        c.reset();

        verifyPositionSet(c, 0);

        c.selectNextItem(); // 1
        verifyPositionChanged(c, 0, 1);
        c.reset(); // back to default (0)
        verifyPositionChanged(c, 1, 0);
    }
}
