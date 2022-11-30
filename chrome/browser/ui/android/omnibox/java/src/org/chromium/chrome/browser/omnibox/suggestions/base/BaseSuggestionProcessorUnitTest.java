// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher.FaviconFetchCompleteListener;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher.FaviconType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

/**
 * Tests for {@link BaseSuggestionViewProcessor}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGURL.class, ShadowLog.class})
public class BaseSuggestionProcessorUnitTest {
    private class TestBaseSuggestionProcessor extends BaseSuggestionViewProcessor {
        private final Context mContext;
        public TestBaseSuggestionProcessor(
                Context context, SuggestionHost suggestionHost, FaviconFetcher faviconFetcher) {
            super(context, suggestionHost, faviconFetcher);
            mContext = context;
        }

        @Override
        public PropertyModel createModel() {
            return new PropertyModel(BaseSuggestionViewProperties.ALL_KEYS);
        }

        @Override
        public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
            return true;
        }

        @Override
        public int getViewTypeId() {
            return OmniboxSuggestionUiType.DEFAULT;
        }

        @Override
        public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
            super.populateModel(suggestion, model, position);
            setSuggestionDrawableState(model,
                    SuggestionDrawableState.Builder.forBitmap(mContext, mDefaultBitmap).build());
            fetchSuggestionFavicon(model, suggestion.getUrl());
        }
    }

    private static final GURL TEST_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock FaviconFetcher mFaviconFetcher;
    private @Mock Bitmap mBitmap;
    private @Mock Bitmap mDefaultBitmap;

    private TestBaseSuggestionProcessor mProcessor;
    private AutocompleteMatch mSuggestion;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mProcessor = new TestBaseSuggestionProcessor(
                ContextUtils.getApplicationContext(), mSuggestionHost, mFaviconFetcher);
    }

    /**
     * Create Suggestion for test.
     */
    private void createSuggestion(int type, GURL url) {
        mSuggestion = AutocompleteMatchBuilder.searchWithType(type).setUrl(url).build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mSuggestion, mModel, 0);
    }

    @Test
    @SmallTest
    public void suggestionFavicons_showFaviconWhenAvailable() {
        final ArgumentCaptor<FaviconFetchCompleteListener> callback =
                ArgumentCaptor.forClass(FaviconFetchCompleteListener.class);
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, TEST_URL);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mFaviconFetcher)
                .fetchFaviconWithBackoff(eq(TEST_URL), eq(false), callback.capture());
        callback.getValue().onFaviconFetchComplete(mBitmap, FaviconType.REGULAR);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertNotEquals(icon1, icon2);
        Assert.assertEquals(mBitmap, ((BitmapDrawable) icon2.drawable).getBitmap());
    }

    @Test
    @SmallTest
    public void suggestionFavicons_doNotReplaceFallbackIconWhenNoFaviconIsAvailable() {
        final ArgumentCaptor<FaviconFetchCompleteListener> callback =
                ArgumentCaptor.forClass(FaviconFetchCompleteListener.class);
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, TEST_URL);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mFaviconFetcher)
                .fetchFaviconWithBackoff(eq(TEST_URL), eq(false), callback.capture());
        callback.getValue().onFaviconFetchComplete(null, FaviconType.NONE);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertEquals(icon1, icon2);
    }
}
