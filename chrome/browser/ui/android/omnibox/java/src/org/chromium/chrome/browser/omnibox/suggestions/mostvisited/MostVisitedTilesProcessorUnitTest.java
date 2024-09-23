// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;

import org.junit.After;
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
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionItemViewBuilder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.browser_ui.widget.tile.TileViewProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.Set;

/** Tests for {@link MostVisitedTilesProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class MostVisitedTilesProcessorUnitTest {
    private static final GURL NAV_URL = JUnitTestGURLs.URL_1;
    private static final GURL NAV_URL_2 = JUnitTestGURLs.URL_2;
    private static final GURL SEARCH_URL = JUnitTestGURLs.SEARCH_URL;

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private PropertyModel mPropertyModel;
    private MostVisitedTilesProcessor mProcessor;
    private List<AutocompleteMatch> mMatches;

    private ArgumentCaptor<Callback<Bitmap>> mFavIconCallbackCaptor =
            ArgumentCaptor.forClass(Callback.class);
    private ArgumentCaptor<Callback<Bitmap>> mGenIconCallbackCaptor =
            ArgumentCaptor.forClass(Callback.class);
    private @Mock Bitmap mFaviconBitmap;
    private @Mock SuggestionHost mSuggestionHost;
    private @Mock OmniboxImageSupplier mImageSupplier;

    static class TileData {
        public final String title;
        public final GURL url;
        public final boolean isSearch;

        public TileData(String title, GURL url, boolean isSearch) {
            this.title = title;
            this.url = url;
            this.isSearch = isSearch;
        }
    }

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        lenient()
                .doNothing()
                .when(mImageSupplier)
                .fetchFavicon(any(), mFavIconCallbackCaptor.capture());
        lenient()
                .doNothing()
                .when(mImageSupplier)
                .generateFavicon(any(), mGenIconCallbackCaptor.capture());

        mProcessor =
                new MostVisitedTilesProcessor(
                        mContext, mSuggestionHost, Optional.of(mImageSupplier));
        OmniboxResourceProvider.disableCachesForTesting();
    }

    @After
    public void tearDown() {
        OmniboxResourceProvider.reenableCachesForTesting();
    }

    /**
     * @param tiles List of tiles that should be presented to the Processor
     * @return Collection of ListItems describing type and properties of each TileView.
     */
    private List<ListItem> populateMatchesForHorizontalRenderGroup(
            int placement, TileData... tiles) {
        mPropertyModel = mProcessor.createModel();
        mMatches = new ArrayList<>();
        for (int index = 0; index < tiles.length; index++) {
            var tile = tiles[index];
            var match =
                    new AutocompleteMatchBuilder(
                                    tile.isSearch
                                            ? OmniboxSuggestionType.TILE_REPEATABLE_QUERY
                                            : OmniboxSuggestionType.TILE_MOST_VISITED_SITE)
                            .setIsSearch(tile.isSearch)
                            .setDisplayText(tile.title)
                            .setUrl(tile.url)
                            .build();
            mProcessor.populateModel(match, mPropertyModel, placement);
            mMatches.add(match);
        }

        var resultingTiles = mPropertyModel.get(BaseCarouselSuggestionViewProperties.TILES);
        assertEquals(tiles.length, resultingTiles.size());
        return resultingTiles;
    }

    @Test
    public void createModel_padding() {
        var model = mProcessor.createModel();

        assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_carousel_suggestion_padding_smaller),
                model.get(BaseCarouselSuggestionViewProperties.TOP_PADDING));
        assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_carousel_suggestion_padding),
                model.get(BaseCarouselSuggestionViewProperties.BOTTOM_PADDING));
    }

    @Test
    public void createModel_carouselBackground() {
        var model = mProcessor.createModel();

        assertFalse(model.get(BaseCarouselSuggestionViewProperties.APPLY_BACKGROUND));
    }

    @Test
    public void populateModel_searchTile() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(0, new TileData("title", SEARCH_URL, true));
        verifyNoMoreInteractions(mImageSupplier);

        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        assertEquals("title", tileModel.get(TileViewProperties.TITLE));
        assertEquals(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileItem.type);
        assertEquals(
                R.drawable.ic_suggestion_magnifier,
                shadowOf(tileModel.get(TileViewProperties.ICON)).getCreatedFromResId());
    }

    @Test
    public void populateModel_navTileIcon_fallbackIcon() {
        mProcessor =
                new MostVisitedTilesProcessor(
                        mContext, mSuggestionHost, /* imageSupplier= */ Optional.empty());
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(0, new TileData("title", NAV_URL, false));

        verifyNoMoreInteractions(mImageSupplier);
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        Drawable drawable = tileModel.get(TileViewProperties.ICON);
        assertEquals(R.drawable.ic_globe_24dp, shadowOf(drawable).getCreatedFromResId());
    }

    @Test
    public void populateModel_navTileIcon_favIcon() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(0, new TileData("title", NAV_URL, false));

        verify(mImageSupplier, times(1)).fetchFavicon(eq(NAV_URL), any());
        mFavIconCallbackCaptor.getValue().onResult(mFaviconBitmap);
        verifyNoMoreInteractions(mImageSupplier);

        // Since we "retrieved" an icon from LargeIconBridge, we should not generate a fallback.
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        Drawable drawable = tileModel.get(TileViewProperties.ICON);
        assertEquals(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileItem.type);
        assertThat(drawable, instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
        assertEquals(mFaviconBitmap, bitmap);
    }

    @Test
    public void populateModel_navTileIcon_generatedBitmap() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(0, new TileData("title", NAV_URL, false));

        verify(mImageSupplier).fetchFavicon(eq(NAV_URL), any());
        mFavIconCallbackCaptor.getValue().onResult(null);

        // We should now observe a request to generate bitmap.
        verify(mImageSupplier).generateFavicon(eq(NAV_URL), mFavIconCallbackCaptor.capture());
        mFavIconCallbackCaptor.getValue().onResult(mFaviconBitmap);
        verifyNoMoreInteractions(mImageSupplier);

        // Since we "retrieved" an icon from LargeIconBridge, we should not generate a fallback.
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        Drawable drawable = tileModel.get(TileViewProperties.ICON);
        assertEquals(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileItem.type);
        assertThat(drawable, instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
        assertEquals(mFaviconBitmap, bitmap);
    }

    @Test
    public void populateModel_navTileIcon_fallbackIconUsedWhenGeneratedBitmapFails() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(0, new TileData("title", NAV_URL, false));

        // Fail to retrieve a real favicon.
        verify(mImageSupplier).fetchFavicon(eq(NAV_URL), any());
        mFavIconCallbackCaptor.getValue().onResult(null);

        // We should now observe a request to generate bitmap. Return null.
        verify(mImageSupplier).generateFavicon(eq(NAV_URL), mFavIconCallbackCaptor.capture());
        mFavIconCallbackCaptor.getValue().onResult(null);
        verifyNoMoreInteractions(mImageSupplier);

        // Since we failed all retrieve attempts, we should keep using fallback icons.
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        Drawable drawable = tileModel.get(TileViewProperties.ICON);
        assertEquals(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileItem.type);
        assertEquals(R.drawable.ic_globe_24dp, shadowOf(drawable).getCreatedFromResId());
    }

    @Test
    public void populateModel_navTileTitle_withMatchDescription() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(0, new TileData("title", NAV_URL, false));
        assertEquals("title", tileList.get(0).model.get(TileViewProperties.TITLE));

        tileList =
                populateMatchesForHorizontalRenderGroup(0, new TileData("title", NAV_URL, false));
        assertEquals("title", tileList.get(0).model.get(TileViewProperties.TITLE));
    }

    @Test
    public void populateModel_navTileTitle_withoutMatchDescriptionUsesHostName() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(0, new TileData("", NAV_URL, false));
        assertEquals(NAV_URL.getHost(), tileList.get(0).model.get(TileViewProperties.TITLE));

        tileList = populateMatchesForHorizontalRenderGroup(0, new TileData("", NAV_URL, false));
        assertEquals(NAV_URL.getHost(), tileList.get(0).model.get(TileViewProperties.TITLE));
    }

    @Test
    public void testInteractions_onClick() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(
                        3,
                        new TileData("search1", SEARCH_URL, true),
                        new TileData("nav1", NAV_URL, false),
                        new TileData("nav2", NAV_URL_2, false));

        InOrder ordered = inOrder(mSuggestionHost);

        // Simulate tile clicks.
        // Note that the value being passed to the suggestion host denotes position of the Carousel
        // on the list, rather than placement of the tile.
        tileList.get(1).model.get(TileViewProperties.ON_CLICK).onClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onSuggestionClicked(eq(mMatches.get(1)), eq(3), eq(NAV_URL));

        tileList.get(2).model.get(TileViewProperties.ON_CLICK).onClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onSuggestionClicked(eq(mMatches.get(2)), eq(3), eq(NAV_URL_2));

        tileList.get(0).model.get(TileViewProperties.ON_CLICK).onClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onSuggestionClicked(eq(mMatches.get(0)), eq(3), eq(SEARCH_URL));

        verifyNoMoreInteractions(mSuggestionHost);

        // Verify histogram increased for delete attempt.
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileType", SuggestTileType.SEARCH));
        assertEquals(
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileType", SuggestTileType.URL));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileIndex", 0));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileIndex", 1));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileIndex", 2));
    }

    @Test
    public void testInteractions_onClick_horizontalGroup() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(
                        3,
                        new TileData("search1", SEARCH_URL, true),
                        new TileData("nav1", NAV_URL, false),
                        new TileData("nav2", NAV_URL_2, false));

        InOrder ordered = inOrder(mSuggestionHost);

        // Simulate tile clicks.
        // Note that the value being passed to the suggestion host denotes position of the Carousel
        // on the list, rather than placement of the tile.
        tileList.get(1).model.get(TileViewProperties.ON_CLICK).onClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onSuggestionClicked(eq(mMatches.get(1)), eq(3), eq(NAV_URL));

        tileList.get(2).model.get(TileViewProperties.ON_CLICK).onClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onSuggestionClicked(eq(mMatches.get(2)), eq(3), eq(NAV_URL_2));

        tileList.get(0).model.get(TileViewProperties.ON_CLICK).onClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onSuggestionClicked(eq(mMatches.get(0)), eq(3), eq(SEARCH_URL));

        verifyNoMoreInteractions(mSuggestionHost);

        // Verify histogram increased for delete attempt.
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileType", SuggestTileType.SEARCH));
        assertEquals(
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileType", SuggestTileType.URL));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileIndex", 0));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileIndex", 1));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestTiles.SelectedTileIndex", 2));
    }

    @Test
    public void testInteractions_onLongClick() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(
                        1,
                        new TileData("search1", SEARCH_URL, true),
                        new TileData("nav1", NAV_URL, true),
                        new TileData("nav2", NAV_URL_2, true));

        InOrder ordered = inOrder(mSuggestionHost);

        // Simulate tile long-clicks.
        // Note that this passes both placement of the carousel in the list as well as particular
        // element that is getting removed.
        tileList.get(1).model.get(TileViewProperties.ON_LONG_CLICK).onLongClick(null);
        ordered.verify(mSuggestionHost, times(1)).onDeleteMatch(eq(mMatches.get(1)), eq("nav1"));

        tileList.get(2).model.get(TileViewProperties.ON_LONG_CLICK).onLongClick(null);
        ordered.verify(mSuggestionHost, times(1)).onDeleteMatch(eq(mMatches.get(2)), eq("nav2"));

        tileList.get(0).model.get(TileViewProperties.ON_LONG_CLICK).onLongClick(null);
        ordered.verify(mSuggestionHost, times(1)).onDeleteMatch(eq(mMatches.get(0)), eq("search1"));

        verifyNoMoreInteractions(mSuggestionHost);
        verifyNoMoreInteractions(mImageSupplier);
    }

    @Test
    public void testInteractions_onLongClick_horizontalGroup() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(
                        1,
                        new TileData("search1", SEARCH_URL, true),
                        new TileData("nav1", NAV_URL, true),
                        new TileData("nav2", NAV_URL_2, true));

        InOrder ordered = inOrder(mSuggestionHost);

        // Simulate tile long-clicks.
        // Note that this passes both placement of the carousel in the list as well as particular
        // element that is getting removed.
        tileList.get(1).model.get(TileViewProperties.ON_LONG_CLICK).onLongClick(null);
        ordered.verify(mSuggestionHost).onDeleteMatch(eq(mMatches.get(1)), eq("nav1"));

        tileList.get(2).model.get(TileViewProperties.ON_LONG_CLICK).onLongClick(null);
        ordered.verify(mSuggestionHost).onDeleteMatch(eq(mMatches.get(2)), eq("nav2"));

        tileList.get(0).model.get(TileViewProperties.ON_LONG_CLICK).onLongClick(null);
        ordered.verify(mSuggestionHost).onDeleteMatch(eq(mMatches.get(0)), eq("search1"));

        verifyNoMoreInteractions(mSuggestionHost);
        verifyNoMoreInteractions(mImageSupplier);
    }

    @Test
    public void testInteractions_movingFocus() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(
                        1,
                        new TileData("search1", SEARCH_URL, true),
                        new TileData("nav1", NAV_URL, true),
                        new TileData("nav2", NAV_URL_2, true));

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
                populateMatchesForHorizontalRenderGroup(0, new TileData("title", SEARCH_URL, true));

        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        // Expect the search string in announcement.
        String expectedDescription =
                mContext.getString(
                        R.string.accessibility_omnibox_most_visited_tile_search, "title");
        assertEquals(expectedDescription, tileModel.get(TileViewProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testAccessibility_navTile() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(0, new TileData("title", NAV_URL, false));

        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        // Expect the navigation string in announcement.
        String expectedDescription =
                mContext.getString(
                        R.string.accessibility_omnibox_most_visited_tile_navigate,
                        "title",
                        NAV_URL.getHost());
        assertEquals(expectedDescription, tileModel.get(TileViewProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testDescriptionWrapping_singleLine() {
        List<ListItem> tileList =
                populateMatchesForHorizontalRenderGroup(0, new TileData("title", NAV_URL, false));

        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;
        assertEquals(1, tileModel.get(TileViewProperties.TITLE_LINES));
    }

    @Test
    public void doesProcessSuggestion_checkSupportedSuggestionTypes() {
        var supportedTypes =
                Set.of(
                        OmniboxSuggestionType.TILE_MOST_VISITED_SITE,
                        OmniboxSuggestionType.TILE_REPEATABLE_QUERY);

        for (@OmniboxSuggestionType int type = 0; type < OmniboxSuggestionType.NUM_TYPES; type++) {
            var match = AutocompleteMatchBuilder.searchWithType(type).build();
            assertEquals(supportedTypes.contains(type), mProcessor.doesProcessSuggestion(match, 0));
        }
    }

    @Test
    public void getViewTypeId_forFullTestCoverage() {
        assertEquals(OmniboxSuggestionUiType.TILE_NAVSUGGEST, mProcessor.getViewTypeId());
    }

    @Test
    public void getCarouselItemViewHeight() {
        // Consider using TileView directly.
        int baseHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_min_height);

        assertEquals(baseHeight, mProcessor.getCarouselItemViewHeight());
    }

    @Test
    public void createModel_checkContentDescription() {
        populateMatchesForHorizontalRenderGroup(0, new TileData("", SEARCH_URL, true));

        assertEquals(
                mContext.getResources().getString(R.string.accessibility_omnibox_most_visited_list),
                mPropertyModel.get(BaseCarouselSuggestionViewProperties.CONTENT_DESCRIPTION));
    }
}
