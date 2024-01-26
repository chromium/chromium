// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.view.ViewGroup.MarginLayoutParams;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.FormFactor;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
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
    public void createModel_noPaddingValues() {
        var view = mock(BaseCarouselSuggestionView.class);
        var model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model, view, BaseCarouselSuggestionViewBinder::bind);

        verify(view, never()).setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void createModel_specificPaddingValues() {
        var view = mock(BaseCarouselSuggestionView.class);
        var model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(BaseCarouselSuggestionViewProperties.TOP_PADDING, 13)
                        .with(BaseCarouselSuggestionViewProperties.BOTTOM_PADDING, 75)
                        .build();
        PropertyModelChangeProcessor.create(model, view, BaseCarouselSuggestionViewBinder::bind);

        verify(view, atLeastOnce()).setPaddingRelative(0, 13, 0, 75);
    }

    @Test
    public void createModel_backgroundDisabled() {
        var layoutParams = new MarginLayoutParams(/* width= */ 0, /* height= */ 0);
        var view = spy(new BaseCarouselSuggestionView(mContext, mAdapter));
        view.setLayoutParams(layoutParams);
        clearInvocations(view);

        var model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(BaseCarouselSuggestionViewProperties.APPLY_BACKGROUND, false)
                        .build();

        PropertyModelChangeProcessor.create(model, view, BaseCarouselSuggestionViewBinder::bind);

        verify(view).setBackgroundColor(Color.TRANSPARENT);
        verify(view).setOutlineProvider(null);
        verify(view).setClipToOutline(false);
        verify(view).setLayoutParams(layoutParams);
        assertEquals(0, layoutParams.getMarginStart());
        assertEquals(0, layoutParams.getMarginEnd());
    }

    @Test
    public void createModel_backgroundEnabled_nonIncognito() {
        var layoutParams = new MarginLayoutParams(/* width= */ 0, /* height= */ 0);
        var view = spy(new BaseCarouselSuggestionView(mContext, mAdapter));
        view.setLayoutParams(layoutParams);
        clearInvocations(view);

        var model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(BaseCarouselSuggestionViewProperties.APPLY_BACKGROUND, true)
                        .build();

        PropertyModelChangeProcessor.create(model, view, BaseCarouselSuggestionViewBinder::bind);

        verify(view)
                .setBackgroundColor(
                        OmniboxResourceProvider.getStandardSuggestionBackgroundColor(mContext));
        verify(view).setOutlineProvider(notNull());
        verify(view).setClipToOutline(true);
        verify(view).setLayoutParams(layoutParams);
        assertEquals(
                OmniboxResourceProvider.getSideSpacing(mContext), layoutParams.getMarginStart());
        assertEquals(OmniboxResourceProvider.getSideSpacing(mContext), layoutParams.getMarginEnd());
    }

    @Test
    public void createModel_backgroundEnabled_incognito() {
        var layoutParams = new MarginLayoutParams(/* width= */ 0, /* height= */ 0);
        var view = spy(new BaseCarouselSuggestionView(mContext, mAdapter));
        view.setLayoutParams(layoutParams);
        clearInvocations(view);

        var model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.INCOGNITO)
                        .with(BaseCarouselSuggestionViewProperties.APPLY_BACKGROUND, true)
                        .build();

        PropertyModelChangeProcessor.create(model, view, BaseCarouselSuggestionViewBinder::bind);

        verify(view).setBackgroundColor(mContext.getColor(R.color.omnibox_suggestion_bg_incognito));
        // Same as in the non-incognito variant.
        verify(view).setOutlineProvider(notNull());
        verify(view).setClipToOutline(true);
        verify(view).setLayoutParams(layoutParams);
        assertEquals(
                OmniboxResourceProvider.getSideSpacing(mContext), layoutParams.getMarginStart());
        assertEquals(OmniboxResourceProvider.getSideSpacing(mContext), layoutParams.getMarginEnd());
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
