// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tail;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link AlignmentManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AlignmentManagerUnitTest {
    private static final int TEXT_AREA_WIDTH = 100;

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock TailSuggestionView mTailView1;
    private @Mock TailSuggestionView mTailView2;
    private @Mock TailSuggestionView mTailView3;
    private AlignmentManager mManager;

    @Before
    public void setUp() {
        mManager = new AlignmentManager();
        mManager.registerView(mTailView1);
        mManager.registerView(mTailView2);
        mManager.registerView(mTailView3);
    }

    /**
     * Helper method to acquire additional padding value for specific view and text widths.
     *
     * @param view View requesting additional padding data.
     * @param queryWidth length of the tail query displayed in the suggestion.
     * @param fullWidth length of the full query that would be executed.
     */
    private int paddingFor(TailSuggestionView view, int queryWidth, int fullWidth) {
        return mManager.requestStartPadding(view, queryWidth, fullWidth, TEXT_AREA_WIDTH);
    }

    @Test
    public void queryAlignment_noRelayout() {
        final int inputWidth = 11;
        final int query1Width = 55;
        final int query2Width = 66;
        final int query3Width = 77;
        final int fullText1Width = inputWidth + query1Width;
        final int fullText2Width = inputWidth + query2Width;
        final int fullText3Width = inputWidth + query3Width;

        // fullTextWidth < TEXT_AREA_WIDTH, so everything should fit in.
        assertEquals(inputWidth, paddingFor(mTailView1, query1Width, fullText1Width));
        assertEquals(inputWidth, paddingFor(mTailView2, query2Width, fullText2Width));
        assertEquals(inputWidth, paddingFor(mTailView3, query3Width, fullText3Width));

        // Confirm no re-layouts requested.
        verify(mTailView1, times(0)).requestLayout();
        verify(mTailView2, times(0)).requestLayout();
        verify(mTailView3, times(0)).requestLayout();
    }

    @Test
    public void tailAlignment_singleQueryForcesRelayout() {
        final int queryWidth = 55;
        final int fullText1Width = 88;
        final int fullText2Width = 99;
        final int fullText3Width = 111;

        // This alignment will only be returned once the longest query is processed.
        final int expectedTargetAlignment = TEXT_AREA_WIDTH - queryWidth;

        assertEquals(
                fullText1Width - queryWidth, paddingFor(mTailView1, queryWidth, fullText1Width));
        assertEquals(
                fullText2Width - queryWidth, paddingFor(mTailView2, queryWidth, fullText2Width));

        // This is query breaks the alignment and does not fit when aligned to user input.
        // Should cause re-layout of other queries.
        assertEquals(expectedTargetAlignment, paddingFor(mTailView3, queryWidth, fullText3Width));

        // Confirm re-layouts requested everywhere but the view that triggered relayout.
        verify(mTailView1, times(1)).requestLayout();
        verify(mTailView2, times(1)).requestLayout();
        verify(mTailView3, times(0)).requestLayout();

        // Confirm that all views are left-aligned to each other.
        assertEquals(expectedTargetAlignment, paddingFor(mTailView1, queryWidth, fullText1Width));
        assertEquals(expectedTargetAlignment, paddingFor(mTailView2, queryWidth, fullText2Width));
        assertEquals(expectedTargetAlignment, paddingFor(mTailView3, queryWidth, fullText3Width));
    }

    @Test
    public void tailAlignment_multipleQueriesForceRelayout() {
        final int query1Width = 55;
        final int query2Width = 66;
        final int query3Width = 77;
        final int fullText1Width = 90;
        final int fullText2Width = 120;
        final int fullText3Width = 150;

        // First query fits in the target area perfectly (fullText1Width < TEXT_AREA_WIDTH).
        assertEquals(
                fullText1Width - query1Width, paddingFor(mTailView1, query1Width, fullText1Width));

        // Second query does not fit, and is the longest one yet. Should force relayout.
        final int expectedAlignment1 = TEXT_AREA_WIDTH - query2Width;
        assertEquals(expectedAlignment1, paddingFor(mTailView2, query2Width, fullText2Width));
        verify(mTailView1, times(1)).requestLayout();
        verify(mTailView3, times(1)).requestLayout();
        // Confirm that on re-layout, first query gets aligned to the second.
        assertEquals(expectedAlignment1, paddingFor(mTailView1, query1Width, fullText1Width));

        verifyNoMoreInteractions(mTailView1, mTailView2, mTailView3);
        clearInvocations(mTailView1, mTailView2, mTailView3);

        // Third query does not fit, too, and is the next longest query. Should force relayout.
        final int expectedAlignment2 = TEXT_AREA_WIDTH - query3Width;
        assertEquals(expectedAlignment2, paddingFor(mTailView3, query3Width, fullText3Width));
        verify(mTailView1, times(1)).requestLayout();
        verify(mTailView2, times(1)).requestLayout();
        // Confirm that on re-layout, first two queries get aligned to the third.
        assertEquals(expectedAlignment2, paddingFor(mTailView1, query1Width, fullText1Width));
        assertEquals(expectedAlignment2, paddingFor(mTailView1, query2Width, fullText2Width));

        verifyNoMoreInteractions(mTailView1, mTailView2, mTailView3);
    }

    @Test
    public void tailAlignment_onlyLongestQueriesForceRelayout() {
        final int query1Width = 55;
        final int query2Width = 77;
        final int query3Width = 66;
        final int fullText1Width = 90;
        final int fullText2Width = 150;
        final int fullText3Width = 120;

        // First query fits in the target area perfectly (fullText1Width < TEXT_AREA_WIDTH).
        assertEquals(
                fullText1Width - query1Width, paddingFor(mTailView1, query1Width, fullText1Width));

        // Second query does not fit, and is the longest one here. Should force relayout.
        final int expectedTargetAlignment = TEXT_AREA_WIDTH - query2Width;
        assertEquals(expectedTargetAlignment, paddingFor(mTailView2, query2Width, fullText2Width));
        verify(mTailView1, times(1)).requestLayout();
        verify(mTailView3, times(1)).requestLayout();
        // Confirm that on re-layout, first query gets aligned to the second.
        assertEquals(expectedTargetAlignment, paddingFor(mTailView1, query1Width, fullText1Width));

        // Third query does not fit, too, but is not the longest one. Should not relayout.
        assertEquals(expectedTargetAlignment, paddingFor(mTailView3, query3Width, fullText3Width));
        // Confirm that alignment has not changed for other two queries either.
        assertEquals(expectedTargetAlignment, paddingFor(mTailView1, query1Width, fullText1Width));
        assertEquals(expectedTargetAlignment, paddingFor(mTailView1, query2Width, fullText2Width));

        verifyNoMoreInteractions(mTailView1, mTailView2, mTailView3);
    }
}
