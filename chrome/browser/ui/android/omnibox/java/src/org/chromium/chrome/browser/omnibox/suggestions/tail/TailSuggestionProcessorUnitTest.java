// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tail;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link TailSuggestionProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TailSuggestionProcessorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock SuggestionHost mSuggestionHost;

    private TailSuggestionProcessor mProcessor;
    private AutocompleteMatch mSuggestion;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mProcessor = new TailSuggestionProcessor(RuntimeEnvironment.application, mSuggestionHost);
    }

    /** Create search suggestion for test. */
    private void createSearchSuggestion(int type, String title) {
        mSuggestion =
                AutocompleteMatchBuilder.searchWithType(type)
                        .setDisplayText(title)
                        .setFillIntoEdit("fill into edit: " + title)
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mSuggestion, mModel, 0);
    }

    @Test
    @Config(qualifiers = "w400dp")
    public void populateModel_tailSuggestion_phone() {
        mProcessor.onSuggestionsReceived();
        createSearchSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, "tail");

        Assert.assertTrue(mProcessor.doesProcessSuggestion(mSuggestion, 1));
        // Alignment is suppressed on phones.
        Assert.assertNull(mModel.get(TailSuggestionViewProperties.ALIGNMENT_MANAGER));
        Assert.assertEquals("… tail", mModel.get(TailSuggestionViewProperties.TEXT).toString());
        Assert.assertEquals(
                "fill into edit: tail", mModel.get(TailSuggestionViewProperties.FILL_INTO_EDIT));
    }

    @Test
    @Config(qualifiers = "w600dp-h820dp")
    public void populateModel_tailSuggestion_tablet() {
        mProcessor.onSuggestionsReceived();
        createSearchSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, "tail");

        Assert.assertTrue(mProcessor.doesProcessSuggestion(mSuggestion, 1));
        Assert.assertNotNull(mModel.get(TailSuggestionViewProperties.ALIGNMENT_MANAGER));
        Assert.assertEquals("… tail", mModel.get(TailSuggestionViewProperties.TEXT).toString());
        Assert.assertEquals(
                "fill into edit: tail", mModel.get(TailSuggestionViewProperties.FILL_INTO_EDIT));
    }

    @Test
    public void doesProcessSuggestion_nonTailSuggestion() {
        mProcessor.onSuggestionsReceived();
        createSearchSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST, "search");
        Assert.assertFalse(mProcessor.doesProcessSuggestion(mSuggestion, 1));
    }

    @Test
    public void getViewTypeId_forFullTestCoverage() {
        Assert.assertEquals(OmniboxSuggestionUiType.TAIL_SUGGESTION, mProcessor.getViewTypeId());
    }
}
