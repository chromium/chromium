// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.SearchEngineType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.search_engines.SearchEngineAssets.SearchEngineIcon;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;
import java.util.function.IntFunction;

/** Unit tests for {@link SearchEngineAssets}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchEngineAssetsUnitTest {
    private static final int DESIRED_SIZE_PX = 16;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LargeIconBridge mLargeIconBridge;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private Callback<@Nullable SearchEngineIcon> mCallback;
    @Mock private TemplateUrl mTemplateUrl;
    @Mock private Bitmap mBitmap;
    @Mock private Drawable mDrawable;
    @Mock private IntFunction<Drawable> mDrawableLoader;

    @Captor private ArgumentCaptor<LargeIconCallback> mLargeIconCallbackCaptor;
    @Captor private ArgumentCaptor<@Nullable SearchEngineIcon> mIconCaptor;

    private Context mContext;
    private SearchEngineAssets mSearchEngineAssets;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mSearchEngineAssets = new SearchEngineAssets(mLargeIconBridge);
        lenient().doReturn("http://example.com/search?q=test").when(mTemplateUrl).getURL();
        lenient()
                .doReturn(new GURL("http://example.com/favicon.ico"))
                .when(mTemplateUrl)
                .getFaviconURL();
        lenient().doReturn("example.com").when(mTemplateUrl).getKeyword();
        lenient().doReturn(StarterPackId.NONE).when(mTemplateUrl).getStarterPackId();
    }

    @Test
    public void testRetrieveFavicon_GoogleDSE() {
        doReturn(SearchEngineType.SEARCH_ENGINE_GOOGLE)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl("example.com");

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);
        verify(mLargeIconBridge, never()).getLargeIconForUrl(any(), anyInt(), anyInt(), any());

        verify(mCallback).onResult(mIconCaptor.capture());
        SearchEngineIcon icon = mIconCaptor.getValue();
        assertEquals(
                SearchEngineIcon.fromDrawableRes(
                        R.drawable.ic_logo_googleg_20dp, /* isMonochrome= */ false),
                icon);
        assertFalse(icon.isMonochrome());
        assertEquals(
                R.drawable.ic_logo_googleg_20dp,
                Shadows.shadowOf(icon.toDrawable(mContext)).getCreatedFromResId());
    }

    @Test
    public void testRetrieveFavicon_StarterPack_Bookmarks() {
        verifyStarterPackFavicon(StarterPackId.BOOKMARKS, R.drawable.ic_star_24dp);
    }

    @Test
    public void testRetrieveFavicon_StarterPack_History() {
        verifyStarterPackFavicon(StarterPackId.HISTORY, R.drawable.ic_history_24dp);
    }

    @Test
    public void testRetrieveFavicon_StarterPack_Tabs() {
        verifyStarterPackFavicon(StarterPackId.TABS, R.drawable.switch_to_tab);
    }

    @Test
    public void testRetrieveFavicon_StarterPack_Gemini() {
        verifyStarterPackFavicon(StarterPackId.GEMINI, R.drawable.ic_spark_24dp);
    }

    @Test
    public void testRetrieveFavicon_StarterPack_AiMode() {
        verifyStarterPackFavicon(StarterPackId.AI_MODE, R.drawable.search_spark);
    }

    @Test
    public void testGetStarterPackDrawableRes_None() {
        assertEquals(
                Resources.ID_NULL,
                SearchEngineAssets.getStarterPackDrawableRes(StarterPackId.NONE));
    }

    @Test
    public void testRetrieveFavicon_LargeIconBridgeSuccess() {
        doReturn(SearchEngineType.SEARCH_ENGINE_OTHER)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl("example.com");
        doReturn(true)
                .when(mLargeIconBridge)
                .getLargeIconForUrl(
                        eq(new GURL("http://example.com/")),
                        eq(1),
                        eq(DESIRED_SIZE_PX),
                        mLargeIconCallbackCaptor.capture());

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);

        mLargeIconCallbackCaptor.getValue().onLargeIconAvailable(mBitmap, 0, false, 0);

        verify(mCallback).onResult(mIconCaptor.capture());
        SearchEngineIcon icon = mIconCaptor.getValue();
        assertEquals(SearchEngineIcon.fromBitmap(mBitmap), icon);
        assertFalse(icon.isMonochrome());
        Drawable drawable = icon.toDrawable(mContext);
        assertTrue(drawable instanceof BitmapDrawable);
        assertEquals(mBitmap, ((BitmapDrawable) drawable).getBitmap());
    }

    @Test
    public void testRetrieveFavicon_LargeIconBridgeReturnsNull() {
        doReturn(SearchEngineType.SEARCH_ENGINE_OTHER)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl("example.com");
        doReturn(true)
                .when(mLargeIconBridge)
                .getLargeIconForUrl(
                        eq(new GURL("http://example.com/")),
                        eq(1),
                        eq(DESIRED_SIZE_PX),
                        mLargeIconCallbackCaptor.capture());

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);

        mLargeIconCallbackCaptor.getValue().onLargeIconAvailable(null, 0, false, 0);

        verify(mCallback).onResult(null);
    }

    @Test
    public void testRetrieveFavicon_LargeIconBridgeWillNotCall() {
        doReturn(SearchEngineType.SEARCH_ENGINE_OTHER)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl("example.com");
        doReturn(false)
                .when(mLargeIconBridge)
                .getLargeIconForUrl(
                        eq(new GURL("http://example.com/")), eq(1), eq(DESIRED_SIZE_PX), any());

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);

        verify(mCallback).onResult(null);
    }

    @Test
    public void testGetDefaultFavicon() {
        SearchEngineIcon defaultIcon = SearchEngineAssets.getDefaultFavicon();
        assertEquals(
                SearchEngineIcon.fromDrawableRes(
                        R.drawable.ic_search_24dp, /* isMonochrome= */ true),
                defaultIcon);
        assertTrue(defaultIcon.isMonochrome());
        assertEquals(
                R.drawable.ic_search_24dp,
                Shadows.shadowOf(defaultIcon.toDrawable(mContext)).getCreatedFromResId());
    }

    @Test
    public void testIconEquality() {
        SearchEngineIcon resIcon1 =
                SearchEngineIcon.fromDrawableRes(R.drawable.ic_star_24dp, /* isMonochrome= */ true);
        SearchEngineIcon resIcon2 =
                SearchEngineIcon.fromDrawableRes(R.drawable.ic_star_24dp, /* isMonochrome= */ true);
        SearchEngineIcon resIconNonMono =
                SearchEngineIcon.fromDrawableRes(
                        R.drawable.ic_star_24dp, /* isMonochrome= */ false);
        SearchEngineIcon bitmapIcon1 = SearchEngineIcon.fromBitmap(mBitmap);
        SearchEngineIcon bitmapIcon2 = SearchEngineIcon.fromBitmap(mBitmap);

        assertEquals(resIcon1, resIcon2);
        assertEquals(resIcon1.hashCode(), resIcon2.hashCode());
        assertNotEquals(resIcon1, resIconNonMono);
        assertEquals(bitmapIcon1, bitmapIcon2);
        assertEquals(bitmapIcon1.hashCode(), bitmapIcon2.hashCode());
        assertNotEquals(resIcon1, bitmapIcon1);
    }

    @Test
    public void testRetrieveFavicon_BrandedBuiltInIconDecodedOffUiThreadAndCached() {
        SearchEngineAssets.setIsChromeBrandedForTesting(true);
        doReturn(createTestPngBytes()).when(mTemplateUrl).getBuiltInSearchEngineIconPngBytes();

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);
        verify(mCallback, never()).onResult(any());

        RobolectricUtil.runAllBackgroundAndUi();
        verify(mCallback).onResult(mIconCaptor.capture());
        SearchEngineIcon firstIcon = mIconCaptor.getValue();
        assertNotNull(firstIcon);
        assertFalse(firstIcon.isMonochrome());
        assertTrue(firstIcon.toDrawable(mContext) instanceof BitmapDrawable);

        clearInvocations(mTemplateUrl, mCallback);
        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);
        verify(mTemplateUrl, never()).getBuiltInSearchEngineIconPngBytes();
        verify(mCallback).onResult(firstIcon);
    }

    @Test
    public void testRetrieveFavicon_BrandedBuiltInIconIgnoredAfterDestroy() {
        SearchEngineAssets.setIsChromeBrandedForTesting(true);
        doReturn(createTestPngBytes()).when(mTemplateUrl).getBuiltInSearchEngineIconPngBytes();

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);
        mSearchEngineAssets.destroy();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback, never()).onResult(any());
    }

    @Test
    public void testToDrawable_usesCustomDrawableLoader() {
        doReturn(mDrawable).when(mDrawableLoader).apply(R.drawable.ic_star_24dp);
        SearchEngineIcon resIcon =
                SearchEngineIcon.fromDrawableRes(R.drawable.ic_star_24dp, /* isMonochrome= */ true);
        assertSame(mDrawable, resIcon.toDrawable(mContext, mDrawableLoader));

        SearchEngineIcon bitmapIcon = SearchEngineIcon.fromBitmap(mBitmap);
        Drawable drawable = bitmapIcon.toDrawable(mContext, mDrawableLoader);
        assertTrue(drawable instanceof BitmapDrawable);
        assertEquals(mBitmap, ((BitmapDrawable) drawable).getBitmap());
        verify(mDrawableLoader, never()).apply(Resources.ID_NULL);
    }

    @Test
    public void testDestroy() {
        mSearchEngineAssets.destroy();
        verify(mLargeIconBridge).destroy();
    }

    private void verifyStarterPackFavicon(
            @StarterPackId int starterPackId, @DrawableRes int expectedDrawableRes) {
        doReturn(starterPackId).when(mTemplateUrl).getStarterPackId();

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);
        verify(mLargeIconBridge, never()).getLargeIconForUrl(any(), anyInt(), anyInt(), any());

        verify(mCallback).onResult(mIconCaptor.capture());
        SearchEngineIcon icon = mIconCaptor.getValue();
        assertEquals(
                SearchEngineIcon.fromDrawableRes(expectedDrawableRes, /* isMonochrome= */ true),
                icon);
        assertTrue(icon.isMonochrome());
        assertEquals(
                expectedDrawableRes,
                Shadows.shadowOf(icon.toDrawable(mContext)).getCreatedFromResId());
    }

    private static byte[] createTestPngBytes() {
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
        return out.toByteArray();
    }
}
