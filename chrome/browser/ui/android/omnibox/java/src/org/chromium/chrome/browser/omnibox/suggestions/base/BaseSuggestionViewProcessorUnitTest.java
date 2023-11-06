// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Typeface;
import android.text.Spannable;
import android.text.style.StyleSpan;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.components.omnibox.AutocompleteMatch.MatchClassification;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link BaseSuggestionViewProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BaseSuggestionViewProcessorUnitTest {
    private static final int FAKE_STRING_LENGTH = 10;

    @Mock Spannable mText;

    private ArgumentMatcher<StyleSpan> mIsHighlightStyle;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mText.length()).thenReturn(FAKE_STRING_LENGTH);

        mIsHighlightStyle = (StyleSpan style) -> style.getStyle() == Typeface.BOLD;
    }

    @Test
    public void highlightTest_noClassifications() {
        assertFalse(BaseSuggestionViewProcessor.applyHighlightToMatchRegions(mText, null));
        verify(mText, times(0)).setSpan(any(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void highlightTest_noMatch() {
        assertFalse(
                BaseSuggestionViewProcessor.applyHighlightToMatchRegions(mText, new ArrayList<>()));
        verify(mText, times(0)).setSpan(any(), anyInt(), anyInt(), anyInt());
    }

    /** Verify string is correctly highlighted when match is the last one on the list. */
    @Test
    public void highlightTest_matchWithNoTerminator() {
        final int matchStart = 4;
        final List<MatchClassification> classifications = new ArrayList<>();

        classifications.add(new MatchClassification(matchStart, MatchClassificationStyle.MATCH));
        assertTrue(
                BaseSuggestionViewProcessor.applyHighlightToMatchRegions(mText, classifications));
        verify(mText, times(1))
                .setSpan(
                        argThat(mIsHighlightStyle),
                        eq(matchStart),
                        eq(FAKE_STRING_LENGTH),
                        eq(Spannable.SPAN_EXCLUSIVE_EXCLUSIVE));

        // Check that the total amount of calls to setSpan.
        verify(mText, times(1)).setSpan(any(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void highlightTest_matchWithTerminator() {
        final int matchStart = 4;
        final int matchEnd = 7;
        final List<MatchClassification> classifications = new ArrayList<>();

        classifications.add(new MatchClassification(matchStart, MatchClassificationStyle.MATCH));
        classifications.add(new MatchClassification(matchEnd, MatchClassificationStyle.NONE));

        assertTrue(
                BaseSuggestionViewProcessor.applyHighlightToMatchRegions(mText, classifications));
        verify(mText, times(1))
                .setSpan(
                        argThat(mIsHighlightStyle),
                        eq(matchStart),
                        eq(matchEnd),
                        eq(Spannable.SPAN_EXCLUSIVE_EXCLUSIVE));

        // Check that the total amount of calls to setSpan.
        verify(mText, times(1)).setSpan(any(), anyInt(), anyInt(), anyInt());
    }

    /** Verify that multiple matches receive proper highlight. */
    @Test
    public void highlightTest_multipleMatches() {
        final int matchStart1 = 4;
        final int matchEnd1 = 5;
        final int matchStart2 = 6;
        final int matchEnd2 = 7;
        final List<MatchClassification> classifications = new ArrayList<>();

        classifications.add(new MatchClassification(matchStart1, MatchClassificationStyle.MATCH));
        classifications.add(new MatchClassification(matchEnd1, MatchClassificationStyle.NONE));
        classifications.add(new MatchClassification(matchStart2, MatchClassificationStyle.MATCH));
        classifications.add(new MatchClassification(matchEnd2, MatchClassificationStyle.NONE));

        assertTrue(
                BaseSuggestionViewProcessor.applyHighlightToMatchRegions(mText, classifications));
        verify(mText, times(1))
                .setSpan(
                        argThat(mIsHighlightStyle),
                        eq(matchStart1),
                        eq(matchEnd1),
                        eq(Spannable.SPAN_EXCLUSIVE_EXCLUSIVE));
        verify(mText, times(1))
                .setSpan(
                        argThat(mIsHighlightStyle),
                        eq(matchStart2),
                        eq(matchEnd2),
                        eq(Spannable.SPAN_EXCLUSIVE_EXCLUSIVE));

        // Check that the total amount of calls to setSpan.
        verify(mText, times(2)).setSpan(any(), anyInt(), anyInt(), anyInt());
    }

    /** Verify that multiple consecutive matches don't overlap in target Span. */
    @Test
    public void highlightTest_overlappingMatches() {
        final int matchStart1 = 4;
        final int matchStart2 = 6;
        final List<MatchClassification> classifications = new ArrayList<>();

        classifications.add(new MatchClassification(matchStart1, MatchClassificationStyle.MATCH));
        classifications.add(new MatchClassification(matchStart2, MatchClassificationStyle.MATCH));

        assertTrue(
                BaseSuggestionViewProcessor.applyHighlightToMatchRegions(mText, classifications));
        verify(mText, times(1))
                .setSpan(
                        argThat(mIsHighlightStyle),
                        eq(matchStart1),
                        eq(matchStart2),
                        eq(Spannable.SPAN_EXCLUSIVE_EXCLUSIVE));
        verify(mText, times(1))
                .setSpan(
                        argThat(mIsHighlightStyle),
                        eq(matchStart2),
                        eq(FAKE_STRING_LENGTH),
                        eq(Spannable.SPAN_EXCLUSIVE_EXCLUSIVE));

        // Check that the total amount of calls to setSpan.
        verify(mText, times(2)).setSpan(any(), anyInt(), anyInt(), anyInt());
    }

    /** Verify that non-matching classifiers don't receive highlight. */
    @Test
    public void highlightTest_nonMatchingClassifiers() {
        final List<MatchClassification> classifications = new ArrayList<>();

        classifications.add(new MatchClassification(0, MatchClassificationStyle.NONE));
        classifications.add(new MatchClassification(1, MatchClassificationStyle.URL));
        classifications.add(new MatchClassification(2, MatchClassificationStyle.DIM));

        assertFalse(
                BaseSuggestionViewProcessor.applyHighlightToMatchRegions(mText, classifications));
        verify(mText, times(0)).setSpan(any(), anyInt(), anyInt(), anyInt());
    }
}
