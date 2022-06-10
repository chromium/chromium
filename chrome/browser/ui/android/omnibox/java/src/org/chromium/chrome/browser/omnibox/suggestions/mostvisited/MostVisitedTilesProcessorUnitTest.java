// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
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
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionItemViewBuilder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.tile.TileViewProperties;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatch.SuggestTile;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
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
@Config(manifest = Config.NONE)
public final class MostVisitedTilesProcessorUnitTest {
    private static final GURL NAV_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
    private static final GURL NAV_URL_2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);
    private static final GURL SEARCH_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL);
    private static final int FALLBACK_COLOR = 0xACE0BA5E;

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private PropertyModel mPropertyModel;
    private MostVisitedTilesProcessor mProcessor;
    private ArgumentCaptor<LargeIconCallback> mIconCallbackCaptor =
            ArgumentCaptor.forClass(LargeIconCallback.class);
    private AutocompleteMatch mMatch;

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock LargeIconBridge mLargeIconBridge;
    private @Mock RoundedIconGenerator mIconGenerator;
    private @Mock Bitmap mGeneratedIconBitmap;
    private @Mock Bitmap mLargeIconBitmap;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mProcessor =
                new MostVisitedTilesProcessor(mActivity, mSuggestionHost, () -> mLargeIconBridge);
        mProcessor.setRoundedIconGeneratorForTesting(mIconGenerator);

        when(mIconGenerator.generateIconForUrl(any(GURL.class))).thenReturn(mGeneratedIconBitmap);
        when(mLargeIconBridge.getLargeIconForUrl(any(), anyInt(), mIconCallbackCaptor.capture()))
                .thenReturn(true);

        mPropertyModel = mProcessor.createModel();
    }

    /**
     * @param tiles List of tiles that should be presented to the Processor
     * @return Collection of ListItems describing type and properties of each TileView.
     */
    private List<ListItem> populateTilePropertiesForTiles(
            int placement, AutocompleteMatch.SuggestTile... tiles) {
        mMatch = new AutocompleteMatchBuilder().setSuggestTiles(Arrays.asList(tiles)).build();
        mProcessor.populateModel(mMatch, mPropertyModel, placement);

        return mPropertyModel.get(BaseCarouselSuggestionViewProperties.TILES);
    }

    @Test
    public void testDecorations_searchTile() {
        List<ListItem> tileList =
                populateTilePropertiesForTiles(0, new SuggestTile("title", SEARCH_URL, true));
        verifyNoMoreInteractions(mIconGenerator);
        verifyNoMoreInteractions(mLargeIconBridge);

        assertEquals(1, tileList.size());
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        assertEquals("title", tileModel.get(TileViewProperties.TITLE));
        assertEquals(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileItem.type);
        assertEquals(R.drawable.ic_suggestion_magnifier,
                shadowOf(tileModel.get(TileViewProperties.ICON)).getCreatedFromResId());
    }

    @Test
    public void testDecorations_navTile_generatedIconOnly() {
        List<ListItem> tileList =
                populateTilePropertiesForTiles(0, new SuggestTile("title", NAV_URL, false));
        verify(mIconGenerator, times(1)).generateIconForUrl(eq(NAV_URL));
        verify(mLargeIconBridge, times(1)).getLargeIconForUrl(eq(NAV_URL), anyInt(), any());

        assertEquals(1, tileList.size());
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        assertEquals("title", tileModel.get(TileViewProperties.TITLE));
        Drawable drawable = tileModel.get(TileViewProperties.ICON);
        assertEquals(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileItem.type);
        assertThat(drawable, instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
        assertEquals(mGeneratedIconBitmap, bitmap);
    }

    @Test
    public void testDecorations_navTile_fallbackColor() {
        List<ListItem> tileList =
                populateTilePropertiesForTiles(0, new SuggestTile("title", NAV_URL, false));
        verify(mIconGenerator, times(1)).generateIconForUrl(eq(NAV_URL));
        verify(mLargeIconBridge, times(1)).getLargeIconForUrl(eq(NAV_URL), anyInt(), any());
        // Report no icon, only color.
        mIconCallbackCaptor.getValue().onLargeIconAvailable(null, FALLBACK_COLOR, true, 0);

        // The logic should ignore the fallback color and focus only on the presence of an
        // actual icon. If no icon is available, we should retain the original generated icon.
        assertEquals(1, tileList.size());
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;
        Drawable drawable = tileModel.get(TileViewProperties.ICON);

        assertEquals(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileItem.type);
        assertThat(drawable, instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
        assertEquals(mGeneratedIconBitmap, bitmap);
    }

    @Test
    public void testDecorations_navTile_actualIcon() {
        List<ListItem> tileList =
                populateTilePropertiesForTiles(0, new SuggestTile("title", NAV_URL, false));
        verify(mIconGenerator, times(1)).generateIconForUrl(eq(NAV_URL));
        verify(mLargeIconBridge, times(1)).getLargeIconForUrl(eq(NAV_URL), anyInt(), any());
        // Report no icon, only color.
        mIconCallbackCaptor.getValue().onLargeIconAvailable(
                mLargeIconBitmap, FALLBACK_COLOR, true, 0);

        // In the presence of actual icon we should expect to see that icon being applied to the
        // model.
        assertEquals(1, tileList.size());
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;
        Drawable drawable = tileModel.get(TileViewProperties.ICON);

        assertEquals(BaseCarouselSuggestionItemViewBuilder.ViewType.TILE_VIEW, tileItem.type);
        assertThat(drawable, instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
        assertEquals(mLargeIconBitmap, bitmap);
    }

    @Test
    public void testInteractions_onClick() {
        List<ListItem> tileList = populateTilePropertiesForTiles(3,
                new SuggestTile("search1", SEARCH_URL, true),
                new SuggestTile("nav1", NAV_URL, true), new SuggestTile("nav2", NAV_URL_2, true));

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
        verifyNoMoreInteractions(mIconGenerator);
        verifyNoMoreInteractions(mLargeIconBridge);
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
                .onDeleteMatchElement(eq(mMatch), eq("nav1"), eq(1), eq(1));

        tileList.get(2).model.get(TileViewProperties.ON_LONG_CLICK).onLongClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onDeleteMatchElement(eq(mMatch), eq("nav2"), eq(1), eq(2));

        tileList.get(0).model.get(TileViewProperties.ON_LONG_CLICK).onLongClick(null);
        ordered.verify(mSuggestionHost, times(1))
                .onDeleteMatchElement(eq(mMatch), eq("search1"), eq(1), eq(0));

        verifyNoMoreInteractions(mSuggestionHost);
        verifyNoMoreInteractions(mIconGenerator);
        verifyNoMoreInteractions(mLargeIconBridge);
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
        verifyNoMoreInteractions(mIconGenerator);
        verifyNoMoreInteractions(mLargeIconBridge);
    }

    @Test
    public void testAccessibility_searchTile() {
        List<ListItem> tileList =
                populateTilePropertiesForTiles(0, new SuggestTile("title", SEARCH_URL, true));

        assertEquals(1, tileList.size());
        ListItem tileItem = tileList.get(0);
        PropertyModel tileModel = tileItem.model;

        // Expect the search string in announcement.
        String expectedDescription = mActivity.getString(
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
                mActivity.getString(R.string.accessibility_omnibox_most_visited_tile_navigate,
                        "title", NAV_URL.getHost());
        assertEquals(expectedDescription, tileModel.get(TileViewProperties.CONTENT_DESCRIPTION));
    }
}
