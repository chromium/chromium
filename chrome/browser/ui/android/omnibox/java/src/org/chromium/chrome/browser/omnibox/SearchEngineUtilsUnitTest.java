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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.res.Resources;
import android.graphics.Bitmap;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for SearchEngineUtils. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchEngineUtilsUnitTest {
    private static final String LOGO_URL = JUnitTestGURLs.URL_1.getSpec();
    private static final String EVENTS_HISTOGRAM = "AndroidSearchEngineLogo.Events";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Captor ArgumentCaptor<FaviconHelper.FaviconImageCallback> mCallbackCaptor;
    @Mock FaviconHelper mFaviconHelper;
    @Mock TemplateUrlService mTemplateUrlService;
    @Mock LocaleManagerDelegate mLocaleManagerDelegate;
    @Mock Resources mResources;
    @Mock Profile mProfile;

    SearchEngineUtils mSearchEngineUtils;
    Bitmap mBitmap;

    @Before
    public void setUp() {
        mBitmap = Shadow.newInstanceOf(Bitmap.class);
        shadowOf(mBitmap).appendDescription("test");

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(LOGO_URL).when(mTemplateUrlService).getUrlForSearchQuery(any());
        doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), any());
        doReturn(false).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();
        LocaleManager.getInstance().setDelegateForTest(mLocaleManagerDelegate);

        // Used when creating bitmaps, needs to be greater than 0.
        doReturn(1).when(mResources).getDimensionPixelSize(anyInt());

        mSearchEngineUtils = new SearchEngineUtils(mProfile);
        mSearchEngineUtils.setFaviconHelperForTesting(mFaviconHelper);
    }

    @After
    public void tearDown() {
        UmaRecorderHolder.resetForTesting();
    }

    @Test
    public void testDefaultEnabledBehavior() {
        // Show DSE logo when using regular profile.
        doReturn(false).when(mProfile).isOffTheRecord();
        mSearchEngineUtils = new SearchEngineUtils(mProfile);
        assertTrue(mSearchEngineUtils.shouldShowSearchEngineLogo());

        // Suppress DSE logo when using incognito profile.
        doReturn(true).when(mProfile).isOffTheRecord();
        mSearchEngineUtils = new SearchEngineUtils(mProfile);
        assertFalse(mSearchEngineUtils.shouldShowSearchEngineLogo());
    }

    @Test
    public void recordEvent() {
        mSearchEngineUtils.recordEvent(
                SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM,
                        SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
    }

    @Test
    public void getSearchEngineLogo() {
        StatusIconResource expected = new StatusIconResource(LOGO_URL, mBitmap, 0);

        Promise<StatusIconResource> promise =
                mSearchEngineUtils.getSearchEngineLogo(
                        BrandedColorScheme.APP_DEFAULT,
                        mTemplateUrlService);
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), mCallbackCaptor.capture());
        FaviconHelper.FaviconImageCallback faviconCallback = mCallbackCaptor.getValue();
        faviconCallback.onFaviconAvailable(mBitmap, new GURL(LOGO_URL));

        assertTrue(promise.isFulfilled());
        assertEquals(promise.getResult(), expected);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM,
                        SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS));
    }

    @Test
    public void getSearchEngineLogo_nullTemplateUrlService() {
        StatusIconResource expected =
                SearchEngineUtils.getSearchLoupeResource(BrandedColorScheme.APP_DEFAULT);

        Promise<StatusIconResource> promise =
                mSearchEngineUtils.getSearchEngineLogo(
                        BrandedColorScheme.APP_DEFAULT,
                        null);

        assertEquals(promise.getResult(), expected);
    }

    @Test
    public void getSearchEngineLogo_searchEngineGoogle() {
        StatusIconResource expected = new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        Promise<StatusIconResource> promise =
                mSearchEngineUtils.getSearchEngineLogo(
                        BrandedColorScheme.APP_DEFAULT,
                        mTemplateUrlService);
        assertEquals(promise.getResult(), expected);
    }

    @Test
    public void getSearchEngineLogo_faviconCached() {
        StatusIconResource expected = new StatusIconResource(LOGO_URL, mBitmap, 0);

        Promise<StatusIconResource> promise =
                mSearchEngineUtils.getSearchEngineLogo(
                        BrandedColorScheme.APP_DEFAULT,
                        mTemplateUrlService);
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), mCallbackCaptor.capture());
        FaviconHelper.FaviconImageCallback faviconCallback = mCallbackCaptor.getValue();
        faviconCallback.onFaviconAvailable(mBitmap, new GURL(LOGO_URL));
        assertEquals(promise.getResult(), expected);

        Promise<StatusIconResource> promise2 =
                mSearchEngineUtils.getSearchEngineLogo(
                        BrandedColorScheme.APP_DEFAULT,
                        mTemplateUrlService);
        assertEquals(promise2.getResult(), expected);

        assertEquals(
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM,
                        SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_SUCCESS_CACHE_HIT));
    }

    @Test
    public void getSearchEngineLogo_nullUrl() {
        StatusIconResource expected =
                SearchEngineUtils.getSearchLoupeResource(BrandedColorScheme.APP_DEFAULT);

        doReturn(null).when(mTemplateUrlService).getUrlForSearchQuery(any());
        Promise<StatusIconResource> promise =
                mSearchEngineUtils.getSearchEngineLogo(
                        BrandedColorScheme.APP_DEFAULT,
                        mTemplateUrlService);

        assertEquals(promise.getResult(), expected);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM,
                        SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineUtils.Events.FETCH_FAILED_NULL_URL));
    }

    @Test
    public void getSearchEngineLogo_faviconHelperError() {
        StatusIconResource expected =
                SearchEngineUtils.getSearchLoupeResource(BrandedColorScheme.APP_DEFAULT);

        when(mFaviconHelper.getLocalFaviconImageForURL(
                        any(), any(), anyInt(), mCallbackCaptor.capture()))
                .thenReturn(false);

        Promise<StatusIconResource> promise =
                mSearchEngineUtils.getSearchEngineLogo(
                        BrandedColorScheme.APP_DEFAULT,
                        mTemplateUrlService);

        assertEquals(promise.getResult(), expected);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM,
                        SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM,
                        SearchEngineUtils.Events.FETCH_FAILED_FAVICON_HELPER_ERROR));
    }

    @Test
    public void getSearchEngineLogo_returnedBitmapNull() {
        StatusIconResource expected =
                SearchEngineUtils.getSearchLoupeResource(BrandedColorScheme.APP_DEFAULT);

        Promise<StatusIconResource> promise =
                mSearchEngineUtils.getSearchEngineLogo(
                        BrandedColorScheme.APP_DEFAULT,
                        mTemplateUrlService);
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), mCallbackCaptor.capture());
        FaviconHelper.FaviconImageCallback faviconCallback = mCallbackCaptor.getValue();
        faviconCallback.onFaviconAvailable(null, new GURL(LOGO_URL));

        assertEquals(promise.getResult(), expected);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM,
                        SearchEngineUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM,
                        SearchEngineUtils.Events.FETCH_FAILED_RETURNED_BITMAP_NULL));
    }

    @Test
    public void getSearchLoupeResource() {
        StatusIconResource expected =
                new StatusIconResource(
                        R.drawable.ic_search, R.color.default_icon_color_white_tint_list);
        Assert.assertEquals(
                expected,
                SearchEngineUtils.getSearchLoupeResource(
                        BrandedColorScheme.DARK_BRANDED_THEME));

        expected =
                new StatusIconResource(
                        R.drawable.ic_search,
                        ThemeUtils.getThemedToolbarIconTintRes(/* useLight= */ true));
        Assert.assertEquals(
                expected,
                SearchEngineUtils.getSearchLoupeResource(BrandedColorScheme.INCOGNITO));
    }

    @Test
    public void needToCheckForSearchEnginePromo_SecurityExceptionThrown() {
        doThrow(SecurityException.class)
                .when(mLocaleManagerDelegate)
                .needToCheckForSearchEnginePromo();

        try {
            mSearchEngineUtils.needToCheckForSearchEnginePromo();
        } catch (Exception e) {
            Assert.fail("No exception should be thrown.");
        }
    }

    @Test
    public void needToCheckForSearchEnginePromo_DeadObjectRuntimeExceptionThrown() {
        doThrow(RuntimeException.class)
                .when(mLocaleManagerDelegate)
                .needToCheckForSearchEnginePromo();

        try {
            mSearchEngineUtils.needToCheckForSearchEnginePromo();
        } catch (Exception e) {
            Assert.fail("No exception should be thrown.");
        }
    }

    @Test
    public void needToCheckForSearchEnginePromo_resultCached() {
        doThrow(RuntimeException.class)
                .when(mLocaleManagerDelegate)
                .needToCheckForSearchEnginePromo();
        assertFalse(mSearchEngineUtils.needToCheckForSearchEnginePromo());

        Mockito.reset(mLocaleManagerDelegate);

        doReturn(true).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();

        assertTrue(mSearchEngineUtils.needToCheckForSearchEnginePromo());

        Mockito.reset(mLocaleManagerDelegate);

        doReturn(false).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();

        assertFalse(mSearchEngineUtils.needToCheckForSearchEnginePromo());
        assertFalse(mSearchEngineUtils.needToCheckForSearchEnginePromo());
        assertFalse(mSearchEngineUtils.needToCheckForSearchEnginePromo());

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
