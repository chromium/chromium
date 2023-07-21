// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;

import androidx.recyclerview.widget.RecyclerView.RecycledViewPool;

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

/**
 * Tests for {@link BaseCarouselSuggestionViewBinder}.
 */
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
    @Features.EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    public void padding_smallBottomPadding() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN.setForTesting(true);
        createMVCForTest();
        Assert.assertEquals(mResources.getDimensionPixelSize(
                                    R.dimen.omnibox_carousel_suggestion_padding_smaller),
                mView.getPaddingTop());
        Assert.assertEquals(mResources.getDimensionPixelSize(
                                    R.dimen.omnibox_carousel_suggestion_small_bottom_padding),
                mView.getPaddingBottom());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    public void padding_smallerMargins() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLER_MARGINS.setForTesting(true);
        createMVCForTest();
        Assert.assertEquals(mResources.getDimensionPixelSize(
                                    R.dimen.omnibox_carousel_suggestion_padding_smallest),
                mView.getPaddingTop());
        Assert.assertEquals(mResources.getDimensionPixelSize(
                                    R.dimen.omnibox_carousel_suggestion_small_bottom_padding),
                mView.getPaddingBottom());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    public void padding_smallestMargins() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS.setForTesting(true);
        createMVCForTest();
        Assert.assertEquals(mResources.getDimensionPixelSize(
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
    public void formFactor_itemSpacingPhone_computedPortrait() {
        int displayWidth = mResources.getDisplayMetrics().widthPixels;
        int tileViewWidth = mResources.getDimensionPixelSize(R.dimen.tile_view_width);
        int tileViewPaddingEdgePortrait =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);
        int tileViewPaddingMax = mResources.getDimensionPixelSize(R.dimen.tile_view_padding);

        final int expectedSpacingPx = Integer.max(-tileViewPaddingMax,
                (int) ((displayWidth - tileViewPaddingEdgePortrait - tileViewWidth * 4.5) / 4));
        Assert.assertEquals(expectedSpacingPx,
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(FormFactor.PHONE, mResources));
    }

    @Test
    @Config(qualifiers = "sw600dp-port")
    public void formFactor_itemSpacingTabletPortrait() {
        Assert.assertEquals(
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait),
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(FormFactor.TABLET, mResources));
    }

    @Test
    @Config(qualifiers = "sw600dp-port")
    public void formFactor_itemSpacingEndToEnd() {
        createMVCForTest();

        final int spacingPx =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);
        Assert.assertEquals(spacingPx,
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(FormFactor.TABLET, mResources));

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
        Assert.assertEquals(mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape),
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(FormFactor.PHONE, mResources));
    }

    @Test
    @Config(qualifiers = "sw600dp-land")
    public void formFactor_itemSpacingTablet_landscape() {
        Assert.assertEquals(mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape),
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(FormFactor.TABLET, mResources));
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
    public void recyclerView_setRecycledViewPool() {
        createMVCForTest();
        RecycledViewPool testRecycledViewPool = new RecycledViewPool();
        mModel.set(BaseCarouselSuggestionViewProperties.RECYCLED_VIEW_POOL, testRecycledViewPool);
        Assert.assertEquals(testRecycledViewPool, mView.getRecycledViewPool());
    }

    @Test
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
        Assert.assertEquals(OmniboxResourceProvider.getHeaderStartPadding(mContext)
                        - mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_padding),
                decoration.leadInSpace);
    }

    @Test
    public void invalidDeviceFormFactorThrowsException() {
        createMVCForTest();
        Assert.assertThrows(AssertionError.class,
                () -> mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, 9));
    }
}
