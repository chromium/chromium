// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.history_clusters;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.action.HistoryClustersAction;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;

/** Tests for {@link HistoryClustersProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HistoryClustersProcessorTest {
    public final @Rule TestRule mFeaturesProcessor = new Features.JUnitProcessor();
    public final @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock OmniboxAction mMockAction;
    private @Mock UrlBarEditingTextStateProvider mUrlBarText;
    private @Mock OmniboxImageSupplier mImageSupplier;
    private @Mock BookmarkState mBookmarkState;
    private @Mock HistoryClustersProcessor.OpenHistoryClustersDelegate mOpenHistoryClustersDelegate;

    private HistoryClustersProcessor mProcessor;

    @Before
    public void setUp() {
        doReturn("").when(mUrlBarText).getTextWithoutAutocomplete();
        mProcessor =
                new HistoryClustersProcessor(
                        mOpenHistoryClustersDelegate,
                        ContextUtils.getApplicationContext(),
                        mSuggestionHost,
                        mUrlBarText,
                        mImageSupplier,
                        mBookmarkState);
        OmniboxResourceProvider.disableCachesForTesting();
    }

    @After
    public void tearDown() {
        OmniboxResourceProvider.reenableCachesForTesting();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER)
    public void doesProcessSuggestion_featureOff() {
        assertFalse(mProcessor.doesProcessSuggestion(createHistoryClustersSuggestion("foobar"), 1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER)
    public void doesProcessSuggestion() {
        assertTrue(mProcessor.doesProcessSuggestion(createHistoryClustersSuggestion("foobar"), 1));
        assertFalse(
                mProcessor.doesProcessSuggestion(
                        createSearchSuggestionBuilder("foobar").build(), 1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER)
    public void testPopulateModel() {
        PropertyModel propertyModel = mProcessor.createModel();
        AutocompleteMatch suggestion = createHistoryClustersSuggestion("foobar");
        mProcessor.populateModel(suggestion, propertyModel, 2);
        assertEquals(
                new SuggestionSpannable(suggestion.getActions().get(0).hint),
                propertyModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT));
        assertTrue(propertyModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        assertNull(propertyModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
        OmniboxDrawableState sds = propertyModel.get(BaseSuggestionViewProperties.ICON);
        assertNotNull(sds);
        assertEquals(R.drawable.action_journeys, shadowOf(sds.drawable).getCreatedFromResId());

        HistogramWatcher noRecordsWatcher =
                HistogramWatcher.newBuilder().expectNoRecords("Omnibox.ResumeJourneyShown").build();
        mProcessor.onOmniboxSessionStateChange(true);
        noRecordsWatcher.assertExpected();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Omnibox.ResumeJourneyShown", 2);
        mProcessor.onOmniboxSessionStateChange(false);
        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER)
    public void doesProcessSuggestion_suggestionWithNoActions() {
        assertFalse(
                mProcessor.doesProcessSuggestion(
                        createSearchSuggestionBuilder("foobar").build(), 0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER)
    public void doesProcessSuggestion_suggestionWithNoHistoryClusterActions() {
        assertFalse(
                mProcessor.doesProcessSuggestion(
                        createSearchSuggestionBuilder("foobar")
                                .setActions(Arrays.asList(mMockAction))
                                .build(),
                        0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER)
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

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER)
    public void testOnClick_noDelegate() {
        mProcessor =
                new HistoryClustersProcessor(
                        /* openHistoryClustersDelegate= */ null,
                        ContextUtils.getApplicationContext(),
                        mSuggestionHost,
                        mUrlBarText,
                        mImageSupplier,
                        mBookmarkState);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Omnibox.SuggestionUsed.ResumeJourney")
                        .build();
        PropertyModel propertyModel = mProcessor.createModel();
        AutocompleteMatch suggestion = createHistoryClustersSuggestion("foobar");
        mProcessor.populateModel(suggestion, propertyModel, 2);
        propertyModel.get(BaseSuggestionViewProperties.ON_CLICK).run();
        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER)
    public void testOnLongClick() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Omnibox.SuggestionUsed.ResumeJourney", 2);
        PropertyModel propertyModel = mProcessor.createModel();
        AutocompleteMatch suggestion = createHistoryClustersSuggestion("foobar");
        mProcessor.populateModel(suggestion, propertyModel, 2);
        propertyModel.get(BaseSuggestionViewProperties.ON_LONG_CLICK).run();

        verify(mOpenHistoryClustersDelegate).openHistoryClustersUi("foobar");
        watcher.assertExpected();
    }

    private AutocompleteMatch createHistoryClustersSuggestion(String query) {
        return createSearchSuggestionBuilder(query)
                .setActions(
                        Arrays.asList(new HistoryClustersAction(0, "hint", "accessibility", query)))
                .build();
    }

    private AutocompleteMatchBuilder createSearchSuggestionBuilder(String query) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED)
                .setDisplayText(query)
                .setIsSearch(true);
    }
}
