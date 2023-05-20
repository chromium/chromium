// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.action.HistoryClustersAction;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.ShadowGURL;

import java.util.Arrays;

/**
 * Tests for {@link HistoryClustersProcessor}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGURL.class})
public class HistoryClustersProcessorTest {
    @Rule
    public final TestRule mFeaturesProcessor = new Features.JUnitProcessor();
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock UrlBarEditingTextStateProvider mUrlBarText;
    private @Mock FaviconFetcher mIconFetcher;
    @Mock
    private BookmarkState mBookmarkState;
    @Mock
    private HistoryClustersProcessor.OpenHistoryClustersDelegate mOpenHistoryClustersDelegate;

    private HistoryClustersProcessor mProcessor;

    @Before
    public void setUp() {
        doReturn("").when(mUrlBarText).getTextWithoutAutocomplete();
        mProcessor = new HistoryClustersProcessor(mOpenHistoryClustersDelegate,
                ContextUtils.getApplicationContext(), mSuggestionHost, mUrlBarText, mIconFetcher,
                mBookmarkState);
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER})
    public void doesProcessSuggestion_featureOff() {
        assertFalse(mProcessor.doesProcessSuggestion(createHistoryClustersSuggestion("foobar"), 1));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER})
    public void doesProcessSuggestion() {
        assertTrue(mProcessor.doesProcessSuggestion(createHistoryClustersSuggestion("foobar"), 1));
        assertFalse(mProcessor.doesProcessSuggestion(createSearchSuggestion("foobar"), 1));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER})
    public void testPopulateModel() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Omnibox.ResumeJourneyShown", 2);
        PropertyModel propertyModel = mProcessor.createModel();
        AutocompleteMatch suggestion = createHistoryClustersSuggestion("foobar");
        mProcessor.populateModel(suggestion, propertyModel, 2);
        assertEquals(new SuggestionSpannable(suggestion.getActions().get(0).hint),
                propertyModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT));
        assertTrue(propertyModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        assertNull(propertyModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
        SuggestionDrawableState sds = propertyModel.get(BaseSuggestionViewProperties.ICON);
        assertNotNull(sds);
        assertEquals(R.drawable.action_journeys, sds.resourceId);

        mProcessor.onUrlFocusChange(false);
        watcher.assertExpected();

        propertyModel = mProcessor.createModel();
        mProcessor.populateModel(createSearchSuggestion("foobar"), propertyModel, 0);
        assertEquals(0, propertyModel.getAllSetProperties().size());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER})
    public void testOnClick() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Omnibox.SuggestionUsed.ResumeJourney", 2);
        PropertyModel propertyModel = mProcessor.createModel();
        AutocompleteMatch suggestion = createHistoryClustersSuggestion("foobar");
        mProcessor.populateModel(suggestion, propertyModel, 2);
        propertyModel.get(BaseSuggestionViewProperties.ON_CLICK).run();

        verify(mOpenHistoryClustersDelegate).openHistoryClustersUi("foobar");
        watcher.assertExpected();
    }

    private AutocompleteMatch createHistoryClustersSuggestion(String query) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED)
                .setDisplayText(query)
                .setIsSearch(true)
                .setActions(Arrays.asList(new HistoryClustersAction("hint", query)))
                .build();
    }

    private AutocompleteMatch createSearchSuggestion(String query) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED)
                .setDisplayText(query)
                .setIsSearch(true)
                .build();
    }
}
