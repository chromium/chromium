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
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
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
import org.robolectric.shadow.api.Shadow;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineService.SearchEngineIconObserver;
import org.chromium.chrome.browser.omnibox.SearchEngineService.SearchEngineNameObserver;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager.JumpStartContext;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.SearchEngineType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;
import org.chromium.components.search_engines.StarterPackId;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;

/** Unit tests for {@link SearchEngineService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchEngineServiceUnitTest {
    private static final String LOGO_URL = "https://www.search.com/";
    private static final String TEMPLATE_URL = "https://www.search.com/search?q={query}";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

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
    @Mock private FuseboxSessionState mFuseboxSessionState;
    @Mock private AutocompleteInput mAutocompleteInput;
    @Mock private ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;

    private Context mContext;
    private Bitmap mBitmap;

    private final SettableNullableObservableSupplier<InputState> mInputStateSupplier =
            ObservableSuppliers.createNullable();

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
        doReturn(SearchEngineType.SEARCH_ENGINE_OTHER)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl(any());
        doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), anyBoolean(), any());
        doReturn(false).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();
        LocaleManager.getInstance().setDelegateForTest(mLocaleManagerDelegate);

        doReturn(mAutocompleteInput).when(mFuseboxSessionState).getAutocompleteInput();
        doReturn(GURL.emptyGURL()).when(mAutocompleteInput).getPageUrl();
        doReturn("").when(mAutocompleteInput).getPageTitle();

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

        // Verify default placeholder text.
        verify(mHintTextObserver).onSearchEngineNameChanged();
        assertEquals(
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.SEARCH, /* fuseboxSessionState= */ null),
                mContext.getString(R.string.omnibox_empty_hint));
    }

    @Test
    public void getSearchEngineLogo() {
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        searchEngineService.addIconObserver(mEngineIconObserver);
        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
        reset(mEngineIconObserver);

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
        reset(mEngineIconObserver);

        // Simulate DSE change to Google.
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
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
        doReturn("google".equals(keyword)).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
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

            // Verify updated placeholder text.
            verify(mHintTextObserver).onSearchEngineNameChanged();
            assertEquals(
                    "Search Google or type URL",
                    searchEngineService.getOmniboxHintText(
                            AutocompleteRequestType.SEARCH, /* fuseboxSessionState= */ null));
            clearInvocations(mHintTextObserver);

            // Make an update
            saveSearchEngineSpecificDataToCache();
            configureSearchEngine("google", "Google");
            searchEngineService.onTemplateURLServiceChanged();
            verifyPersistedSearchEngine("google");
            verifySearchEngineSpecificDataRetainedInCache();

            // Verify updated placeholder text.
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

            // Verify updated placeholder text.
            verify(mHintTextObserver).onSearchEngineNameChanged();
            assertEquals(
                    "Search Another Engine or type URL",
                    searchEngineService.getOmniboxHintText(
                            AutocompleteRequestType.SEARCH, /* fuseboxSessionState= */ null));
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

            // Verify default placeholder text.
            verify(mHintTextObserver).onSearchEngineNameChanged();
            assertEquals(
                    "Search or type URL",
                    searchEngineService.getOmniboxHintText(
                            AutocompleteRequestType.SEARCH, /* fuseboxSessionState= */ null));
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

            // Verify default placeholder text.
            verify(mHintTextObserver).onSearchEngineNameChanged();
            assertEquals(
                    "Search or type URL",
                    searchEngineService.getOmniboxHintText(
                            AutocompleteRequestType.SEARCH, /* fuseboxSessionState= */ null));
        }
    }

    @Test
    public void getSearchEngineLogo_faviconCached() {
        // Expect only one actual fetch, that happens independently from get request.
        // All get requests always supply cached value.
        var searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        searchEngineService.addIconObserver(mEngineIconObserver);
        verify(mEngineIconObserver).onSearchEngineIconChanged(null);
        reset(mEngineIconObserver);

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
        reset(mEngineIconObserver);

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

        reset(mLocaleManagerDelegate);

        doReturn(true).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();

        assertTrue(searchEngineService.needToCheckForSearchEnginePromo());

        reset(mLocaleManagerDelegate);

        doReturn(false).when(mLocaleManagerDelegate).needToCheckForSearchEnginePromo();

        assertFalse(searchEngineService.needToCheckForSearchEnginePromo());
        assertFalse(searchEngineService.needToCheckForSearchEnginePromo());
        assertFalse(searchEngineService.needToCheckForSearchEnginePromo());

        verify(mLocaleManagerDelegate, times(1)).needToCheckForSearchEnginePromo();
    }

    @Test
    public void testGetOmniboxHintText_ContextualTasks() {
        SearchEngineService searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        GURL aiUrl = new GURL("chrome://contextual-tasks");
        String aiTitle = "My AI Page";

        doReturn(mAutocompleteInput).when(mFuseboxSessionState).getAutocompleteInput();
        doReturn(aiUrl).when(mAutocompleteInput).getPageUrl();
        doReturn(aiTitle).when(mAutocompleteInput).getPageTitle();

        assertEquals(
                aiTitle,
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.SEARCH, mFuseboxSessionState));
    }

    @Test
    public void testGetOmniboxHintText_FuseboxSessionState() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        configureSearchEngine("google", "Google");
        SearchEngineService searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);
        String searchEngineHint = "Search Google or type URL";
        String aiModeHint = "Ask anything";
        String imageGenHint = "Image Gen Tool Hint";
        String deepSearchHint = "Deep Search Tool Hint";
        String canvasHint = "Canvas Tool Hint";
        doReturn(mComposeboxQueryControllerBridge)
                .when(mFuseboxSessionState)
                .getComposeboxQueryControllerBridge();
        doReturn(mInputStateSupplier)
                .when(mComposeboxQueryControllerBridge)
                .getInputStateSupplier();

        ToolConfig aiModeConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_UNSPECIFIED)
                        .setHintText(aiModeHint)
                        .build();
        ToolConfig imageGenConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .setHintText(imageGenHint)
                        .build();
        ToolConfig deepSearchConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .setHintText(deepSearchHint)
                        .build();
        ToolConfig canvasConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_CANVAS)
                        .setHintText(canvasHint)
                        .build();
        byte[][] toolConfigs =
                new byte[][] {
                    aiModeConfig.toByteArray(),
                    imageGenConfig.toByteArray(),
                    deepSearchConfig.toByteArray(),
                    canvasConfig.toByteArray()
                };
        InputState inputState = new InputState.Builder().withToolConfigs(toolConfigs).build();
        mInputStateSupplier.set(inputState);

        assertEquals(
                imageGenHint,
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.IMAGE_GENERATION, mFuseboxSessionState));

        assertEquals(
                deepSearchHint,
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.DEEP_SEARCH, mFuseboxSessionState));

        assertEquals(
                canvasHint,
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.CANVAS, mFuseboxSessionState));

        assertEquals(
                aiModeHint,
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.AI_MODE, mFuseboxSessionState));

        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        assertEquals(
                searchEngineHint,
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.DEEP_SEARCH, mFuseboxSessionState));
        OmniboxFeatures.sShowModelPicker.setForTesting(true);

        assertEquals(
                searchEngineHint,
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.DEEP_SEARCH, /* fuseboxSessionState= */ null));

        doReturn(null).when(mFuseboxSessionState).getComposeboxQueryControllerBridge();
        assertEquals(
                searchEngineHint,
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.DEEP_SEARCH, mFuseboxSessionState));
        doReturn(mComposeboxQueryControllerBridge)
                .when(mFuseboxSessionState)
                .getComposeboxQueryControllerBridge();

        mInputStateSupplier.set(null);
        assertEquals(
                searchEngineHint,
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.DEEP_SEARCH, mFuseboxSessionState));

        InputState emptyHintState = new InputState.Builder().withHintText("").build();
        mInputStateSupplier.set(emptyHintState);
        assertEquals(
                searchEngineHint,
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.DEEP_SEARCH, mFuseboxSessionState));
    }

    @Test
    public void testGetOmniboxHintText_ModelPickerDisabled() {
        configureSearchEngine("google", "Google");
        SearchEngineService searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);

        assertEquals(
                "Search Google or type URL",
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.SEARCH, mFuseboxSessionState));

        assertEquals(
                "Ask anything",
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.AI_MODE, mFuseboxSessionState));

        assertEquals(
                "Describe your image",
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.IMAGE_GENERATION, mFuseboxSessionState));
    }

    @Test
    public void testGetOmniboxHintText_UseAskHintForNtp() {
        SearchEngineService searchEngineService = new SearchEngineService(mProfile, mFaviconHelper);

        // Case 1: Feature disabled
        OmniboxFeatures.sUseAskHintForNtp.setForTesting(false);
        configureSearchEngine("google", "Google");
        searchEngineService.onTemplateURLServiceChanged();
        assertEquals(
                "Search Google or type URL",
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.SEARCH, /* fuseboxSessionState= */ null));

        // Case 2: Feature enabled, Search Engine is Google
        OmniboxFeatures.sUseAskHintForNtp.setForTesting(true);
        configureSearchEngine("google", "Google");
        searchEngineService.onTemplateURLServiceChanged();
        assertEquals(
                "Ask Google or type URL",
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.SEARCH, /* fuseboxSessionState= */ null));

        // Case 3: Feature enabled, Search Engine is NOT Google
        OmniboxFeatures.sUseAskHintForNtp.setForTesting(true);
        configureSearchEngine("yahoo", "Yahoo");
        searchEngineService.onTemplateURLServiceChanged();
        assertEquals(
                "Search Yahoo or type URL",
                searchEngineService.getOmniboxHintText(
                        AutocompleteRequestType.SEARCH, /* fuseboxSessionState= */ null));

        // Reset for testing
        OmniboxFeatures.sUseAskHintForNtp.setForTesting(false);
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
}
