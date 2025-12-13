// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.SelectionController.Mode;

/** Robolectric unit tests for {@link SimpleSelectionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SimpleSelectionControllerUnitTest {
    private static final int MAX_POSITION = 3; // Items 0‒2 inclusive.

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock SimpleSelectionController.OnSelectionChangedListener mListener;

    private void verifyPositionSet(SelectionController c, int position) {
        verify(mListener).onSelectionChanged(position, true);
        assertEquals(Integer.valueOf(position), c.getPosition());
        assertFalse(c.isParkedAtSentinel());
        clearInvocations(mListener);
    }

    private void verifyPositionChanged(SelectionController c, int from, int to) {
        verify(mListener).onSelectionChanged(from, false);
        verifyPositionSet(c, to);
    }

    @Test
    public void setItemCount() {
        SimpleSelectionController c =
                new SimpleSelectionController(mListener, MAX_POSITION, Mode.SATURATING);
        verifyPositionSet(c, 0);

        // Grow list of items
        c.setItemCount(5);
        verifyPositionSet(c, 0);

        assertTrue(c.selectNextItem()); // Should now reach index 4 without saturating
        verifyPositionChanged(c, 0, 1);
        assertTrue(c.selectNextItem()); // 2
        verifyPositionChanged(c, 1, 2);
        assertTrue(c.selectNextItem()); // 3
        verifyPositionChanged(c, 2, 3);
        assertTrue(c.selectNextItem()); // 4
        verifyPositionChanged(c, 3, 4);

        // Shrink list of items
        c.setItemCount(2);
        verifyPositionSet(c, 1);
    }
}
