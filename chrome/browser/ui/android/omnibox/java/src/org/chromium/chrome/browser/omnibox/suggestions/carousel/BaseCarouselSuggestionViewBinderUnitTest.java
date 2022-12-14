// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView.RecycledViewPool;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.FormFactor;
import org.chromium.chrome.test.util.browser.Features;
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
@Config(manifest = Config.NONE)
public class BaseCarouselSuggestionViewBinderUnitTest {
    static final int SUGGESTION_VERTICAL_PADDING = 123;
    static final int SUGGESTION_SMALL_BOTTOM_PADDING = 31;

    public @Rule TestRule mFeatures = new Features.JUnitProcessor();

    @Mock
    BaseCarouselSuggestionView mView;

    @Mock
    TextView mHeaderTextView;

    @Mock
    View mHeaderView;

    @Mock
    View mItemView;

    @Mock
    SimpleRecyclerViewAdapter mAdapter;

    @Mock
    Resources mResources;

    private ModelList mTiles;
    private PropertyModel mModel;
    private Configuration mConfiguration;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = new PropertyModel(BaseCarouselSuggestionViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, BaseCarouselSuggestionViewBinder::bind);

        mTiles = new ModelList();
        mConfiguration = new Configuration();
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        mContext = ContextUtils.getApplicationContext();

        when(mView.getHeaderTextView()).thenReturn(mHeaderTextView);
        when(mView.getHeaderView()).thenReturn(mHeaderView);
        when(mView.getAdapter()).thenReturn(mAdapter);
        when(mAdapter.getModelList()).thenReturn(mTiles);
        when(mView.getResources()).thenReturn(mResources);

        when(mResources.getDimensionPixelSize(eq(R.dimen.omnibox_carousel_suggestion_padding)))
                .thenReturn(SUGGESTION_VERTICAL_PADDING);
        when(mResources.getDimensionPixelSize(
                     eq(R.dimen.omnibox_carousel_suggestion_small_bottom_padding)))
                .thenReturn(SUGGESTION_SMALL_BOTTOM_PADDING);
        when(mResources.getConfiguration()).thenReturn(mConfiguration);
        when(mView.getContext()).thenReturn(mContext);
    }

    @Test
    public void headerTitle_set() {
        mModel.set(BaseCarouselSuggestionViewProperties.TITLE, "title");
        verify(mHeaderTextView, times(1)).setText(eq("title"));
        verifyNoMoreInteractions(mHeaderTextView);
    }

    @Test
    public void headerTitle_updateToSameIsNoOp() {
        mModel.set(BaseCarouselSuggestionViewProperties.TITLE, "title");
        reset(mHeaderTextView);
        mModel.set(BaseCarouselSuggestionViewProperties.TITLE, "title");
        verifyNoMoreInteractions(mHeaderTextView);
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
    public void headerTitle_visibilityChangeAltersTopPadding() {
        mModel.set(BaseCarouselSuggestionViewProperties.SHOW_TITLE, true);
        verify(mHeaderView, times(1)).setVisibility(eq(View.VISIBLE));
        verify(mHeaderView, times(1)).setVisibility(anyInt());
        verify(mView, times(1))
                .setPaddingRelative(eq(0), eq(0), eq(0), eq(SUGGESTION_VERTICAL_PADDING));
        verify(mView, times(1)).setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());

        mModel.set(BaseCarouselSuggestionViewProperties.SHOW_TITLE, false);
        verify(mHeaderView, times(1)).setVisibility(eq(View.GONE));
        verify(mHeaderView, times(2)).setVisibility(anyInt());
        verify(mView, times(1))
                .setPaddingRelative(eq(0), eq(SUGGESTION_VERTICAL_PADDING), eq(0),
                        eq(SUGGESTION_VERTICAL_PADDING));
        verify(mView, times(2)).setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    public void headerTitle_visibilityChangeAltersTopPadding_smallBottomPadding() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN.setForTesting(true);
        mModel.set(BaseCarouselSuggestionViewProperties.SHOW_TITLE, true);
        verify(mHeaderView, times(1)).setVisibility(eq(View.VISIBLE));
        verify(mHeaderView, times(1)).setVisibility(anyInt());
        verify(mView, times(1))
                .setPaddingRelative(eq(0), eq(0), eq(0), eq(SUGGESTION_SMALL_BOTTOM_PADDING));
        verify(mView, times(1)).setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());

        mModel.set(BaseCarouselSuggestionViewProperties.SHOW_TITLE, false);
        verify(mHeaderView, times(1)).setVisibility(eq(View.GONE));
        verify(mHeaderView, times(2)).setVisibility(anyInt());
        verify(mView, times(1))
                .setPaddingRelative(eq(0), eq(SUGGESTION_VERTICAL_PADDING), eq(0),
                        eq(SUGGESTION_SMALL_BOTTOM_PADDING));
        verify(mView, times(2)).setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());
    }

    /**
     * We expect value to be computed as the tile margin value computed is larger than
     * tile_view_padding
     */
    @Test
    public void formFactor_itemSpacingPhone_computedPortrait() {
        int displayWidth = 1440;
        int tileViewPaddingEdgePortrait = 12;
        int tileViewwidth = 280;

        when(mResources.getDimensionPixelSize(eq(R.dimen.tile_view_padding_edge_portrait)))
                .thenReturn(tileViewPaddingEdgePortrait);
        when(mResources.getDimensionPixelOffset(eq(R.dimen.tile_view_width)))
                .thenReturn(tileViewwidth);

        DisplayMetrics displayMetrics = new DisplayMetrics();
        displayMetrics.widthPixels = displayWidth;
        when(mResources.getDisplayMetrics()).thenReturn(displayMetrics);

        final int expectedSpacingPx =
                (int) ((displayWidth - tileViewPaddingEdgePortrait - tileViewwidth * 4.5) / 4);
        Assert.assertEquals(expectedSpacingPx,
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(FormFactor.PHONE, mResources));
    }

    @Test
    public void formFactor_itemSpacingTabletPortrait() {
        final int paddingPx = 100;
        when(mResources.getDimensionPixelSize(eq(R.dimen.tile_view_padding_edge_portrait)))
                .thenReturn(paddingPx);
        Assert.assertEquals(paddingPx,
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(FormFactor.TABLET, mResources));
    }

    @Test
    public void formFactor_itemSpacingEndToEnd() {
        final int spacingPx = 100;
        when(mResources.getDimensionPixelSize(eq(R.dimen.tile_view_padding_edge_portrait)))
                .thenReturn(spacingPx);
        Assert.assertEquals(spacingPx,
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(FormFactor.TABLET, mResources));
        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.TABLET);
        verify(mView, times(1)).setItemSpacingPx(eq(spacingPx));
    }

    @Test
    public void formFactor_itemSpacingPhone_landscape() {
        int landscapePadding = 123456789;

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;

        // Ignore all other dimensions. Allow the logic to return garbage (or crash) if expectation
        // is not met.
        when(mResources.getDimensionPixelOffset(eq(R.dimen.tile_view_padding_landscape)))
                .thenReturn(landscapePadding);

        Assert.assertEquals(landscapePadding,
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(FormFactor.PHONE, mResources));
    }

    @Test
    public void formFactor_itemSpacingTablet_landscape() {
        int landscapePadding = 123456789;

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;

        // Ignore all other dimensions. Allow the logic to return garbage (or crash) if expectation
        // is not met.
        when(mResources.getDimensionPixelOffset(eq(R.dimen.tile_view_padding_landscape)))
                .thenReturn(landscapePadding);

        Assert.assertEquals(landscapePadding,
                BaseCarouselSuggestionViewBinder.getItemSpacingPx(FormFactor.TABLET, mResources));
    }

    @Test
    public void mView_setHorizontalFadingEdgeEnabled() {
        mModel.set(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE, true);
        verify(mView, times(1)).setCarouselHorizontalFade(true);

        mModel.set(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE, false);
        verify(mView, times(1)).setCarouselHorizontalFade(false);
    }

    @Test
    public void recyclerView_setCarouselRecycledViewPool() {
        RecycledViewPool testRecycledViewPool = new RecycledViewPool();

        mModel.set(BaseCarouselSuggestionViewProperties.RECYCLED_VIEW_POOL, testRecycledViewPool);
        verify(mView, times(1)).setCarouselRecycledViewPool(testRecycledViewPool);

        mModel.set(BaseCarouselSuggestionViewProperties.RECYCLED_VIEW_POOL, testRecycledViewPool);
        verify(mView, times(1)).setCarouselRecycledViewPool(testRecycledViewPool);
    }
}
