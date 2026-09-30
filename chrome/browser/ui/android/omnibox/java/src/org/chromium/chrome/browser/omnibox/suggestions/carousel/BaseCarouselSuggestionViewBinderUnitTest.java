// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.ViewGroup.MarginLayoutParams;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.RecyclerViewSelectionController;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SpacingRecyclerViewItemDecoration;
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

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private PropertyModel mPropertyModel;
    @Mock private RecyclerViewSelectionController mSelectionController;
    private BaseCarouselSuggestionView mView;
    private Context mContext;
    private Resources mResources;
    private ModelList mTiles;
    private SimpleRecyclerViewAdapter mAdapter;
    private PropertyModel mModel;
    private BaseCarouselSuggestionViewBinder mBinder;
    private OmniboxResourceProvider mResourceProvider;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mResources = mContext.getResources();

        mResourceProvider = new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);
        mBinder = new BaseCarouselSuggestionViewBinder();
        mTiles = new ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(mTiles);
        mModel =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.RESOURCE_PROVIDER, mResourceProvider)
                        .build();
        mView = new BaseCarouselSuggestionView(mContext, mAdapter);
        mView.setSelectionControllerForTesting(mSelectionController);
        PropertyModelChangeProcessor.create(mModel, mView, mBinder);
    }

    @Test
    public void modelList_setItems() {
        final List<ListItem> tiles = new ArrayList<>();
        tiles.add(new ListItem(0, mPropertyModel));
        tiles.add(new ListItem(0, mPropertyModel));
        tiles.add(new ListItem(0, mPropertyModel));

        assertEquals(0, mTiles.size());
        mModel.set(BaseCarouselSuggestionViewProperties.TILES, tiles);
        assertEquals(3, mTiles.size());
        assertEquals(tiles.get(0), mTiles.get(0));
        assertEquals(tiles.get(1), mTiles.get(1));
        assertEquals(tiles.get(2), mTiles.get(2));
    }

    @Test
    public void modelList_clearItems() {
        final List<ListItem> tiles = new ArrayList<>();
        tiles.add(new ListItem(0, mPropertyModel));
        tiles.add(new ListItem(0, mPropertyModel));
        tiles.add(new ListItem(0, mPropertyModel));

        assertEquals(0, mTiles.size());
        mModel.set(BaseCarouselSuggestionViewProperties.TILES, tiles);
        assertEquals(3, mTiles.size());
        verify(mSelectionController).reset();

        clearInvocations(mSelectionController);

        mModel.set(BaseCarouselSuggestionViewProperties.TILES, null);
        assertEquals(0, mTiles.size());
        verify(mSelectionController).reset();
    }

    @Test
    public void createModel_noPaddingValues() {
        mView.setPaddingRelative(1, 2, 3, 4);
        var model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model, mView, mBinder);

        assertEquals(1, mView.getPaddingStart());
        assertEquals(2, mView.getPaddingTop());
        assertEquals(3, mView.getPaddingEnd());
        assertEquals(4, mView.getPaddingBottom());
    }

    @Test
    public void createModel_specificPaddingValues() {
        var model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(BaseCarouselSuggestionViewProperties.TOP_PADDING, 13)
                        .with(BaseCarouselSuggestionViewProperties.BOTTOM_PADDING, 75)
                        .build();
        PropertyModelChangeProcessor.create(model, mView, mBinder);

        assertEquals(0, mView.getPaddingStart());
        assertEquals(13, mView.getPaddingTop());
        assertEquals(0, mView.getPaddingEnd());
        assertEquals(75, mView.getPaddingBottom());
    }

    @Test
    public void createModel_backgroundDisabled() {
        var layoutParams = new MarginLayoutParams(/* width= */ 0, /* height= */ 0);
        var view = new BaseCarouselSuggestionView(mContext, mAdapter);
        view.setLayoutParams(layoutParams);

        var model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(BaseCarouselSuggestionViewProperties.APPLY_BACKGROUND, false)
                        .build();

        PropertyModelChangeProcessor.create(model, view, mBinder);

        assertEquals(Color.TRANSPARENT, ((ColorDrawable) view.getBackground()).getColor());
        assertNull(view.getOutlineProvider());
        assertFalse(view.getClipToOutline());
        assertSame(layoutParams, view.getLayoutParams());
        assertEquals(0, layoutParams.getMarginStart());
        assertEquals(0, layoutParams.getMarginEnd());
    }

    @Test
    public void createModel_backgroundEnabled_nonIncognito() {
        var layoutParams = new MarginLayoutParams(/* width= */ 0, /* height= */ 0);
        var view = new BaseCarouselSuggestionView(mContext, mAdapter);
        view.setLayoutParams(layoutParams);

        var model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.RESOURCE_PROVIDER, mResourceProvider)
                        .with(BaseCarouselSuggestionViewProperties.APPLY_BACKGROUND, true)
                        .build();

        PropertyModelChangeProcessor.create(model, view, mBinder);

        assertEquals(
                OmniboxResourceProvider.getStandardSuggestionBackgroundColor(
                        mContext, BrandedColorScheme.APP_DEFAULT),
                ((ColorDrawable) view.getBackground()).getColor());
        assertNotNull(view.getOutlineProvider());
        assertTrue(view.getClipToOutline());
        assertSame(layoutParams, view.getLayoutParams());
        assertEquals(mResourceProvider.getSideSpacing(), layoutParams.getMarginStart());
        assertEquals(mResourceProvider.getSideSpacing(), layoutParams.getMarginEnd());
    }

    @Test
    public void createModel_backgroundEnabled_incognito() {
        var layoutParams = new MarginLayoutParams(/* width= */ 0, /* height= */ 0);
        var view = new BaseCarouselSuggestionView(mContext, mAdapter);
        view.setLayoutParams(layoutParams);

        var model =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.RESOURCE_PROVIDER, mResourceProvider)
                        .with(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.INCOGNITO)
                        .with(BaseCarouselSuggestionViewProperties.APPLY_BACKGROUND, true)
                        .build();

        mResourceProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        PropertyModelChangeProcessor.create(model, view, mBinder);

        assertEquals(
                mContext.getColor(R.color.search_suggestion_bg_color_incognito),
                ((ColorDrawable) view.getBackground()).getColor());
        // Same as in the non-incognito variant.
        assertNotNull(view.getOutlineProvider());
        assertTrue(view.getClipToOutline());
        assertSame(layoutParams, view.getLayoutParams());
        assertEquals(mResourceProvider.getSideSpacing(), layoutParams.getMarginStart());
        assertEquals(mResourceProvider.getSideSpacing(), layoutParams.getMarginEnd());
    }

    @Test
    public void itemDecoration_setItemWidth() {
        // View was initially created with no decorations.
        assertEquals(0, mView.getItemDecorationCount());

        // Create a new model with a decoration attached.
        var decoration = new SpacingRecyclerViewItemDecoration(10, 5);
        mModel =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(BaseCarouselSuggestionViewProperties.ITEM_DECORATION, decoration)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, mBinder);

        assertEquals(1, mView.getItemDecorationCount());
        assertSame(decoration, mView.getItemDecorationAt(0));
    }

    @Test
    public void bindContentDescription_nullDescription() {
        mModel =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(BaseCarouselSuggestionViewProperties.CONTENT_DESCRIPTION, null)
                        .build();
        mView = new BaseCarouselSuggestionView(mContext, mAdapter);
        PropertyModelChangeProcessor.create(mModel, mView, mBinder);

        assertNull(mView.getContentDescription());
    }

    @Test
    public void bindContentDescription_nonNullDescription() {
        mModel =
                new PropertyModel.Builder(BaseCarouselSuggestionViewProperties.ALL_KEYS)
                        .with(
                                BaseCarouselSuggestionViewProperties.CONTENT_DESCRIPTION,
                                "description")
                        .build();
        mView = new BaseCarouselSuggestionView(mContext, mAdapter);
        PropertyModelChangeProcessor.create(mModel, mView, mBinder);

        assertEquals("description", mView.getContentDescription());
    }
}
