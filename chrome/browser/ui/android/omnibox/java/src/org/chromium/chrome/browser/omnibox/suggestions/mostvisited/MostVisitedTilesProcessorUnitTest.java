// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionItemViewBuilder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.browser_ui.widget.tile.TileViewProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatch.SuggestTile;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link MostVisitedTilesProcessor}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public final class MostVisitedTilesProcessorUnitTest {
    private static final GURL NAV_URL = JUnitTestGURLs.URL_1;
    private static final GURL NAV_URL_2 = JUnitTestGURLs.URL_2;
    private static final GURL SEARCH_URL = JUnitTestGURLs.SEARCH_URL;

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private PropertyModel mPropertyModel;
    private MostVisitedTilesProcessor mProcessor;
    private AutocompleteMatch mMatch;

    private ArgumentCaptor<Callback<Bitmap>> mFavIconCallbackCaptor =
            ArgumentCaptor.forClass(Callback.class);
    private ArgumentCaptor<Callback<Bitmap>> mGenIconCallbackCaptor =
            ArgumentCaptor.forClass(Callback.class);
    private @Mock Bitmap mFaviconBitmap;
    private @Mock SuggestionHost mSuggestionHost;
    private @Mock OmniboxImageSupplier mImageSupplier;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();

        doNothing().when(mImageSupplier).fetchFavicon(any(), mFavIconCallbackCaptor.capture());
        doNothing().when(mImageSupplier).generateFavicon(any(), mGenIconCallbackCaptor.capture());

        mProcessor = new MostVisitedTilesProcessor(mContext, mSuggestionHost, mImageSupplier);
        mPropertyModel = mProcessor.createModel();
    }

    /**
     * @param tiles List of tiles that should be presented to the Processor
     * @return Collection of ListItems describing type and properties of each TileView.
     */
    private List<ListItem> populateTilePropertiesForTiles(
            int placement, AutocompleteMatch.SuggestTile... tiles) {
        mProcessor.onNativeInitialized();
        mMatch = new AutocompleteMatchBuilder(OmniboxSuggestionType.TILE_NAVSUGGEST)
                         .setSuggestTiles(Arrays.asList(tiles))
                         .build();
        mProcessor.populateModel(mMatch, mPropertyModel, placement);

        return mPropertyModel.get(BaseCarouselSuggestionViewProperties.TILES);
    }

    @Test
    public void testDecorations_searchTile() {
        List<ListItem> tileList =
                populateTilePropertiesForTiles(0, new SuggestTile("title", SEARCH_URL, true));
        verifyNoMoreInteractions(mImageSupplier);

        assertEquals(1, tileList.size());
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        assertEquals("title", tileModel.get(TileViewProperties.TITLE));
        assertEquals(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileItem.type);
        assertEquals(R.drawable.ic_suggestion_magnifier,
                shadowOf(tileModel.get(TileViewProperties.ICON)).getCreatedFromResId());
    }

    @Test
    public void testDecorations_navTile() {
        List<ListItem> tileList =
                populateTilePropertiesForTiles(0, new SuggestTile("title", NAV_URL, false));
        verify(mImageSupplier, times(1)).fetchFavicon(eq(NAV_URL), any());
        mFavIconCallbackCaptor.getValue().onResult(mFaviconBitmap);

        // Since we "retrieved" an icon from LargeIconBridge, we should not generate a fallback.
        assertEquals(1, tileList.size());
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        assertEquals("title", tileModel.get(TileViewProperties.TITLE));
        Drawable drawable = tileModel.get(TileViewProperties.ICON);
        assertEquals(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileItem.type);
        assertThat(drawable, instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
        assertEquals(mFaviconBitmap, bitmap);
    }

    @Test
    public void testDecorations_navTileWithEmptyTitle_navTitleShouldBeUrlHost() {
        List<ListItem> tileList =
                populateTilePropertiesForTiles(0, new SuggestTile("", NAV_URL, false));
        verify(mImageSupplier, times(1)).fetchFavicon(eq(NAV_URL), any());
        mFavIconCallbackCaptor.getValue().onResult(mFaviconBitmap);

        // Since we "retrieved" an icon from LargeIconBridge, we should not generate a fallback.
        assertEquals(1, tileList.size());
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        assertEquals(NAV_URL.getHost(), tileModel.get(TileViewProperties.TITLE));
        Drawable drawable = tileModel.get(TileViewProperties.ICON);
        assertEquals(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileItem.type);
        assertThat(drawable, instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
        assertEquals(mFaviconBitmap, bitmap);
    }

    @Test
    public void testInteractions_onClick() {
        List<ListItem> tileList = populateTilePropertiesForTiles(3,
                new SuggestTile("search1", SEARCH_URL, true),
                new SuggestTile("nav1", NAV_URL, false), new SuggestTile("nav2", NAV_URL_2, false));

        assertEquals(3, tileList.size());

        InOrder ordered = inOrder(mSuggestionHost);

        // Simulate tile clicks.
        // Note that the value being passed to the suggestion host denotes position of the Carousel
        // on the list, rather than placement of the tile.
        tileList.get(1).model.get(TileViewProperties.ON_CLICK).onClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onSuggestionClicked(eq(mMatch), eq(3), eq(NAV_URL));

        tileList.get(2).model.get(TileViewProperties.ON_CLICK).onClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onSuggestionClicked(eq(mMatch), eq(3), eq(NAV_URL_2));

        tileList.get(0).model.get(TileViewProperties.ON_CLICK).onClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onSuggestionClicked(eq(mMatch), eq(3), eq(SEARCH_URL));

        verifyNoMoreInteractions(mSuggestionHost);

        // Verify histogram increased for delete attempt.
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileType", SuggestTileType.SEARCH));
        assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileType", SuggestTileType.URL));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileIndex", 0));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileIndex", 1));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileIndex", 2));
    }

    @Test
    public void testInteractions_onLongClick() {
        List<ListItem> tileList = populateTilePropertiesForTiles(1,
                new SuggestTile("search1", SEARCH_URL, true),
                new SuggestTile("nav1", NAV_URL, true), new SuggestTile("nav2", NAV_URL_2, true));

        assertEquals(3, tileList.size());

        InOrder ordered = inOrder(mSuggestionHost);

        // Simulate tile long-clicks.
        // Note that this passes both placement of the carousel in the list as well as particular
        // element that is getting removed.
        tileList.get(1).model.get(TileViewProperties.ON_LONG_CLICK).onLongClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onDeleteMatchElement(eq(mMatch), eq("nav1"), eq(1));

        tileList.get(2).model.get(TileViewProperties.ON_LONG_CLICK).onLongClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onDeleteMatchElement(eq(mMatch), eq("nav2"), eq(2));

        tileList.get(0).model.get(TileViewProperties.ON_LONG_CLICK).onLongClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onDeleteMatchElement(eq(mMatch), eq("search1"), eq(0));

        verifyNoMoreInteractions(mSuggestionHost);
        verifyNoMoreInteractions(mImageSupplier);
    }

    @Test
    public void testInteractions_movingFocus() {
        List<ListItem> tileList = populateTilePropertiesForTiles(1,
                new SuggestTile("search1", SEARCH_URL, true),
                new SuggestTile("nav1", NAV_URL, true), new SuggestTile("nav2", NAV_URL_2, true));

        assertEquals(3, tileList.size());

        InOrder ordered = inOrder(mSuggestionHost);

        // Simulate navigation between the tiles. Expect the signal to be passed back to the
        // suggestions host, describing what should be shown in the Omnibox.
        tileList.get(1).model.get(TileViewProperties.ON_FOCUS_VIA_SELECTION).run();
        ordered.verify(mSuggestionHost, times(1)).setOmniboxEditingText(eq(NAV_URL.getSpec()));

        tileList.get(2).model.get(TileViewProperties.ON_FOCUS_VIA_SELECTION).run();
        ordered.verify(mSuggestionHost, times(1)).setOmniboxEditingText(eq(NAV_URL_2.getSpec()));

        tileList.get(0).model.get(TileViewProperties.ON_FOCUS_VIA_SELECTION).run();
        ordered.verify(mSuggestionHost, times(1)).setOmniboxEditingText(eq(SEARCH_URL.getSpec()));

        verifyNoMoreInteractions(mSuggestionHost);
        verifyNoMoreInteractions(mImageSupplier);
    }

    @Test
    public void testAccessibility_searchTile() {
        List<ListItem> tileList =
                populateTilePropertiesForTiles(0, new SuggestTile("title", SEARCH_URL, true));

        assertEquals(1, tileList.size());
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        // Expect the search string in announcement.
        String expectedDescription = mContext.getString(
                R.string.accessibility_omnibox_most_visited_tile_search, "title");
        assertEquals(expectedDescription, tileModel.get(TileViewProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testAccessibility_navTile() {
        List<ListItem> tileList =
                populateTilePropertiesForTiles(0, new SuggestTile("title", NAV_URL, false));

        assertEquals(1, tileList.size());
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        // Expect the navigation string in announcement.
        String expectedDescription =
                mContext.getString(R.string.accessibility_omnibox_most_visited_tile_navigate,
                        "title", NAV_URL.getHost());
        assertEquals(expectedDescription, tileModel.get(TileViewProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testDescriptionWrapping_singleLine() {
        mProcessor.onNativeInitialized();
        List<ListItem> tileList =
                populateTilePropertiesForTiles(0, new SuggestTile("title", NAV_URL, false));

        assertEquals(1, tileList.size());
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;
        assertEquals(1, tileModel.get(TileViewProperties.TITLE_LINES));
    }
}
