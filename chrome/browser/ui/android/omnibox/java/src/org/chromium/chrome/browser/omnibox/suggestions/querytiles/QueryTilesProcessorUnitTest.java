// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.ContextThemeWrapper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

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
    public void getMinimumCarouselItemViewHeight() {
        assertEquals(0, mProcessor.getMinimumCarouselItemViewHeight());
    }

    @Test
    public void populateModel() {
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.TILE_SUGGESTION)
                        .build();
        mProcessor.populateModel(match, mModel, 0);

        // Currently expected to do nothing.
        assertEquals(0, mTiles.size());
    }
}
