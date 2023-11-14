// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Tests for {@link SuggestionLayout}.
 *
 * <p>Note: SuggestionLayout is a the layout class used by BaseSuggestionView. Logic is tested by
 * both BaseSuggestionViewUnitTest and this file.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionLayoutUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule TestRule mProcessor = new Features.JUnitProcessor();

    private Context mContext = ContextUtils.getApplicationContext();
    private @Spy SuggestionLayout mLayout = new SuggestionLayout(mContext);

    @Test
    public void setRoundingEdges_redrawViewOnChange() {
        // This test verifies whether View properly rounds its edges and that it redraws itself when
        // the new configuration differs from the old one.

        // By default, no edges are rounded.
        assertFalse(
                "Unexpected default value of the top edge rounding",
                mLayout.mOutlineProvider.isTopEdgeRounded());
        assertFalse(
                "Unexpected default value of the bottom edge rounding",
                mLayout.mOutlineProvider.isBottomEdgeRounded());

        mLayout.setRoundingEdges(false, false);
        assertFalse(
                "Top edge rounding does not reflect the requested state: false",
                mLayout.mOutlineProvider.isTopEdgeRounded());
        assertFalse(
                "Bottom edge rounding does not reflect the requested state: false",
                mLayout.mOutlineProvider.isBottomEdgeRounded());
        // No invalidate calls, because nothing has changed.
        verify(mLayout, times(0)).invalidateOutline();

        // Enable rounding of bottom corners only. Observe redraw.
        mLayout.setRoundingEdges(false, true);
        assertFalse(
                "Top edge rounding does not reflect the requested state: false",
                mLayout.mOutlineProvider.isTopEdgeRounded());
        assertTrue(
                "Bottom edge rounding does not reflect the requested state: true",
                mLayout.mOutlineProvider.isBottomEdgeRounded());
        verify(mLayout, times(1)).invalidateOutline();
        clearInvocations(mLayout);

        // Apply the same configuration as previously. Observe no redraw.
        mLayout.setRoundingEdges(false, true);
        assertFalse(
                "Top edge rounding does not reflect the requested state: false",
                mLayout.mOutlineProvider.isTopEdgeRounded());
        assertTrue(
                "Bottom edge rounding does not reflect the requested state: true",
                mLayout.mOutlineProvider.isBottomEdgeRounded());
        verify(mLayout, times(0)).invalidateOutline();

        // Enable rounding of all corners. Observe redraw.
        mLayout.setRoundingEdges(true, true);
        assertTrue(
                "Top edge rounding does not reflect the requested state: true",
                mLayout.mOutlineProvider.isTopEdgeRounded());
        assertTrue(
                "Bottom edge rounding does not reflect the requested state: true",
                mLayout.mOutlineProvider.isBottomEdgeRounded());
        verify(mLayout, times(1)).invalidateOutline();
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
    @DisableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    @Config(qualifiers = "sw600dp")
    public void suggestionPadding_modernUiDisabled() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);

        int startSpace =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_start_padding);
        int endSpace =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_end_padding);
        assertEquals(startSpace, mLayout.getPaddingStart());
        assertEquals(endSpace, mLayout.getPaddingEnd());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    @Config(qualifiers = "sw600dp")
    public void suggestionPadding_modernUiEnabled() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);

        // Re-create layout with new feature flags and overrides.
        mLayout = new SuggestionLayout(mContext);

        int endSpace =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_end_padding_modern);
        int inflateSize =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_dropdown_side_spacing);

        // We currently make corrections of the same size as the size by which the dropdown is
        // inflated.
        assertEquals(endSpace, inflateSize);
        assertEquals(0, mLayout.getPaddingStart());
        assertEquals(endSpace, mLayout.getPaddingEnd());
    }
}
