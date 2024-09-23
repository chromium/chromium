// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionLayout.LayoutParams.SuggestionViewType;
import org.chromium.chrome.browser.omnibox.test.R;

/**
 * Tests for {@link SuggestionLayout}.
 *
 * <p>Note: SuggestionLayout is a the layout class used by BaseSuggestionView. Logic is tested by
 * both BaseSuggestionViewUnitTest and this file.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionLayoutUnitTest {

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext = ContextUtils.getApplicationContext();
    private View mDecorationView = new View(mContext);
    private View mContentView = new View(mContext);
    private SuggestionLayout mLayout = new SuggestionLayout(mContext);

    @Test
    public void setRoundingEdges_redrawViewOnChange() {
        SuggestionLayout spy = spy(mLayout);
        // This test verifies whether View properly rounds its edges and that it redraws itself when
        // the new configuration differs from the old one.

        // By default, no edges are rounded.
        assertFalse(
                "Unexpected default value of the top edge rounding",
                spy.mOutlineProvider.isTopEdgeRounded());
        assertFalse(
                "Unexpected default value of the bottom edge rounding",
                spy.mOutlineProvider.isBottomEdgeRounded());

        spy.setRoundingEdges(false, false);
        assertFalse(
                "Top edge rounding does not reflect the requested state: false",
                spy.mOutlineProvider.isTopEdgeRounded());
        assertFalse(
                "Bottom edge rounding does not reflect the requested state: false",
                spy.mOutlineProvider.isBottomEdgeRounded());
        // No invalidate calls, because nothing has changed.
        verify(spy, times(0)).invalidateOutline();

        // Enable rounding of bottom corners only. Observe redraw.
        spy.setRoundingEdges(false, true);
        assertFalse(
                "Top edge rounding does not reflect the requested state: false",
                spy.mOutlineProvider.isTopEdgeRounded());
        assertTrue(
                "Bottom edge rounding does not reflect the requested state: true",
                spy.mOutlineProvider.isBottomEdgeRounded());
        verify(spy, times(1)).invalidateOutline();
        clearInvocations(spy);

        // Apply the same configuration as previously. Observe no redraw.
        spy.setRoundingEdges(false, true);
        assertFalse(
                "Top edge rounding does not reflect the requested state: false",
                spy.mOutlineProvider.isTopEdgeRounded());
        assertTrue(
                "Bottom edge rounding does not reflect the requested state: true",
                spy.mOutlineProvider.isBottomEdgeRounded());
        verify(spy, times(0)).invalidateOutline();

        // Enable rounding of all corners. Observe redraw.
        spy.setRoundingEdges(true, true);
        assertTrue(
                "Top edge rounding does not reflect the requested state: true",
                spy.mOutlineProvider.isTopEdgeRounded());
        assertTrue(
                "Bottom edge rounding does not reflect the requested state: true",
                spy.mOutlineProvider.isBottomEdgeRounded());
        verify(spy, times(1)).invalidateOutline();
    }

    @Test
    public void setRoundingEdges_clipToOutlineWhenRounding() {
        // By default, all edges are rounded.
        assertFalse(
                "Unexpected default value of the top edge rounding",
                mLayout.mOutlineProvider.isTopEdgeRounded());
        assertFalse(
                "Unexpected default value of the bottom edge rounding",
                mLayout.mOutlineProvider.isBottomEdgeRounded());
        assertFalse(
                "Clipping should be disabled when rounding is not in use",
                mLayout.getClipToOutline());

        // When any of the edges are rounded, we should also enable clipping.
        mLayout.setRoundingEdges(true, false);
        assertTrue(
                "Clipping should be enabled when rounding only top edge corners",
                mLayout.getClipToOutline());
        mLayout.setRoundingEdges(false, true);
        assertTrue(
                "Clipping should be enabled when rounding only bottom edge corners",
                mLayout.getClipToOutline());
        mLayout.setRoundingEdges(true, true);
        assertTrue(
                "Clipping should be enabled when rounding both top and bottom edge corners",
                mLayout.getClipToOutline());

        // Revert back to no rounding. Observe that we're not clipping any longer.
        mLayout.setRoundingEdges(false, false);
        assertFalse(
                "Clipping should be disabled when rounding is not in use",
                mLayout.getClipToOutline());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void suggestionPadding_modernUiEnabled() {
        // Re-create layout with new feature flags and overrides.
        mLayout = new SuggestionLayout(mContext);

        int endSpace =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_end_padding);
        int inflateSize =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_dropdown_side_spacing);

        // We currently make corrections of the same size as the size by which the dropdown is
        // inflated.
        assertEquals(endSpace, inflateSize);
        assertEquals(0, mLayout.getPaddingStart());
        assertEquals(endSpace, mLayout.getPaddingEnd());
    }

    @Test
    public void testUseLargeDecoration() {
        mLayout.addView(
                mContentView,
                SuggestionLayout.LayoutParams.forViewType(SuggestionViewType.CONTENT));
        assertFalse(mLayout.getUseLargeDecoration());

        mLayout.addView(
                mDecorationView,
                SuggestionLayout.LayoutParams.forViewType(SuggestionViewType.DECORATION));
        assertFalse(mLayout.getUseLargeDecoration());

        mDecorationView.setLayoutParams(SuggestionLayout.LayoutParams.forLargeDecorationIcon());
        assertTrue(mLayout.getUseLargeDecoration());
    }

    @Test
    public void testHiddenDecoration() {
        mLayout.addView(
                mDecorationView,
                SuggestionLayout.LayoutParams.forViewType(SuggestionViewType.DECORATION));
        mLayout.addView(
                mContentView,
                SuggestionLayout.LayoutParams.forViewType(SuggestionViewType.CONTENT));
        mLayout.measure(
                MeasureSpec.makeMeasureSpec(200, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(48, MeasureSpec.AT_MOST));
        mLayout.layout(0, 0, 200, 48);

        assertEquals(
                OmniboxResourceProvider.getSuggestionDecorationIconSizeWidth(mContext),
                mContentView.getLeft());

        mDecorationView.setVisibility(View.GONE);

        mLayout.measure(
                MeasureSpec.makeMeasureSpec(200, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(48, MeasureSpec.AT_MOST));
        mLayout.layout(0, 0, 200, 48);
        assertEquals(
                mContext.getResources().getDimensionPixelSize(R.dimen.omnibox_simple_card_leadin),
                mContentView.getLeft());
    }
}
