// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.FormFactor;
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

        mTiles = new ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(mTiles);
        mView = spy(new BaseCarouselSuggestionView(mContext, mAdapter));
        mModel = new PropertyModel(BaseCarouselSuggestionViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, BaseCarouselSuggestionViewBinder::bind);
    }

    @Test
    public void modelList_setItems() {
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
        Assert.assertEquals(
                mResources.getDimensionPixelSize(
                        R.dimen.omnibox_carousel_suggestion_padding_smaller),
                mView.getPaddingTop());
        Assert.assertEquals(
                mResources.getDimensionPixelSize(R.dimen.omnibox_carousel_suggestion_padding),
                mView.getPaddingBottom());
    }

    @Test
    public void formFactor_itemDecorationsDoNotAggregate() {
        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.TABLET);
        Assert.assertEquals(1, mView.getItemDecorationCount());

        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.PHONE);
        Assert.assertEquals(1, mView.getItemDecorationCount());

        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.TABLET);
        Assert.assertEquals(1, mView.getItemDecorationCount());
    }

    @Test
    public void mView_setHorizontalFadingEdgeEnabled() {
        mModel.set(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE, true);
        Assert.assertTrue(mView.isHorizontalFadingEdgeEnabled());

        mModel.set(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE, false);
        Assert.assertFalse(mView.isHorizontalFadingEdgeEnabled());
    }

    @Test
    @Config(qualifiers = "sw600dp-land")
    public void customVisualAlignment_classicUi() {
        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.TABLET);
        var decoration = mView.getItemDecoration();
        Assert.assertEquals(
                OmniboxResourceProvider.getSideSpacing(mContext), decoration.getLeadInSpace());
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
        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.TABLET);
        var decoration = mView.getItemDecoration();
        Assert.assertEquals(
                OmniboxResourceProvider.getHeaderStartPadding(mContext)
                        - mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_padding),
                decoration.getLeadInSpace());
    }

    @Test
    public void itemDecoration_setItemWidth() {
        mModel.set(BaseCarouselSuggestionViewProperties.ITEM_WIDTH, 10);
        Assert.assertEquals(10, mView.getItemDecoration().getItemWidthForTesting());

        mModel.set(BaseCarouselSuggestionViewProperties.ITEM_WIDTH, 30);
        Assert.assertEquals(30, mView.getItemDecoration().getItemWidthForTesting());
    }

    @Test
    public void bindContentDescription_nullDescription() {
        mModel =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(BaseCarouselSuggestionViewProperties.CONTENT_DESCRIPTION, null)
                        .build();
        mView = spy(new BaseCarouselSuggestionView(mContext, mAdapter));
        PropertyModelChangeProcessor.create(mModel, mView, BaseCarouselSuggestionViewBinder::bind);

        verify(mView).setContentDescription(null);
    }

    @Test
    public void bindContentDescription_nonNullDescription() {
        mModel =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(
                                BaseCarouselSuggestionViewProperties.CONTENT_DESCRIPTION,
                                "description")
                        .build();
        mView = spy(new BaseCarouselSuggestionView(mContext, mAdapter));
        PropertyModelChangeProcessor.create(mModel, mView, BaseCarouselSuggestionViewBinder::bind);

        verify(mView).setContentDescription("description");
    }
}
