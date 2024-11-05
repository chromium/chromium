// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.res.Resources;
import android.graphics.Bitmap;

import org.junit.After;
import org.junit.Assert;
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

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager.JumpStartContext;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
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
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Captor ArgumentCaptor<FaviconHelper.FaviconImageCallback> mCallbackCaptor;
    @Mock FaviconHelper mFaviconHelper;
    @Mock TemplateUrlService mTemplateUrlService;
    @Mock TemplateUrl mTemplateUrl;
    @Mock LocaleManagerDelegate mLocaleManagerDelegate;
    @Mock Resources mResources;
    @Mock Profile mProfile;

    Bitmap mBitmap;

    @Before
    public void setUp() {
        mBitmap = Shadow.newInstanceOf(Bitmap.class);
        shadowOf(mBitmap).appendDescription("test");

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        doReturn(TEMPLATE_URL).when(mTemplateUrl).getURL();
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
        // Show DSE logo when using regular profile.
        doReturn(false).when(mProfile).isOffTheRecord();
        searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        assertTrue(searchEngineUtils.shouldShowSearchEngineLogo());

        // Suppress DSE logo when using incognito profile.
        doReturn(true).when(mProfile).isOffTheRecord();
        searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        assertFalse(searchEngineUtils.shouldShowSearchEngineLogo());
    }

    @Test
    public void recordEvent() {
        UmaRecorderHolder.resetForTesting();

        SearchEngineUtils.recordEvent(SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));

        SearchEngineUtils.recordEvent(SearchEngineUtils.Events.FETCH_SUCCESS_CACHE_HIT);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS_CACHE_HIT));
    }

    @Test
    public void getSearchEngineLogo() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        // SearchEngineUtils retrieves logo when it's first created, and whenever the DSE changes.
        verify(mFaviconHelper).getLocalFaviconImageForURL(any(), any(), anyInt(), any());
        mCallbackCaptor.getValue().onFaviconAvailable(mBitmap, new GURL(LOGO_URL));

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS));

        var expected = new StatusIconResource(LOGO_URL, mBitmap, 0);
        var icon = searchEngineUtils.getSearchEngineLogo(BrandedColorScheme.APP_DEFAULT);
        assertEquals(icon, expected);
    }

    @Test
    public void getSearchEngineLogo_nullTemplateUrlService() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        StatusIconResource expected =
                SearchEngineUtils.getFallbackSearchIcon(BrandedColorScheme.APP_DEFAULT);

        var icon = searchEngineUtils.getSearchEngineLogo(BrandedColorScheme.APP_DEFAULT);

        assertEquals(icon, expected);
    }

    @Test
    public void getSearchEngineLogo_searchEngineGoogle() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        // Simulate DSE change to Google.
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        searchEngineUtils.onTemplateURLServiceChanged();

        var expected = new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0);
        var icon = searchEngineUtils.getSearchEngineLogo(BrandedColorScheme.APP_DEFAULT);
        assertEquals(icon, expected);
    }

    private void configureSearchEngine(String keyword) {
        doReturn("google".equals(keyword)).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(keyword).when(mTemplateUrl).getKeyword();
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
            configureSearchEngine("google");
            new SearchEngineUtils(mProfile, mFaviconHelper);
            verifyPersistedSearchEngine("google");
            verifyNoSearchEngineSpecificDataInCache();
        }

        {
            // To Non-Google
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("engine");
            new SearchEngineUtils(mProfile, mFaviconHelper);
            verifyPersistedSearchEngine("engine");
            verifyNoSearchEngineSpecificDataInCache();
        }
    }

    @Test
    public void onTemplateUrlServiceChanged_newTemplateUrl_withDifferentPreviousEngine() {
        {
            // To Google
            configureSearchEngine("engine");
            var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("google");
            searchEngineUtils.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("google");
            verifyNoSearchEngineSpecificDataInCache();
        }

        {
            // To Non-Google
            configureSearchEngine("google");
            var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("engine");
            searchEngineUtils.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("engine");
            verifyNoSearchEngineSpecificDataInCache();
        }
    }

    @Test
    public void onTemplateUrlServiceChanged_newTemplateUrl_withSamePreviousEngine() {
        {
            // Google to Google
            configureSearchEngine("google");
            var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("google");
            searchEngineUtils.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("google");
            verifySearchEngineSpecificDataRetainedInCache();
        }

        {
            // Non-Google to same non-Google.
            configureSearchEngine("engine");
            var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("engine");
            searchEngineUtils.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("engine");
            verifySearchEngineSpecificDataRetainedInCache();
        }
    }

    @Test
    public void getSearchEngineLogo_faviconCached() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        verify(mFaviconHelper).getLocalFaviconImageForURL(any(), any(), anyInt(), any());
        mCallbackCaptor.getValue().onFaviconAvailable(mBitmap, new GURL(LOGO_URL));

        var expected = new StatusIconResource(LOGO_URL, mBitmap, 0);
        var icon = searchEngineUtils.getSearchEngineLogo(BrandedColorScheme.APP_DEFAULT);
        assertEquals(icon, expected);

        icon = searchEngineUtils.getSearchEngineLogo(BrandedColorScheme.APP_DEFAULT);
        assertEquals(icon, expected);

        // Expect only one actual fetch, that happens independently from get request.
        // All get requests always supply cached value.
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS));
        assertEquals(
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS_CACHE_HIT));
    }

    @Test
    public void getSearchEngineLogo_nullUrl() {
        UmaRecorderHolder.resetForTesting();
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);

        // Simulate DSE change - policy blocking searches
        doReturn(null).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        searchEngineUtils.onTemplateURLServiceChanged();

        var expected = SearchEngineUtils.getFallbackSearchIcon(BrandedColorScheme.APP_DEFAULT);
        var icon = searchEngineUtils.getSearchEngineLogo(BrandedColorScheme.APP_DEFAULT);

        assertEquals(icon, expected);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_FAILED_NULL_URL));
    }

    @Test
    public void getSearchEngineLogo_faviconHelperError() {
        UmaRecorderHolder.resetForTesting();

        // Simulate FaviconFetcher failure on the next TemplateUrl change.
        doReturn(false)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), mCallbackCaptor.capture());
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);

        var expected = SearchEngineUtils.getFallbackSearchIcon(BrandedColorScheme.APP_DEFAULT);
        var icon = searchEngineUtils.getSearchEngineLogo(BrandedColorScheme.APP_DEFAULT);

        assertEquals(icon, expected);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM,
                        SearchEngineUtils.Events.FETCH_FAILED_FAVICON_HELPER_ERROR));
    }

    @Test
    public void getSearchEngineLogo_returnedBitmapNull() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        StatusIconResource expected =
                SearchEngineUtils.getFallbackSearchIcon(BrandedColorScheme.APP_DEFAULT);

        var icon = searchEngineUtils.getSearchEngineLogo(BrandedColorScheme.APP_DEFAULT);
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), mCallbackCaptor.capture());
        FaviconHelper.FaviconImageCallback faviconCallback = mCallbackCaptor.getValue();
        faviconCallback.onFaviconAvailable(null, new GURL(LOGO_URL));

        assertEquals(icon, expected);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM,
                        SearchEngineUtils.Events.FETCH_FAILED_RETURNED_BITMAP_NULL));
    }

    @Test
    public void getFallbackSearchIcon() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        StatusIconResource expected =
                new StatusIconResource(
                        R.drawable.ic_search, R.color.default_icon_color_white_tint_list);
        Assert.assertEquals(
                expected,
                SearchEngineUtils.getFallbackSearchIcon(BrandedColorScheme.DARK_BRANDED_THEME));

        expected =
                new StatusIconResource(
                        R.drawable.ic_search,
                        ThemeUtils.getThemedToolbarIconTintRes(/* useLight= */ true));
        Assert.assertEquals(
                expected, SearchEngineUtils.getFallbackSearchIcon(BrandedColorScheme.INCOGNITO));
    }

    @Test
    public void getFallbackNavigationIcon() {
        var searchEngineUtils = new SearchEngineUtils(mProfile, mFaviconHelper);
        StatusIconResource expected =
                new StatusIconResource(
                        R.drawable.ic_globe_24dp, R.color.default_icon_color_white_tint_list);

        Assert.assertEquals(
                expected,
                SearchEngineUtils.getFallbackNavigationIcon(BrandedColorScheme.DARK_BRANDED_THEME));

        expected =
                new StatusIconResource(
                        R.drawable.ic_globe_24dp,
                        ThemeUtils.getThemedToolbarIconTintRes(/* useLight= */ true));
        Assert.assertEquals(
                expected,
                SearchEngineUtils.getFallbackNavigationIcon(BrandedColorScheme.INCOGNITO));
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

    private static Bitmap createSolidImage(int width, int height, int color) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        for (int x = 0; x < bitmap.getWidth(); x++) {
            for (int y = 0; y < bitmap.getHeight(); y++) {
                bitmap.setPixel(x, y, color);
            }
        }
        return bitmap;
    }

    private static Bitmap createSolidImageWithDifferentInnerColor(
            int width, int height, int outerColor, int innerColor) {
        Bitmap bitmap = createSolidImage(width, height, outerColor);
        for (int x = 1; x < bitmap.getWidth() - 1; x++) {
            for (int y = 1; y < bitmap.getHeight() - 1; y++) {
                bitmap.setPixel(x, y, innerColor);
            }
        }
        return bitmap;
    }

    private static Bitmap createSolidImageWithSlighlyLargerEdgeCoverage(
            int width, int height, int largerColor, int smallerColor) {
        Bitmap bitmap = createSolidImage(width, height, largerColor);
        for (int x = 0; x < bitmap.getWidth(); x++) {
            for (int y = bitmap.getHeight() + 1; y < bitmap.getHeight(); y++) {
                bitmap.setPixel(x, y, smallerColor);
            }
        }
        return bitmap;
    }
}
