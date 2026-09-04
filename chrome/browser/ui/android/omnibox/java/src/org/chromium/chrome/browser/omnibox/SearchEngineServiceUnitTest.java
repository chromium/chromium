// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.graphics.Bitmap;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.shadow.api.Shadow;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineService.SearchEngineIconObserver;
import org.chromium.chrome.browser.omnibox.SearchEngineService.SearchEngineNameObserver;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager.JumpStartContext;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.SearchEngineType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.StarterPackId;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;

/** Unit tests for {@link SearchEngineService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchEngineServiceUnitTest {
    private static final String LOGO_URL = "https://www.search.com/";
    private static final String TEMPLATE_URL = "https://www.search.com/search?q={query}";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Captor private ArgumentCaptor<FaviconHelper.FaviconImageCallback> mCallbackCaptor;
    @Captor private ArgumentCaptor<StatusIconResource> mStatusIconCaptor;
    @Mock private FaviconHelper mFaviconHelper;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mTemplateUrl;
    @Mock private Callback<StatusIconResource> mStarterPackCallback;
    @Mock private LocaleManagerDelegate mLocaleManagerDelegate;
    @Mock private Profile mProfile;
    @Mock private SearchEngineNameObserver mHintTextObserver;
    @Mock private SearchEngineIconObserver mEngineIconObserver;
    private Context mContext;
    private Bitmap mBitmap;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mBitmap = Shadow.newInstanceOf(Bitmap.class);
        shadowOf(mBitmap).appendDescription("test");

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        lenient().doReturn(TEMPLATE_URL).when(mTemplateUrl).getURL();
        GURL faviconUrl = new GURL(LOGO_URL);
        lenient().doReturn(faviconUrl).when(mTemplateUrl).getFaviconURL();
        lenient()
                .doReturn(mTemplateUrl)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        lenient().doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        lenient()
                .doReturn(SearchEngineType.SEARCH_ENGINE_OTHER)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl(any());
        lenient()
                .doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), anyBoolean(), any());
        lenient().doReturn(false).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();
        LocaleManager.getInstance().setDelegateForTest(mLocaleManagerDelegate);

        lenient()
                .doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        any(), any(), anyInt(), anyBoolean(), mCallbackCaptor.capture());
    }

    @Test
    public void testDefaultEnabledBehavior() {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        searchEngineService.addSearchEngineNameObserver(mHintTextObserver);

        // Show DSE logo when using regular profile.
        doReturn(false).when(mProfile).isOffTheRecord();
        searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        assertTrue(searchEngineService.shouldShowSearchEngineLogo());

        // Suppress DSE logo when using incognito profile.
        doReturn(true).when(mProfile).isOffTheRecord();
        searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        assertFalse(searchEngineService.shouldShowSearchEngineLogo());

        // Verify observer notified.
        verify(mHintTextObserver).onSearchEngineNameChanged();
    }

    @Test
    public void getSearchEngineLogo() {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        searchEngineService.addIconObserver(mEngineIconObserver);
        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
        clearInvocations(mEngineIconObserver);

        // SearchEngineService retrieves logo when it's first created, and whenever the DSE changes.
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), anyBoolean(), any());
        mCallbackCaptor.getValue().onFaviconAvailable(mBitmap, new GURL(LOGO_URL));

        verify(mEngineIconObserver).onSearchEngineIconChanged(isNotNull());
    }

    @Test
    public void getSearchEngineLogo_nullTemplateUrlService() {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        searchEngineService.addIconObserver(mEngineIconObserver);

        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
    }

    @Test
    public void getSearchEngineLogo_searchEngineGoogle() {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        searchEngineService.addIconObserver(mEngineIconObserver);
        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
        clearInvocations(mEngineIconObserver);

        // Simulate DSE change to Google.
        doReturn(SearchEngineType.SEARCH_ENGINE_GOOGLE)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl(any());
        searchEngineService.onTemplateURLServiceChanged();

        verify(mEngineIconObserver).onSearchEngineIconChanged(mStatusIconCaptor.capture());
        assertEquals(
                mStatusIconCaptor.getValue(),
                new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0));
    }

    private void configureSearchEngine(String keyword, String shortName) {
        doReturn(
                        "google".equals(keyword)
                                ? SearchEngineType.SEARCH_ENGINE_GOOGLE
                                : SearchEngineType.SEARCH_ENGINE_OTHER)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl(any());
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
        assertEquals(UrlConstantResolver.getOriginalNativeNtpUrl(), jumpStartContext.url.getSpec());
        assertEquals(
                PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
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
            new SearchEngineService(mProfile, mFaviconHelper);
            verifyPersistedSearchEngine("google");
            verifyNoSearchEngineSpecificDataInCache();
        }

        {
            // To Non-Google
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("engine", "Some Engine");
            new SearchEngineService(mProfile, mFaviconHelper);
            verifyPersistedSearchEngine("engine");
            verifyNoSearchEngineSpecificDataInCache();
        }
    }

    @Test
    public void onTemplateUrlServiceChanged_newTemplateUrl_withDifferentPreviousEngine() {
        {
            // To Google
            configureSearchEngine("engine", "Some Engine");
            var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("google", "Google");
            searchEngineService.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("google");
            verifyNoSearchEngineSpecificDataInCache();
        }

        {
            // To Non-Google
            configureSearchEngine("google", "Google");
            var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("engine", "Some Engine");
            searchEngineService.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("engine");
            verifyNoSearchEngineSpecificDataInCache();
        }
    }

    @Test
    public void onTemplateUrlServiceChanged_newTemplateUrl_withSamePreviousEngine() {
        {
            // Google to Google
            configureSearchEngine("google", "Google");
            var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
            searchEngineService.addSearchEngineNameObserver(mHintTextObserver);

            // Verify observer notified.
            verify(mHintTextObserver).onSearchEngineNameChanged();
            clearInvocations(mHintTextObserver);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("google", "Google");
            searchEngineService.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("google");
            verifySearchEngineSpecificDataRetainedInCache();

            // Verify observer not notified if engine name didn't change.
            verify(mHintTextObserver, never()).onSearchEngineNameChanged();
        }

        clearInvocations(mHintTextObserver);

        {
            // Non-Google to same non-Google.
            configureSearchEngine("engine", "Some Engine");
            var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
            searchEngineService.addSearchEngineNameObserver(mHintTextObserver);
            clearInvocations(mHintTextObserver);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("engine", "Another Engine");
            searchEngineService.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("engine");
            verifySearchEngineSpecificDataRetainedInCache();

            // Verify observer notified of name change.
            verify(mHintTextObserver).onSearchEngineNameChanged();
        }

        clearInvocations(mHintTextObserver);

        {
            // Non-Google, unnamed engine
            configureSearchEngine("engine", "Some Engine");
            var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
            searchEngineService.addSearchEngineNameObserver(mHintTextObserver);
            clearInvocations(mHintTextObserver);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("engine", null);
            searchEngineService.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("engine");
            verifySearchEngineSpecificDataRetainedInCache();

            // Verify observer notified.
            verify(mHintTextObserver).onSearchEngineNameChanged();
        }

        clearInvocations(mHintTextObserver);

        {
            // Non-Google, unnamed engine
            configureSearchEngine("engine", "Some Engine");
            var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
            searchEngineService.addSearchEngineNameObserver(mHintTextObserver);
            clearInvocations(mHintTextObserver);

            // Make an update to no engine
            doReturn(null).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
            searchEngineService.onTemplateURLServiceChanged();

            // Verify observer notified.
            verify(mHintTextObserver).onSearchEngineNameChanged();
        }
    }

    @Test
    public void getSearchEngineLogo_faviconCached() {
        // Expect only one actual fetch, that happens independently from get request.
        // All get requests always supply cached value.
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        searchEngineService.addIconObserver(mEngineIconObserver);
        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
        clearInvocations(mEngineIconObserver);

        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), anyBoolean(), any());
        mCallbackCaptor.getValue().onFaviconAvailable(mBitmap, new GURL(LOGO_URL));

        verify(mEngineIconObserver).onSearchEngineIconChanged(isNotNull());
    }

    @Test
    public void getSearchEngineLogo_nullUrl() {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        searchEngineService.addIconObserver(mEngineIconObserver);

        // Simulate DSE change - policy blocking searches
        doReturn(null).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        searchEngineService.onTemplateURLServiceChanged();

        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
    }

    @Test
    public void getSearchEngineLogo_faviconHelperError() {
        // Simulate FaviconFetcher failure on the next TemplateUrl change.
        doReturn(false)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        any(), any(), anyInt(), anyBoolean(), mCallbackCaptor.capture());
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        searchEngineService.addIconObserver(mEngineIconObserver);

        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
    }

    @Test
    public void getSearchEngineLogo_returnedBitmapNull() {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        searchEngineService.addIconObserver(mEngineIconObserver);

        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
        clearInvocations(mEngineIconObserver);

        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        any(), any(), anyInt(), anyBoolean(), mCallbackCaptor.capture());
        FaviconHelper.FaviconImageCallback faviconCallback = mCallbackCaptor.getValue();
        faviconCallback.onFaviconAvailable(null, new GURL(LOGO_URL));

        // Not emitting second null icon
        verifyNoMoreInteractions(mEngineIconObserver);
    }

    @Test
    public void needToCheckForSearchEnginePromo_SecurityExceptionThrown() {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        doThrow(SecurityException.class)
                .when(mLocaleManagerDelegate)
                .needToCheckForSearchEnginePromo();

        try {
            searchEngineService.needToCheckForSearchEnginePromo();
        } catch (Exception e) {
            throw new AssertionError("No exception should be thrown.", e);
        }
    }

    @Test
    public void needToCheckForSearchEnginePromo_DeadObjectRuntimeExceptionThrown() {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        doThrow(RuntimeException.class)
                .when(mLocaleManagerDelegate)
                .needToCheckForSearchEnginePromo();

        try {
            searchEngineService.needToCheckForSearchEnginePromo();
        } catch (Exception e) {
            throw new AssertionError("No exception should be thrown.", e);
        }
    }

    @Test
    public void needToCheckForSearchEnginePromo_resultCached() {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        doThrow(RuntimeException.class)
                .when(mLocaleManagerDelegate)
                .needToCheckForSearchEnginePromo();
        assertFalse(searchEngineService.needToCheckForSearchEnginePromo());

        clearInvocations(mLocaleManagerDelegate);

        doReturn(true).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();

        assertTrue(searchEngineService.needToCheckForSearchEnginePromo());

        clearInvocations(mLocaleManagerDelegate);

        doReturn(false).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();

        assertFalse(searchEngineService.needToCheckForSearchEnginePromo());
        assertFalse(searchEngineService.needToCheckForSearchEnginePromo());
        assertFalse(searchEngineService.needToCheckForSearchEnginePromo());

        verify(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();
    }

    @Test
    public void testIsDefaultSearchEngineGoogle() {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        assertTrue(searchEngineService.isDefaultSearchEngineGoogle());

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        assertFalse(searchEngineService.isDefaultSearchEngineGoogle());
    }

    @Test
    public void testRetrieveFavicon_Gemini() {
        checkStarterPackFavicon(StarterPackId.GEMINI, R.drawable.ic_spark_4c_16dp);
    }

    @Test
    public void testRetrieveFavicon_Bookmarks() {
        checkStarterPackFavicon(StarterPackId.BOOKMARKS, R.drawable.ic_star_24dp);
    }

    @Test
    public void testRetrieveFavicon_History() {
        checkStarterPackFavicon(StarterPackId.HISTORY, R.drawable.ic_history_24dp);
    }

    @Test
    public void testRetrieveFavicon_Tabs() {
        checkStarterPackFavicon(StarterPackId.TABS, R.drawable.switch_to_tab);
    }

    private void checkStarterPackFavicon(
            @StarterPackId int starterPackId, int expectedDrawableRes) {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        doReturn(starterPackId).when(mTemplateUrl).getStarterPackId();

        searchEngineService.retrieveFavicon(mTemplateUrl, mStarterPackCallback);

        verify(mStarterPackCallback).onResult(mStatusIconCaptor.capture());
        assertEquals(expectedDrawableRes, mStatusIconCaptor.getValue().getIconRes());
    }

    @Test
    public void testGetNtpHintText_UseShortAskHint_onDesktop() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        configureSearchEngine("google", "Google");
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);

        assertEquals("Ask Google", searchEngineService.getNtpHintText(mContext));
    }

    @Test
    public void testGetNtpHintText_Default_onMobile() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        configureSearchEngine("google", "Google");
        OmniboxFeatures.sUseAskHintForNtp.setForTesting(false);
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);

        assertEquals("Search Google or type URL", searchEngineService.getNtpHintText(mContext));
    }

    @Test
    public void testGetNtpHintText_UseLongAskHint_onMobile() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        configureSearchEngine("google", "Google");
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        OmniboxFeatures.sUseAskHintForNtp.setForTesting(true);
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);

        assertEquals("Ask Google or type URL", searchEngineService.getNtpHintText(mContext));
    }

    @Test
    public void testGetNtpHintText_UseEmptyHint() {
        configureSearchEngine("engine", "");
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);

        assertEquals("Search or type URL", searchEngineService.getNtpHintText(mContext));
    }

    @Test
    public void testGetOmniboxHintString_Default() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        configureSearchEngine("google", "Google");
        OmniboxFeatures.sUseAskHintForNtp.setForTesting(false);
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        assertEquals("Search Google or type URL", searchEngineService.getOmniboxHintString());
    }

    @Test
    public void testGetOmniboxHintString_AskHint() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        configureSearchEngine("google", "Google");
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        OmniboxFeatures.sUseAskHintForNtp.setForTesting(true);
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        assertEquals("Ask Google or type URL", searchEngineService.getOmniboxHintString());
    }

    @Test
    public void testGetOmniboxHintString_onDesktop() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        configureSearchEngine("google", "Google");
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        assertEquals("Ask Google or type URL", searchEngineService.getOmniboxHintString());
    }

    @Test
    public void testGetOmniboxHintString_NonGoogle() {
        configureSearchEngine("yahoo", "Yahoo");
        OmniboxFeatures.sUseAskHintForNtp.setForTesting(true);
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        assertEquals("Search Yahoo or type URL", searchEngineService.getOmniboxHintString());
    }
}
