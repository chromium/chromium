// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.SelectionController.Mode;

import java.util.OptionalInt;

/** Robolectric unit tests for {@link SimpleSelectionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SimpleSelectionControllerUnitTest {
    private static final int MAX_POSITION = 3; // Items 0‒2 inclusive.

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock SimpleSelectionController.OnSelectionChangedListener mListener;

    @Before
    public void setUp() {
        // Allow selection changes to succeed unless explicitly overridden.
        when(mListener.onSelectionChanged(anyInt(), eq(true))).thenReturn(true);
        when(mListener.onSelectionChanged(anyInt(), eq(false))).thenReturn(true);
    }

    private void verifyPositionReset(SimpleSelectionController c, int position) {
        verify(mListener).onSelectionChanged(position, false);
        assertEquals(OptionalInt.empty(), c.getPosition());
        assertTrue(c.isParkedAtSentinel());
        clearInvocations(mListener);
    }

    private void verifyPositionSet(SelectionController c, int position) {
        verify(mListener).onSelectionChanged(position, true);
        assertEquals(OptionalInt.of(position), c.getPosition());
        assertFalse(c.isParkedAtSentinel());
        clearInvocations(mListener);
    }

    private void verifyPositionChanged(SelectionController c, int from, int to) {
        verify(mListener).onSelectionChanged(from, false);
        verifyPositionSet(c, to);
    }

    @Test
    public void advanceForward_saturating() {
        SimpleSelectionController c =
                new SimpleSelectionController(mListener, MAX_POSITION, Mode.SATURATING);
        verifyPositionSet(c, 0);

        assertTrue(c.advanceForward());
        verifyPositionChanged(c, 0, 1);

        assertTrue(c.advanceForward());
        verifyPositionChanged(c, 1, 2);

        assertTrue(c.advanceForward());
        verifyPositionChanged(c, 2, 2);

        assertTrue(c.advanceForward());
        verifyPositionChanged(c, 2, 2);
    }

    @Test
    public void advanceForward_saturatingWithSentinel() {
        SimpleSelectionController c =
                new SimpleSelectionController(
                        mListener, MAX_POSITION, Mode.SATURATING_WITH_SENTINEL);
        assertTrue(c.isParkedAtSentinel());

        assertTrue(c.advanceForward());
        verifyPositionSet(c, 0);

        assertTrue(c.advanceForward());
        verifyPositionChanged(c, 0, 1);

        assertTrue(c.advanceForward());
        verifyPositionChanged(c, 1, 2);

        assertFalse(c.advanceForward());
        verifyPositionReset(c, 2);

        assertFalse(c.advanceForward());
        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void advanceBack_saturating() {
        SimpleSelectionController c =
                new SimpleSelectionController(mListener, MAX_POSITION, Mode.SATURATING);
        c.setPosition(MAX_POSITION);
        verifyPositionChanged(c, 0, 2);

        assertTrue(c.advanceBack());
        verifyPositionChanged(c, 2, 1);

        assertTrue(c.advanceBack());
        verifyPositionChanged(c, 1, 0);

        assertTrue(c.advanceBack());
        verifyPositionChanged(c, 0, 0);

        assertTrue(c.advanceBack());
        verifyPositionChanged(c, 0, 0);
    }

    @Test
    public void advanceBack_saturatingWithSentinel() {
        SimpleSelectionController c =
                new SimpleSelectionController(
                        mListener, MAX_POSITION, Mode.SATURATING_WITH_SENTINEL);
        c.setPosition(MAX_POSITION - 1);
        verifyPositionSet(c, 2);

        assertTrue(c.advanceBack());
        verifyPositionChanged(c, 2, 1);

        assertTrue(c.advanceBack());
        verifyPositionChanged(c, 1, 0);

        assertFalse(c.advanceBack());
        verifyPositionReset(c, 0);

        assertFalse(c.advanceBack());
        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void advanceForward_saturating_listenerReturnsFalse() {
        when(mListener.onSelectionChanged(1, true)).thenReturn(false);

        SimpleSelectionController c =
                new SimpleSelectionController(mListener, MAX_POSITION, Mode.SATURATING);
        verifyPositionSet(c, 0);
        assertFalse(c.advanceForward());
        verifyPositionChanged(c, 0, 0);
    }

    @Test
    public void setItemCount() {
        SimpleSelectionController c =
                new SimpleSelectionController(mListener, MAX_POSITION, Mode.SATURATING);
        verifyPositionSet(c, 0);

        // Grow list of items
        c.setItemCount(5);
        verifyPositionSet(c, 0);

        assertTrue(c.advanceForward()); // Should now reach index 4 without saturating
        verifyPositionChanged(c, 0, 1);
        assertTrue(c.advanceForward()); // 2
        verifyPositionChanged(c, 1, 2);
        assertTrue(c.advanceForward()); // 3
        verifyPositionChanged(c, 2, 3);
        assertTrue(c.advanceForward()); // 4
        verifyPositionChanged(c, 3, 4);

        // Shrink list of items
        c.setItemCount(2);
        verifyPositionSet(c, 1);
    }

    @Test
    public void selectionControllerWithNoItems() {
        SimpleSelectionController c = new SimpleSelectionController(mListener, 0, Mode.SATURATING);
        // Normally, saturating controller should start at valid range, but this is an edge case.
        assertTrue(c.isParkedAtSentinel());
        assertEquals(OptionalInt.empty(), c.getPosition());

        // Simulate we now have an item. This should make the saturating controller immediately jump
        // to the first valid item.
        c.setItemCount(1);
        assertFalse(c.isParkedAtSentinel());
        assertEquals(OptionalInt.of(0), c.getPosition());

        // Simulate we lost all items. This should make the saturating controller revert to sentnel.
        c.setItemCount(0);
        assertTrue(c.isParkedAtSentinel());
        assertEquals(OptionalInt.empty(), c.getPosition());
    }

    @Test
    public void reset_saturating() {
        SimpleSelectionController c =
                new SimpleSelectionController(mListener, MAX_POSITION, Mode.SATURATING);
        verifyPositionSet(c, 0);

        c.advanceForward(); // 1
        verifyPositionChanged(c, 0, 1);
        c.advanceForward(); // 2
        verifyPositionChanged(c, 1, 2);
        c.reset(); // back to default (0)
        verifyPositionChanged(c, 2, 0);
    }
}
