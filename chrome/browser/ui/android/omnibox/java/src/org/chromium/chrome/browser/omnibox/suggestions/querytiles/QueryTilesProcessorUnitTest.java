// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/** Tests for {@link MostVisitedTilesProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class QueryTilesProcessorUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private PropertyModel mModel;
    private QueryTilesProcessor mProcessor;
    private List<ListItem> mTiles;
    private @Mock SuggestionHost mSuggestionHost;
    private @Mock OmniboxImageSupplier mImageSupplier;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mProcessor = new QueryTilesProcessor(mContext, mSuggestionHost, mImageSupplier);
        mModel = mProcessor.createModel();
        mTiles = mModel.get(BaseCarouselSuggestionViewProperties.TILES);
        OmniboxResourceProvider.disableCachesForTesting();
    }

    @After
    public void tearDown() {
        OmniboxResourceProvider.reenableCachesForTesting();
    }

    @Test
    public void doesProcessSuggestion() {
        for (int type = 0; type < OmniboxSuggestionType.NUM_TYPES; type++) {
            var match = AutocompleteMatchBuilder.searchWithType(type).build();
            assertEquals(
                    type == OmniboxSuggestionType.TILE_SUGGESTION,
                    mProcessor.doesProcessSuggestion(match, 0));
        }
    }

    @Test
    public void getViewTypeId() {
        assertEquals(OmniboxSuggestionUiType.QUERY_TILES, mProcessor.getViewTypeId());
    }

    @Test
    public void getCarouselItemViewSize() {
        var view = new QueryTileView(mContext);
        view.measure(
                View.MeasureSpec.makeMeasureSpec(Integer.MAX_VALUE, View.MeasureSpec.AT_MOST),
                View.MeasureSpec.makeMeasureSpec(Integer.MAX_VALUE, View.MeasureSpec.AT_MOST));

        assertEquals(view.getMeasuredHeight(), mProcessor.getCarouselItemViewHeight());
        assertEquals(view.getMeasuredWidth(), mProcessor.getCarouselItemViewWidth());
    }

    @Test
    public void populateModel_oneTileForEveryMatch() {
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.TILE_SUGGESTION)
                        .build();
        assertEquals(0, mTiles.size());

        mProcessor.populateModel(match, mModel, 0);
        assertEquals(1, mTiles.size());

        mProcessor.populateModel(match, mModel, 0);
        assertEquals(2, mTiles.size());

        mProcessor.populateModel(match, mModel, 0);
        assertEquals(3, mTiles.size());

        verifyNoMoreInteractions(mImageSupplier, mSuggestionHost);
    }

    @Test
    public void populateModel_displayTextUsedAsTitle() {
        var matchTmpl =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.TILE_SUGGESTION);
        var match1 = matchTmpl.setDisplayText("News").build();
        var match2 = matchTmpl.setDisplayText("Movies").build();
        var match3 = matchTmpl.setDisplayText("Games").build();

        mProcessor.populateModel(match1, mModel, 0);
        mProcessor.populateModel(match2, mModel, 0);
        mProcessor.populateModel(match3, mModel, 0);

        assertEquals(3, mTiles.size());

        assertEquals("News", mTiles.get(0).model.get(QueryTileViewProperties.TITLE));
        assertEquals("Movies", mTiles.get(1).model.get(QueryTileViewProperties.TITLE));
        assertEquals("Games", mTiles.get(2).model.get(QueryTileViewProperties.TITLE));

        verifyNoMoreInteractions(mImageSupplier, mSuggestionHost);
    }

    @Test
    public void populateModel_imageUrlUsedAsThumbnail() {
        var imageUrl = new GURL("http://image.url");
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.TILE_SUGGESTION)
                        .setImageUrl(imageUrl)
                        .build();
        ArgumentCaptor<Callback<Bitmap>> captor = ArgumentCaptor.forClass(Callback.class);

        mProcessor.populateModel(match, mModel, 0);
        assertEquals(1, mTiles.size());

        // Confirm image request sent.
        verify(mImageSupplier).fetchImage(eq(imageUrl), captor.capture());
        verifyNoMoreInteractions(mImageSupplier);

        // Image not sent back yet. Expect no image in model.
        assertEquals(null, mTiles.get(0).model.get(QueryTileViewProperties.IMAGE));

        // Send bitmap back.
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8);
        captor.getValue().onResult(bitmap);

        // Confirm we got BitmapDrawable that encapsulates our bitmap.
        BitmapDrawable drawable =
                (BitmapDrawable) mTiles.get(0).model.get(QueryTileViewProperties.IMAGE);
        assertEquals(bitmap, drawable.getBitmap());

        verifyNoMoreInteractions(mImageSupplier, mSuggestionHost);
    }

    @Test
    public void populateModel_noImageRequestsWhenImageIsNotSet() {
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.TILE_SUGGESTION)
                        .build();

        mProcessor.populateModel(match, mModel, 0);
        assertEquals(1, mTiles.size());
        verifyNoMoreInteractions(mImageSupplier, mSuggestionHost);
    }

    @Test
    public void populateModel_noImageRequestsWhenImageSupplierIsNotSet() {
        mProcessor = new QueryTilesProcessor(mContext, mSuggestionHost, null);
        var imageUrl = new GURL("http://image.url");
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.TILE_SUGGESTION)
                        .setImageUrl(imageUrl)
                        .build();

        mProcessor.populateModel(match, mModel, 0);
        assertEquals(1, mTiles.size());
        verifyNoMoreInteractions(mImageSupplier, mSuggestionHost);
    }

    @Test
    public void populateModel_fillIntoEditUsedOnFocus() {
        var matchTmpl =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.TILE_SUGGESTION);
        var match1 = matchTmpl.setFillIntoEdit("Fill News").build();
        var match2 = matchTmpl.setFillIntoEdit("Fill Movies").build();
        var match3 = matchTmpl.setFillIntoEdit("Fill Games").build();

        mProcessor.populateModel(match1, mModel, 0);
        mProcessor.populateModel(match2, mModel, 0);
        mProcessor.populateModel(match3, mModel, 0);

        assertEquals(3, mTiles.size());

        mTiles.get(0).model.get(QueryTileViewProperties.ON_FOCUS_VIA_SELECTION).run();
        verify(mSuggestionHost).setOmniboxEditingText("Fill News");
        mTiles.get(1).model.get(QueryTileViewProperties.ON_FOCUS_VIA_SELECTION).run();
        verify(mSuggestionHost).setOmniboxEditingText("Fill Movies");
        mTiles.get(2).model.get(QueryTileViewProperties.ON_FOCUS_VIA_SELECTION).run();
        verify(mSuggestionHost).setOmniboxEditingText("Fill Games");

        verifyNoMoreInteractions(mImageSupplier, mSuggestionHost);
    }

    @Test
    public void populateModel_clickEventInitiatesNavigation() {
        var matchTmpl =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.TILE_SUGGESTION);
        var url1 = new GURL("http:/one");
        var url2 = new GURL("http:/two");
        var url3 = new GURL("http:/ate");
        var match1 = matchTmpl.setUrl(url1).build();
        var match2 = matchTmpl.setUrl(url2).build();
        var match3 = matchTmpl.setUrl(url3).build();

        mProcessor.populateModel(match1, mModel, 7);
        mProcessor.populateModel(match2, mModel, 7);
        mProcessor.populateModel(match3, mModel, 7);

        assertEquals(3, mTiles.size());

        mTiles.get(0).model.get(QueryTileViewProperties.ON_CLICK).onClick(null);
        verify(mSuggestionHost).onSuggestionClicked(match1, 7, url1);
        mTiles.get(1).model.get(QueryTileViewProperties.ON_CLICK).onClick(null);
        verify(mSuggestionHost).onSuggestionClicked(match2, 7, url2);
        mTiles.get(2).model.get(QueryTileViewProperties.ON_CLICK).onClick(null);
        verify(mSuggestionHost).onSuggestionClicked(match3, 7, url3);

        verifyNoMoreInteractions(mImageSupplier, mSuggestionHost);
    }
}
