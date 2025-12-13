// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadow.api.Shadow;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager.JumpStartContext;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;

/** Tests for SearchEngineUtils. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchEngineUtilsUnitTest {
    private static final String LOGO_URL = "https://www.search.com/";
    private static final String TEMPLATE_URL = "https://www.search.com/search?q={query}";
    private static final String EVENTS_HISTOGRAM = "AndroidSearchEngineLogo.Events";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Captor ArgumentCaptor<FaviconHelper.FaviconImageCallback> mCallbackCaptor;
    @Captor ArgumentCaptor<StatusIconResource> mStatusIconCaptor;
    @Mock FaviconHelper mFaviconHelper;
    @Mock TemplateUrlService mTemplateUrlService;
    @Mock TemplateUrl mTemplateUrl;
    @Mock LocaleManagerDelegate mLocaleManagerDelegate;
    @Mock Resources mResources;
    @Mock Profile mProfile;
    @Mock SearchEngineUtils.SearchBoxHintTextObserver mHintTextObserver;
    @Mock SearchEngineUtils.SearchEngineIconObserver mEngineIconObserver;

    private Context mContext;
    private Bitmap mBitmap;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mBitmap = Shadow.newInstanceOf(Bitmap.class);
        shadowOf(mBitmap).appendDescription("test");

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        doReturn(TEMPLATE_URL).when(mTemplateUrl).getURL();
        GURL faviconUrl = new GURL(LOGO_URL);
        doReturn(faviconUrl).when(mTemplateUrl).getFaviconURL();
        doReturn(mTemplateUrl).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), any());
        doReturn(false).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();
        LocaleManager.getInstance().setDelegateForTest(mLocaleManagerDelegate);

        lenient()
                .doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), mCallbackCaptor.capture());

        // Used when creating bitmaps, needs to be greater than 0.
        doReturn(1).when(mResources).getDimensionPixelSize(anyInt());
    }

    @After
    public void tearDown() {
        UmaRecorderHolder.resetForTesting();
    }

    @Test
    public void testDefaultEnabledBehavior() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        searchEngineUtils.addSearchBoxHintTextObserver(mHintTextObserver);

        // Show DSE logo when using regular profile.
        doReturn(false).when(mProfile).isOffTheRecord();
        searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        assertTrue(searchEngineUtils.shouldShowSearchEngineLogo());

        // Suppress DSE logo when using incognito profile.
        doReturn(true).when(mProfile).isOffTheRecord();
        searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        assertFalse(searchEngineUtils.shouldShowSearchEngineLogo());

        // Verify default placeholder text.
        verify(mHintTextObserver).onSearchBoxHintTextChanged();
        assertEquals(
                searchEngineUtils.getOmniboxHintText(AutocompleteRequestType.SEARCH),
                mContext.getString(R.string.omnibox_empty_hint));
    }

    @Test
    public void recordEvent() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                EVENTS_HISTOGRAM,
                                SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST)
                        .expectIntRecord(
                                EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS_CACHE_HIT)
                        .build();
        UmaRecorderHolder.resetForTesting();

        SearchEngineUtils.recordEvent(SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST);

        SearchEngineUtils.recordEvent(SearchEngineUtils.Events.FETCH_SUCCESS_CACHE_HIT);
        histograms.assertExpected();
    }

    @Test
    public void getSearchEngineLogo() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                EVENTS_HISTOGRAM,
                                SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST)
                        .expectIntRecord(
                                EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS_CACHE_HIT)
                        .expectIntRecord(EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS)
                        .build();
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        searchEngineUtils.addIconObserver(mEngineIconObserver);
        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
        reset(mEngineIconObserver);

        // SearchEngineUtils retrieves logo when it's first created, and whenever the DSE changes.
        verify(mFaviconHelper).getLocalFaviconImageForURL(any(), any(), anyInt(), any());
        mCallbackCaptor.getValue().onFaviconAvailable(mBitmap, new GURL(LOGO_URL));

        histograms.assertExpected();

        verify(mEngineIconObserver).onSearchEngineIconChanged(isNotNull());
    }

    @Test
    public void getSearchEngineLogo_nullTemplateUrlService() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        searchEngineUtils.addIconObserver(mEngineIconObserver);

        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
    }

    @Test
    public void getSearchEngineLogo_searchEngineGoogle() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        searchEngineUtils.addIconObserver(mEngineIconObserver);
        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
        reset(mEngineIconObserver);

        // Simulate DSE change to Google.
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        searchEngineUtils.onTemplateURLServiceChanged();

        verify(mEngineIconObserver).onSearchEngineIconChanged(mStatusIconCaptor.capture());
        assertEquals(
                mStatusIconCaptor.getValue(),
                new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0));
    }

    private void configureSearchEngine(String keyword, String shortName) {
        doReturn("google".equals(keyword)).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(keyword).when(mTemplateUrl).getKeyword();
        doReturn(shortName).when(mTemplateUrl).getShortName();
    }

    private void verifyPersistedSearchEngine(String keyword) {
        var dseMetadata = CachedZeroSuggestionsManager.readSearchEngineMetadata();
        assertEquals(keyword, dseMetadata.keyword);
    }

    private void saveSearchEngineSpecificDataToCache() {
        CachedZeroSuggestionsManager.saveJumpStartContext(
                new JumpStartContext(new GURL("https://some.url"), 12345));
    }

    private void verifyNoSearchEngineSpecificDataInCache() {
        var jumpStartContext = CachedZeroSuggestionsManager.readJumpStartContext();
        assertEquals(UrlConstants.NTP_URL, jumpStartContext.url.getSpec());
        assertEquals(
                PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE,
                jumpStartContext.pageClass);
    }

    private void verifySearchEngineSpecificDataRetainedInCache() {
        var jumpStartContext = CachedZeroSuggestionsManager.readJumpStartContext();
        assertEquals(new GURL("https://some.url"), jumpStartContext.url);
        assertEquals(12345, jumpStartContext.pageClass);
    }

    @Test
    public void onTemplateUrlServiceChanged_newTemplateUrl_noPreviousEngine() {
        {
            // To Google
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("google", "Google");
            new SearchEngineUtils(mProfile, mFaviconHelper);
            verifyPersistedSearchEngine("google");
            verifyNoSearchEngineSpecificDataInCache();
        }

        {
            // To Non-Google
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("engine", "Some Engine");
            new SearchEngineUtils(mProfile, mFaviconHelper);
            verifyPersistedSearchEngine("engine");
            verifyNoSearchEngineSpecificDataInCache();
        }
    }

    @Test
    public void onTemplateUrlServiceChanged_newTemplateUrl_withDifferentPreviousEngine() {
        {
            // To Google
            configureSearchEngine("engine", "Some Engine");
            var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("google", "Google");
            searchEngineUtils.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("google");
            verifyNoSearchEngineSpecificDataInCache();
        }

        {
            // To Non-Google
            configureSearchEngine("google", "Google");
            var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("engine", "Some Engine");
            searchEngineUtils.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("engine");
            verifyNoSearchEngineSpecificDataInCache();
        }
    }

    @Test
    public void onTemplateUrlServiceChanged_newTemplateUrl_withSamePreviousEngine() {
        {
            // Google to Google
            configureSearchEngine("google", "Google");
            var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
            searchEngineUtils.addSearchBoxHintTextObserver(mHintTextObserver);

            // Verify updated placeholder text.
            verify(mHintTextObserver).onSearchBoxHintTextChanged();
            assertEquals(
                    "Search Google or type URL",
                    searchEngineUtils.getOmniboxHintText(AutocompleteRequestType.SEARCH));
            clearInvocations(mHintTextObserver);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("google", "Google");
            searchEngineUtils.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("google");
            verifySearchEngineSpecificDataRetainedInCache();

            // Verify updated placeholder text.
            verify(mHintTextObserver, never()).onSearchBoxHintTextChanged();
        }

        clearInvocations(mHintTextObserver);

        {
            // Non-Google to same non-Google.
            configureSearchEngine("engine", "Some Engine");
            var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
            searchEngineUtils.addSearchBoxHintTextObserver(mHintTextObserver);
            clearInvocations(mHintTextObserver);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("engine", "Another Engine");
            searchEngineUtils.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("engine");
            verifySearchEngineSpecificDataRetainedInCache();

            // Verify updated placeholder text.
            verify(mHintTextObserver).onSearchBoxHintTextChanged();
            assertEquals(
                    "Search Another Engine or type URL",
                    searchEngineUtils.getOmniboxHintText(AutocompleteRequestType.SEARCH));
        }

        clearInvocations(mHintTextObserver);

        {
            // Non-Google, unnamed engine
            configureSearchEngine("engine", "Some Engine");
            var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
            searchEngineUtils.addSearchBoxHintTextObserver(mHintTextObserver);
            clearInvocations(mHintTextObserver);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("engine", null);
            searchEngineUtils.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("engine");
            verifySearchEngineSpecificDataRetainedInCache();

            // Verify default placeholder text.
            verify(mHintTextObserver).onSearchBoxHintTextChanged();
            assertEquals(
                    "Search or type URL",
                    searchEngineUtils.getOmniboxHintText(AutocompleteRequestType.SEARCH));
        }

        clearInvocations(mHintTextObserver);

        {
            // Non-Google, unnamed engine
            configureSearchEngine("engine", "Some Engine");
            var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
            searchEngineUtils.addSearchBoxHintTextObserver(mHintTextObserver);
            clearInvocations(mHintTextObserver);

            // Make an update to no engine
            doReturn(null).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
            searchEngineUtils.onTemplateURLServiceChanged();

            // Verify default placeholder text.
            verify(mHintTextObserver).onSearchBoxHintTextChanged();
            assertEquals(
                    "Search or type URL",
                    searchEngineUtils.getOmniboxHintText(AutocompleteRequestType.SEARCH));
        }
    }

    @Test
    public void getSearchEngineLogo_faviconCached() {
        // Expect only one actual fetch, that happens independently from get request.
        // All get requests always supply cached value.
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                EVENTS_HISTOGRAM,
                                SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST)
                        .expectIntRecord(
                                EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS_CACHE_HIT)
                        .expectIntRecord(EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS)
                        .build();
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        searchEngineUtils.addIconObserver(mEngineIconObserver);
        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
        reset(mEngineIconObserver);

        verify(mFaviconHelper).getLocalFaviconImageForURL(any(), any(), anyInt(), any());
        mCallbackCaptor.getValue().onFaviconAvailable(mBitmap, new GURL(LOGO_URL));

        verify(mEngineIconObserver).onSearchEngineIconChanged(isNotNull());

        histograms.assertExpected();
    }

    @Test
    public void getSearchEngineLogo_nullUrl() {
        UmaRecorderHolder.resetForTesting();
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                EVENTS_HISTOGRAM,
                                SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST)
                        .expectIntRecord(
                                EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_FAILED_NULL_URL)
                        .build();
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        searchEngineUtils.addIconObserver(mEngineIconObserver);

        // Simulate DSE change - policy blocking searches
        doReturn(null).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        searchEngineUtils.onTemplateURLServiceChanged();

        verify(mEngineIconObserver).onSearchEngineIconChanged(null);

        histograms.assertExpected();
    }

    @Test
    public void getSearchEngineLogo_faviconHelperError() {
        UmaRecorderHolder.resetForTesting();
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                EVENTS_HISTOGRAM,
                                SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST)
                        .expectIntRecord(
                                EVENTS_HISTOGRAM,
                                SearchEngineUtils.Events.FETCH_FAILED_FAVICON_HELPER_ERROR)
                        .build();
        // Simulate FaviconFetcher failure on the next TemplateUrl change.
        doReturn(false)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), mCallbackCaptor.capture());
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        searchEngineUtils.addIconObserver(mEngineIconObserver);

        verify(mEngineIconObserver).onSearchEngineIconChanged(null);

        histograms.assertExpected();
    }

    @Test
    public void getSearchEngineLogo_returnedBitmapNull() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                EVENTS_HISTOGRAM,
                                SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST)
                        .expectIntRecord(
                                EVENTS_HISTOGRAM,
                                SearchEngineUtils.Events.FETCH_FAILED_RETURNED_BITMAP_NULL)
                        .build();
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        searchEngineUtils.addIconObserver(mEngineIconObserver);

        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
        reset(mEngineIconObserver);

        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), mCallbackCaptor.capture());
        FaviconHelper.FaviconImageCallback faviconCallback = mCallbackCaptor.getValue();
        faviconCallback.onFaviconAvailable(null, new GURL(LOGO_URL));

        histograms.assertExpected();

        // Not emitting second null icon
        verifyNoMoreInteractions(mEngineIconObserver);
    }

    @Test
    public void needToCheckForSearchEnginePromo_SecurityExceptionThrown() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        doThrow(SecurityException.class)
                .when(mLocaleManagerDelegate)
                .needToCheckForSearchEnginePromo();

        try {
            searchEngineUtils.needToCheckForSearchEnginePromo();
        } catch (Exception e) {
            throw new AssertionError("No exception should be thrown.", e);
        }
    }

    @Test
    public void needToCheckForSearchEnginePromo_DeadObjectRuntimeExceptionThrown() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        doThrow(RuntimeException.class)
                .when(mLocaleManagerDelegate)
                .needToCheckForSearchEnginePromo();

        try {
            searchEngineUtils.needToCheckForSearchEnginePromo();
        } catch (Exception e) {
            throw new AssertionError("No exception should be thrown.", e);
        }
    }

    @Test
    public void needToCheckForSearchEnginePromo_resultCached() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        doThrow(RuntimeException.class)
                .when(mLocaleManagerDelegate)
                .needToCheckForSearchEnginePromo();
        assertFalse(searchEngineUtils.needToCheckForSearchEnginePromo());

        Mockito.reset(mLocaleManagerDelegate);

        doReturn(true).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();

        assertTrue(searchEngineUtils.needToCheckForSearchEnginePromo());

        Mockito.reset(mLocaleManagerDelegate);

        doReturn(false).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();

        assertFalse(searchEngineUtils.needToCheckForSearchEnginePromo());
        assertFalse(searchEngineUtils.needToCheckForSearchEnginePromo());
        assertFalse(searchEngineUtils.needToCheckForSearchEnginePromo());

        verify(mLocaleManagerDelegate, times(1)).needToCheckForSearchEnginePromo();
    }
}
