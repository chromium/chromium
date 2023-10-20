// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.FormFactor;
import org.chromium.chrome.browser.omnibox.suggestions.base.SpacingRecyclerViewItemDecoration;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link BaseCarouselSuggestionViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BaseCarouselSuggestionViewBinderUnitTest {
    public @Rule TestRule mFeatures = new Features.JUnitProcessor();

    private BaseCarouselSuggestionView mView;
    private Context mContext;
    private Resources mResources;
    private ModelList mTiles;
    private SimpleRecyclerViewAdapter mAdapter;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mResources = mContext.getResources();
    }

    private void createMVCForTest() {
        mTiles = new ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(mTiles);
        mView = spy(new BaseCarouselSuggestionView(mContext, mAdapter));
        mModel = new PropertyModel(BaseCarouselSuggestionViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, BaseCarouselSuggestionViewBinder::bind);
    }

    @Test
    public void modelList_setItems() {
        createMVCForTest();
        final List<ListItem> tiles = new ArrayList<>();
        tiles.add(new ListItem(0, null));
        tiles.add(new ListItem(0, null));
        tiles.add(new ListItem(0, null));

        Assert.assertEquals(0, mTiles.size());
        mModel.set(BaseCarouselSuggestionViewProperties.TILES, tiles);
        Assert.assertEquals(3, mTiles.size());
        Assert.assertEquals(tiles.get(0), mTiles.get(0));
        Assert.assertEquals(tiles.get(1), mTiles.get(1));
        Assert.assertEquals(tiles.get(2), mTiles.get(2));
    }

    @Test
    public void modelList_clearItems() {
        createMVCForTest();
        final List<ListItem> tiles = new ArrayList<>();
        tiles.add(new ListItem(0, null));
        tiles.add(new ListItem(0, null));
        tiles.add(new ListItem(0, null));

        Assert.assertEquals(0, mTiles.size());
        mModel.set(BaseCarouselSuggestionViewProperties.TILES, tiles);
        Assert.assertEquals(3, mTiles.size());
        mModel.set(BaseCarouselSuggestionViewProperties.TILES, null);
        Assert.assertEquals(0, mTiles.size());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    public void padding_smallestMargins() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS.setForTesting(true);
        createMVCForTest();
        Assert.assertEquals(
                mResources.getDimensionPixelSize(
                        R.dimen.omnibox_carousel_suggestion_padding_smaller),
                mView.getPaddingTop());
        Assert.assertEquals(
                mResources.getDimensionPixelSize(R.dimen.omnibox_carousel_suggestion_padding),
                mView.getPaddingBottom());
    }

    /**
     * We expect value to be computed as the tile margin value computed is larger than
     * tile_view_padding
     */
    @Test
    @Config(qualifiers = "sw480dp-port")
    public void formFactor_itemSpacingPhone_computedPortrait_exactFit() {
        int carouselWidth = mResources.getDisplayMetrics().widthPixels;
        int initialPadding = 50;
        int adjustedWidth = carouselWidth - initialPadding;
        int baseSpacing = mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);

        // Adjusted carousel size should be (displayWidth - initialPadding).
        // Let's compute hypothetical tile size which would yield itemSpacing == baseSpacing
        // if we were to show 4.5 of them.
        int totalTileAreaSize = adjustedWidth - (4 * baseSpacing);
        int singleTileSize = (int) (totalTileAreaSize / 4.5);

        // Quickly verify our logic. We should not deviate by more than 4.5 pixels (rounding).
        assertEquals((int) (singleTileSize * 4.5 + baseSpacing * 4), adjustedWidth, 4.5);

        // Verify that logic returns baseSpacing as computed itemSpacing.
        assertEquals(
                baseSpacing,
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(
                        FormFactor.PHONE, singleTileSize, initialPadding, mResources));
    }

    @Test
    @Config(qualifiers = "sw480dp-port")
    public void formFactor_itemSpacingPhone_computedPortrait_tightFit() {
        int carouselWidth = mResources.getDisplayMetrics().widthPixels;
        int initialPadding = 50;
        int adjustedWidth = carouselWidth - initialPadding;
        int baseSpacing = mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);

        // Adjusted carousel size should be (displayWidth - initialPadding).
        // Let's compute hypothetical tile size which would yield itemSpacing < baseSpacing
        // if we were to show 4.5 of them.
        int totalTileAreaSize = adjustedWidth - (4 * baseSpacing);
        int singleTileSize = (int) (totalTileAreaSize / 4.5) + 5;

        // Quickly verify our logic. We should exceed the available space, forcing the algorithm to
        // reduce number of visible items.
        assertTrue((int) (singleTileSize * 4.5 + baseSpacing * 4) > adjustedWidth);
        // Compute expected padding in that case.
        int expectedPadding = (int) (adjustedWidth - 3.5 * singleTileSize) / 3;

        // Verify that logic returns padding for 3.5 tiles.
        assertEquals(
                expectedPadding,
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(
                        FormFactor.PHONE, singleTileSize, initialPadding, mResources));
    }

    @Test
    @Config(qualifiers = "sw480dp-port")
    public void formFactor_itemSpacingPhone_computedPortrait_impossibleFit() {
        int carouselWidth = mResources.getDisplayMetrics().widthPixels;
        // No way to fit in 1.5 tiles on screen.
        int singleTileSize = carouselWidth;
        int baseSpacing = mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);

        assertEquals(
                baseSpacing,
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(
                        FormFactor.PHONE, singleTileSize, 0, mResources));
    }

    @Test
    @Config(qualifiers = "sw600dp-port")
    public void formFactor_itemSpacingTabletPortrait() {
        Assert.assertEquals(
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait),
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(
                        FormFactor.TABLET, Integer.MAX_VALUE, Integer.MAX_VALUE, mResources));
    }

    @Test
    @Config(qualifiers = "sw600dp-port")
    public void formFactor_itemSpacingEndToEnd() {
        createMVCForTest();

        final int spacingPx =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);
        Assert.assertEquals(
                spacingPx,
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(
                        FormFactor.TABLET, Integer.MAX_VALUE, Integer.MAX_VALUE, mResources));

        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.TABLET);
        ArgumentCaptor<SpacingRecyclerViewItemDecoration> captor =
                ArgumentCaptor.forClass(SpacingRecyclerViewItemDecoration.class);
        verify(mView, times(1)).addItemDecoration(captor.capture());
        var decoration = captor.getValue();
        Assert.assertEquals(
                OmniboxResourceProvider.getSideSpacing(mContext), decoration.leadInSpace);
        Assert.assertEquals(spacingPx / 2, decoration.elementSpace);
    }

    @Test
    public void formFactor_itemDecorationsDoNotAggregate() {
        createMVCForTest();
        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.TABLET);
        verify(mView, times(1)).addItemDecoration(any());
        Assert.assertEquals(1, mView.getItemDecorationCount());
        clearInvocations(mView);

        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.PHONE);
        verify(mView, times(1)).addItemDecoration(any());
        Assert.assertEquals(1, mView.getItemDecorationCount());
        clearInvocations(mView);

        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.TABLET);
        verify(mView, times(1)).addItemDecoration(any());
        Assert.assertEquals(1, mView.getItemDecorationCount());
    }

    @Test
    @Config(qualifiers = "land")
    public void formFactor_itemSpacingPhone_landscape() {
        Assert.assertEquals(
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape),
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(
                        FormFactor.PHONE, Integer.MAX_VALUE, Integer.MAX_VALUE, mResources));
    }

    @Test
    @Config(qualifiers = "sw600dp-land")
    public void formFactor_itemSpacingTablet_landscape() {
        Assert.assertEquals(
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape),
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(
                        FormFactor.TABLET, Integer.MAX_VALUE, Integer.MAX_VALUE, mResources));
    }

    @Test
    public void mView_setHorizontalFadingEdgeEnabled() {
        createMVCForTest();
        mModel.set(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE, true);
        Assert.assertTrue(mView.isHorizontalFadingEdgeEnabled());

        mModel.set(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE, false);
        Assert.assertFalse(mView.isHorizontalFadingEdgeEnabled());
    }

    @Test
    @Config(qualifiers = "sw600dp-land")
    public void customVisualAlignment_classicUi() {
        createMVCForTest();
        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.TABLET);
        ArgumentCaptor<SpacingRecyclerViewItemDecoration> captor =
                ArgumentCaptor.forClass(SpacingRecyclerViewItemDecoration.class);
        verify(mView, times(1)).addItemDecoration(captor.capture());
        var decoration = captor.getValue();
        Assert.assertEquals(
                OmniboxResourceProvider.getSideSpacing(mContext), decoration.leadInSpace);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    public void customVisualAlignment_modernUi_regular() {
        runCustomVisualAlignmentTest();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    public void customVisualAlignment_modernUi_smaller() {
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLER_MARGINS.setForTesting(true);
        runCustomVisualAlignmentTest();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    public void customVisualAlignment_modernUi_smallest() {
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS.setForTesting(true);
        runCustomVisualAlignmentTest();
    }

    void runCustomVisualAlignmentTest() {
        createMVCForTest();
        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.TABLET);
        ArgumentCaptor<SpacingRecyclerViewItemDecoration> captor =
                ArgumentCaptor.forClass(SpacingRecyclerViewItemDecoration.class);
        verify(mView, times(1)).addItemDecoration(captor.capture());
        var decoration = captor.getValue();
        Assert.assertEquals(
                OmniboxResourceProvider.getHeaderStartPadding(mContext)
                        - mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_padding),
                decoration.leadInSpace);
    }

    @Test
    public void invalidDeviceFormFactorThrowsException() {
        createMVCForTest();
        Assert.assertThrows(
                AssertionError.class,
                () -> mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, 9));
    }
}
