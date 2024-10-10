// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Rect;
import android.text.TextUtils;
import android.util.Property;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate.AutocompleteLoadCallback;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridgeJni;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.content_public.common.ResourceRequestBodyJni;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for LocationBarMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        shadows = {
            LocationBarMediatorTest.ShadowUrlUtilities.class,
            LocationBarMediatorTest.ShadowGeolocationHeader.class,
            LocationBarMediatorTest.ObjectAnimatorShadow.class
        })
@DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
public class LocationBarMediatorTest {
    @Implements(UrlUtilities.class)
    static class ShadowUrlUtilities {
        static boolean sIsNtp;

        @Implementation
        public static boolean isNtpUrl(GURL url) {
            return sIsNtp;
        }

        @Implementation
        public static boolean isNtpUrl(String url) {
            return sIsNtp;
        }
    }

    @Implements(GeolocationHeader.class)
    static class ShadowGeolocationHeader {
        @Implementation
        public static void primeLocationForGeoHeaderIfEnabled(
                Profile profile, TemplateUrlService templateService) {
            sGeoHeaderPrimeCount++;
        }

        @Implementation
        public static void stopListeningForLocationUpdates() {
            sGeoHeaderStopCount++;
        }
    }

    @Implements(ObjectAnimator.class)
    static class ObjectAnimatorShadow {
        private static ObjectAnimator sUrlAnimator;

        @Implementation
        public static <T> ObjectAnimator ofFloat(
                T target, Property<T, Float> property, float... values) {
            return sUrlAnimator;
        }

        static void setUrlAnimator(ObjectAnimator objectAnimator) {
            sUrlAnimator = objectAnimator;
        }
    }

    private static final String TEST_URL = "http://www.example.org";

    private static int sGeoHeaderPrimeCount;
    private static int sGeoHeaderStopCount;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private LocationBarLayout mLocationBarLayout;
    @Mock private LocationBarTablet mLocationBarTablet;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private OverrideUrlLoadingDelegate mOverrideUrlLoadingDelegate;
    @Mock private LocaleManager mLocaleManager;
    @Mock private UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock private Tab mTab;
    @Mock private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private AutocompleteCoordinator mAutocompleteCoordinator;
    @Mock private UrlBarCoordinator mUrlCoordinator;
    @Mock private StatusCoordinator mStatusCoordinator;
    @Mock private OmniboxPrerender.Natives mPrerenderJni;
    @Mock private TextView mView;
    @Mock private KeyEvent mKeyEvent;
    @Mock private KeyEvent.DispatcherState mKeyDispatcherState;
    @Mock private BackKeyBehaviorDelegate mOverrideBackKeyBehaviorDelegate;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ObjectAnimator mUrlAnimator;
    @Mock private View mRootView;
    @Mock private SearchEngineUtils mSearchEngineUtils;
    @Mock private AutocompleteLoadCallback mAutocompleteLoadCallback;
    @Mock private LoadUrlParams mLoadUrlParams;
    @Mock private LoadUrlResult mLoadUrlResult;

    @Mock private LensController mLensController;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private Profile mProfile;
    @Mock private PreloadPagesSettingsBridge.Natives mPreloadPagesSettingsJni;
    @Mock private LocationBarMediator.OmniboxUma mOmniboxUma;
    @Mock private OmniboxSuggestionsDropdownEmbedderImpl mEmbedderImpl;
    @Mock private ResourceRequestBody.Natives mResourceRequestBodyJni;

    @Captor private ArgumentCaptor<Runnable> mRunnableCaptor;
    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private Context mContext;
    private ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    private LocationBarMediator mMediator;
    private LocationBarMediator mTabletMediator;
    private UrlBarData mUrlBarData;
    private boolean mIsToolbarMicEnabled;
    private LocationBarEmbedderUiOverrides mUiOverrides;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mUrlBarData = UrlBarData.create(null, "text", 0, 0, "text");
        doReturn(true).when(mSearchEngineUtils).shouldShowSearchEngineLogo();
        SearchEngineUtils.setInstanceForTesting(mSearchEngineUtils);
        doReturn(mUrlBarData).when(mLocationBarDataProvider).getUrlBarData();
        doReturn(mTabModelSelector).when(mTabModelSelectorSupplier).get();
        doReturn(mRootView).when(mLocationBarLayout).getRootView();
        doReturn(mRootView).when(mLocationBarTablet).getRootView();
        doReturn(new WeakReference<Activity>(null)).when(mWindowAndroid).getActivity();
        mJniMocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
        mJniMocker.mock(OmniboxPrerenderJni.TEST_HOOKS, mPrerenderJni);
        mJniMocker.mock(PreloadPagesSettingsBridgeJni.TEST_HOOKS, mPreloadPagesSettingsJni);
        mJniMocker.mock(ResourceRequestBodyJni.TEST_HOOKS, mResourceRequestBodyJni);
        doReturn(mProfile).when(mTab).getProfile();
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        OneshotSupplierImpl<TemplateUrlService> templateUrlServiceSupplier =
                new OneshotSupplierImpl<>();
        templateUrlServiceSupplier.set(mTemplateUrlService);
        mUiOverrides = new LocationBarEmbedderUiOverrides();
        mMediator =
                new LocationBarMediator(
                        mContext,
                        mLocationBarLayout,
                        mLocationBarDataProvider,
                        mUiOverrides,
                        mProfileSupplier,
                        mOverrideUrlLoadingDelegate,
                        mLocaleManager,
                        templateUrlServiceSupplier,
                        mOverrideBackKeyBehaviorDelegate,
                        mWindowAndroid,
                        /* isTablet= */ false,
                        mLensController,
                        tab -> true,
                        mOmniboxUma,
                        () -> mIsToolbarMicEnabled,
                        mEmbedderImpl,
                        mTabModelSelectorSupplier);
        mMediator.setCoordinators(mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);
        ObjectAnimatorShadow.setUrlAnimator(mUrlAnimator);

        mTabletMediator =
                new LocationBarMediator(
                        mContext,
                        mLocationBarTablet,
                        mLocationBarDataProvider,
                        mUiOverrides,
                        mProfileSupplier,
                        mOverrideUrlLoadingDelegate,
                        mLocaleManager,
                        templateUrlServiceSupplier,
                        mOverrideBackKeyBehaviorDelegate,
                        mWindowAndroid,
                        /* isTablet= */ true,
                        mLensController,
                        tab -> true,
                        (tab, transition, isNtp) -> {},
                        () -> mIsToolbarMicEnabled,
                        mEmbedderImpl,
                        mTabModelSelectorSupplier);
        mTabletMediator.setCoordinators(
                mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);
        ShadowUrlUtilities.sIsNtp = false;
        sGeoHeaderPrimeCount = 0;
        sGeoHeaderStopCount = 0;
    }

    @Test
    public void testGetVoiceRecognitionHandler_safeToCallAfterDestroy() {
        mMediator.onFinishNativeInitialization();
        mMediator.destroy();
        mMediator.getVoiceRecognitionHandler();
    }

    @Test
    public void testOnTabLoadingNtp() {
        mMediator.onNtpStartedLoading();
        verify(mLocationBarLayout).onNtpStartedLoading();
    }

    @Test
    public void testRevertChanges_focused() {
        mMediator.onUrlFocusChange(true);
        UrlBarData urlBarData = mock(UrlBarData.class);
        doReturn(urlBarData).when(mLocationBarDataProvider).getUrlBarData();
        mMediator.revertChanges();
        verify(mUrlCoordinator)
                .setUrlBarData(urlBarData, UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_ALL);
    }

    @Test
    public void testRevertChanges_focusedNativePage() {
        doReturn(JUnitTestGURLs.NTP_URL).when(mLocationBarDataProvider).getCurrentGurl();
        mMediator.onUrlFocusChange(true);
        mMediator.revertChanges();
        verify(mUrlCoordinator)
                .setUrlBarData(
                        UrlBarData.EMPTY,
                        UrlBar.ScrollType.SCROLL_TO_BEGINNING,
                        SelectionState.SELECT_ALL);
    }

    @Test
    public void testRevertChanges_unFocused() {
        doReturn(JUnitTestGURLs.BLUE_1).when(mLocationBarDataProvider).getCurrentGurl();
        mMediator.revertChanges();
        verify(mUrlCoordinator)
                .setUrlBarData(
                        mLocationBarDataProvider.getUrlBarData(),
                        UrlBar.ScrollType.SCROLL_TO_TLD,
                        SelectionState.SELECT_ALL);
    }

    @Test
    public void testOnSuggestionsChanged() {
        ArgumentCaptor<OmniboxPrerender> omniboxPrerenderCaptor =
                ArgumentCaptor.forClass(OmniboxPrerender.class);
        doReturn(123L).when(mPrerenderJni).init(omniboxPrerenderCaptor.capture());
        mMediator.onFinishNativeInitialization();
        Profile profile = mock(Profile.class);
        mProfileSupplier.set(profile);
        verify(mPrerenderJni)
                .initializeForProfile(123L, omniboxPrerenderCaptor.getValue(), profile);

        doReturn(PreloadPagesState.NO_PRELOADING)
                .when(mPreloadPagesSettingsJni)
                .getState(eq(profile));
        mMediator.onSuggestionsChanged(
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("text")
                        .setIsSearch(true)
                        .setAllowedToBeDefaultMatch(true)
                        .build());
        verify(mPrerenderJni, never())
                .prerenderMaybe(
                        anyLong(), any(), anyString(), anyString(), anyLong(), any(), any());
        verify(mStatusCoordinator).onDefaultMatchClassified(true);

        doReturn(PreloadPagesState.STANDARD_PRELOADING)
                .when(mPreloadPagesSettingsJni)
                .getState(eq(profile));
        GURL url = JUnitTestGURLs.RED_1;
        mMediator.setUrl(url, null);
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(456L).when(mAutocompleteCoordinator).getCurrentNativeAutocompleteResult();
        doReturn("text").when(mUrlCoordinator).getTextWithoutAutocomplete();
        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mMediator.onUrlFocusChange(true);

        mMediator.onSuggestionsChanged(
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("text")
                        .setInlineAutocompletion("textWithAutocomplete")
                        .setAdditionalText("additionalText")
                        .setIsSearch(false)
                        .setAllowedToBeDefaultMatch(true)
                        .build());
        verify(mPrerenderJni)
                .prerenderMaybe(
                        123L,
                        omniboxPrerenderCaptor.getValue(),
                        "text",
                        JUnitTestGURLs.RED_1.getSpec(),
                        456L,
                        profile,
                        mTab);
        verify(mStatusCoordinator).onDefaultMatchClassified(false);
        verify(mUrlCoordinator)
                .setAutocompleteText("text", "textWithAutocomplete", "additionalText");
    }

    @Test
    public void testOnSuggestionsChanged_nullMatch() {
        doReturn("text").when(mUrlCoordinator).getTextWithoutAutocomplete();
        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();

        mMediator.onSuggestionsChanged(null);
        verify(mStatusCoordinator).onDefaultMatchClassified(true);
        verify(mUrlCoordinator).setAutocompleteText("text", null, null);
    }

    @Test
    public void testLoadUrl() {
        mMediator.onFinishNativeInitialization();

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(false)
                        .build());

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
    }

    @Test
    public void testLoadUrlWithAutocompleteLoadCallback() {
        mMediator.onFinishNativeInitialization();

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(false)
                        .setAutocompleteLoadCallback(mAutocompleteLoadCallback)
                        .build());

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor.getValue().onLoadUrl(mTab, mLoadUrlParams, mLoadUrlResult);
        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
        verify(mAutocompleteLoadCallback).onLoadUrl(mLoadUrlParams, mLoadUrlResult);
    }

    @Test
    public void testLoadUrlWithPostData() {
        mMediator.onFinishNativeInitialization();
        String text = "text";
        byte[] data = new byte[] {0, 1, 2, 3, 4};

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(data).when(mResourceRequestBodyJni).createResourceRequestBodyFromBytes(any());
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setpostDataAndType(data, text)
                        .build());

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
        assertTrue(mLoadUrlParamsCaptor.getValue().getVerbatimHeaders().contains(text));
        assertEquals(data, mLoadUrlParamsCaptor.getValue().getPostData().getEncodedNativeForm());
    }

    @Test
    public void testLoadUrl_NativeNotInitialized() {
        if (BuildConfig.ENABLE_ASSERTS) {
            try {
                mMediator.loadUrl(
                        new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                                .setOpenInNewTab(false)
                                .build());
                throw new Error("Expected an assert to be triggered.");
            } catch (AssertionError e) {
            }
        }
    }

    @Test
    public void testLoadUrl_OverrideLoadingDelegate() {
        mMediator.onFinishNativeInitialization();

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        ArgumentCaptor<OmniboxLoadUrlParams> captor =
                ArgumentCaptor.forClass(OmniboxLoadUrlParams.class);
        doReturn(true)
                .when(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(any(), anyBoolean());
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(false)
                        .build());

        verify(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(captor.capture(), anyBoolean());

        var params = captor.getValue();
        assertEquals(TEST_URL, params.url);
        assertEquals(PageTransition.TYPED, params.transitionType);
        assertEquals(0, params.inputStartTimestamp);
        assertNull(null, params.postData);
        assertNull(null, params.postDataType);
        assertFalse(params.openInNewTab);
        verify(mTab, times(0)).loadUrl(any());
    }

    @Test
    public void testLoadUrl_openInNewTab() {
        mMediator.onFinishNativeInitialization();

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(false).when(mTab).isIncognito();
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(true)
                        .build());

        verify(mTabModelSelector)
                .openNewTab(
                        mLoadUrlParamsCaptor.capture(),
                        eq(TabLaunchType.FROM_OMNIBOX),
                        eq(mTab),
                        eq(false));
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
    }

    @Test
    public void testAllowKeyboardLearning() {
        doReturn(false).when(mLocationBarDataProvider).isOffTheRecord();
        assertTrue(mMediator.allowKeyboardLearning());

        doReturn(true).when(mLocationBarDataProvider).isOffTheRecord();
        assertFalse(mMediator.allowKeyboardLearning());
    }

    @Test
    public void testGetViewForUrlBackFocus() {
        doReturn(mView).when(mTab).getView();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        assertEquals(mView, mMediator.getViewForUrlBackFocus());
        verify(mTab).getView();

        doReturn(null).when(mLocationBarDataProvider).getTab();
        assertNull(mMediator.getViewForUrlBackFocus());
        verify(mLocationBarDataProvider, times(3)).getTab();
        verify(mTab, times(1)).getView();
    }

    @Test
    public void testSetSearchQuery() {
        String query = "example search";
        mMediator.onFinishNativeInitialization();
        mMediator.setSearchQuery(query);

        verify(mUrlCoordinator).requestFocus();
        verify(mUrlCoordinator)
                .setUrlBarData(
                        argThat(matchesUrlBarDataForQuery(query)),
                        eq(UrlBar.ScrollType.NO_SCROLL),
                        eq(SelectionState.SELECT_ALL));
        verify(mAutocompleteCoordinator).startAutocompleteForQuery(query);
        verify(mUrlCoordinator).setKeyboardVisibility(true, false);
    }

    @Test
    public void testSetSearchQuery_empty() {
        mMediator.setSearchQuery("");
        verify(mUrlCoordinator, never()).requestFocus();
        verify(mLocationBarLayout, never()).post(any());

        mMediator.onFinishNativeInitialization();
        verify(mUrlCoordinator, never()).requestFocus();

        mMediator.setSearchQuery("");
        verify(mUrlCoordinator, never()).requestFocus();
        verify(mLocationBarLayout, never()).post(any());
    }

    @Test
    public void testSetSearchQuery_preNative() {
        String query = "example search";
        mMediator.setSearchQuery(query);
        mMediator.onFinishNativeInitialization();

        verify(mLocationBarLayout).post(mRunnableCaptor.capture());
        mRunnableCaptor.getValue().run();

        verify(mUrlCoordinator).requestFocus();
        verify(mUrlCoordinator)
                .setUrlBarData(
                        argThat(matchesUrlBarDataForQuery(query)),
                        eq(UrlBar.ScrollType.NO_SCROLL),
                        eq(SelectionState.SELECT_ALL));
        verify(mAutocompleteCoordinator).startAutocompleteForQuery(query);
        verify(mUrlCoordinator).setKeyboardVisibility(true, false);
    }

    @Test
    public void testPerformSearchQuery() {
        mMediator.onFinishNativeInitialization();
        String query = "example search";
        List<String> params = Arrays.asList("param 1", "param 2");
        doReturn("http://www.search.com")
                .when(mTemplateUrlService)
                .getUrlForSearchQuery("example search", params);
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mMediator.performSearchQuery(query, params);

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals("http://www.search.com", mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.GENERATED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
    }

    @Test
    public void testPerformSearchQuery_empty() {
        mMediator.performSearchQuery("", Collections.emptyList());
        verify(mUrlCoordinator, never()).requestFocus();
        verify(mLocationBarLayout, never()).post(any());

        mMediator.onFinishNativeInitialization();
        verify(mUrlCoordinator, never()).requestFocus();

        mMediator.setSearchQuery("");
        verify(mUrlCoordinator, never()).requestFocus();
        verify(mLocationBarLayout, never()).post(any());
    }

    @Test
    public void testPerformSearchQuery_emptyUrl() {
        String query = "example search";
        List<String> params = Arrays.asList("param 1", "param 2");
        mMediator.onFinishNativeInitialization();
        doReturn("").when(mTemplateUrlService).getUrlForSearchQuery("example search", params);
        mMediator.performSearchQuery(query, params);

        verify(mUrlCoordinator).requestFocus();
        verify(mUrlCoordinator)
                .setUrlBarData(
                        argThat(matchesUrlBarDataForQuery(query)),
                        eq(UrlBar.ScrollType.NO_SCROLL),
                        eq(SelectionState.SELECT_ALL));
        verify(mAutocompleteCoordinator).startAutocompleteForQuery(query);
        verify(mUrlCoordinator).setKeyboardVisibility(true, false);
    }

    @Test
    public void testOnConfigurationChanged_qwertyKeyboard() {
        mMediator.onUrlFocusChange(true);
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        Configuration newConfig = new Configuration();
        newConfig.keyboard = Configuration.KEYBOARD_QWERTY;
        mMediator.onConfigurationChanged(newConfig);

        verify(mUrlCoordinator, never()).clearFocus();
    }

    @Test
    public void testOnConfigurationChanged_nonQwertyKeyboard() {
        Configuration newConfig = new Configuration();
        newConfig.keyboard = Configuration.KEYBOARD_NOKEYS;
        mMediator.onConfigurationChanged(newConfig);
        verify(mUrlCoordinator, never()).clearFocus();

        mMediator.onUrlFocusChange(true);
        mMediator.onConfigurationChanged(newConfig);
        verify(mUrlCoordinator, never()).clearFocus();

        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mMediator.onConfigurationChanged(newConfig);
        verify(mUrlCoordinator).clearFocus();
    }

    // KEYCODE_BACK will not be sent from Android OS starting from T. And no feature should
    // rely on KEYCODE_BACK to intercept back press.
    @Test
    public void testOnKey_autocompleteHandles() {
        doReturn(false)
                .when(mAutocompleteCoordinator)
                .handleKeyEvent(KeyEvent.KEYCODE_BACK, mKeyEvent);
        mMediator.onKey(mView, KeyEvent.KEYCODE_BACK, mKeyEvent);
        // No-op.
        verify(mAutocompleteCoordinator).handleKeyEvent(KeyEvent.KEYCODE_BACK, mKeyEvent);
    }

    @Test
    public void testOnKey_back() {
        doReturn(mKeyDispatcherState).when(mLocationBarLayout).getKeyDispatcherState();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        assertFalse(mMediator.onKey(mView, KeyEvent.KEYCODE_BACK, mKeyEvent));

        verify(mKeyDispatcherState, never()).startTracking(mKeyEvent, mMediator);

        doReturn(false).when(mKeyEvent).isTracking();
        doReturn(KeyEvent.ACTION_UP).when(mKeyEvent).getAction();

        assertFalse(mMediator.onKey(mView, KeyEvent.KEYCODE_BACK, mKeyEvent));

        verify(mKeyDispatcherState, never().description("Should not handle KEYCODE_BACK"))
                .handleUpEvent(mKeyEvent);
        verify(
                        mOverrideBackKeyBehaviorDelegate,
                        never().description("should not handle KEYCODE_BACK"))
                .handleBackKeyPressed();
    }

    @Test
    public void testOnKey_escape() {
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        assertTrue(mMediator.onKey(mView, KeyEvent.KEYCODE_ESCAPE, mKeyEvent));
        verify(mUrlCoordinator)
                .setUrlBarData(
                        mLocationBarDataProvider.getUrlBarData(),
                        UrlBar.ScrollType.SCROLL_TO_TLD,
                        SelectionState.SELECT_ALL);
    }

    @Test
    public void testOnKey_right() {
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(KeyEvent.KEYCODE_DPAD_RIGHT).when(mKeyEvent).getKeyCode();
        doReturn("a").when(mView).getText();
        doReturn(0).when(mView).getSelectionStart();
        doReturn(1).when(mView).getSelectionEnd();

        assertFalse(mMediator.onKey(mView, KeyEvent.KEYCODE_DPAD_RIGHT, mKeyEvent));

        doReturn(1).when(mView).getSelectionStart();
        assertTrue(mMediator.onKey(mView, KeyEvent.KEYCODE_DPAD_RIGHT, mKeyEvent));
    }

    @Test
    public void testOnKey_leftRtl() {
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(KeyEvent.KEYCODE_DPAD_LEFT).when(mKeyEvent).getKeyCode();
        doReturn("a").when(mView).getText();
        doReturn(0).when(mView).getSelectionStart();
        doReturn(1).when(mView).getSelectionEnd();
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mView).getLayoutDirection();

        assertFalse(mMediator.onKey(mView, KeyEvent.KEYCODE_DPAD_LEFT, mKeyEvent));

        doReturn(1).when(mView).getSelectionStart();
        assertTrue(mMediator.onKey(mView, KeyEvent.KEYCODE_DPAD_LEFT, mKeyEvent));
    }

    @Test
    public void testOnKey_unhandled() {
        doReturn(KeyEvent.KEYCODE_BUTTON_14).when(mKeyEvent).getAction();
        assertFalse(mMediator.onKey(mView, KeyEvent.KEYCODE_BACK, mKeyEvent));
    }

    @Test
    public void testHandleTypingStarted_triggersFocusAnimation() {
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.onUrlFocusChange(true);
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);

        mMediator.completeUrlFocusAnimationAndEnableSuggestions();

        verify(mUrlCoordinator, times(2)).onUrlFocusChange(true);
    }

    @Test
    public void testUpdateColors_lightBrandedColor() {
        doReturn(Color.parseColor("#eaecf0" /*Light grey color*/))
                .when(mLocationBarDataProvider)
                .getPrimaryColor();
        doReturn(false).when(mLocationBarDataProvider).isIncognito();

        mMediator.updateBrandedColorScheme();

        verify(mLocationBarLayout).setDeleteButtonTint(any(ColorStateList.class));
        verify(mStatusCoordinator).setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        verify(mAutocompleteCoordinator)
                .updateVisualsForState(BrandedColorScheme.LIGHT_BRANDED_THEME);
    }

    @Test
    public void testUpdateColors_darkBrandedColor() {
        doReturn(Color.BLACK).when(mLocationBarDataProvider).getPrimaryColor();
        doReturn(false).when(mLocationBarDataProvider).isIncognito();

        mMediator.updateBrandedColorScheme();

        verify(mLocationBarLayout).setDeleteButtonTint(any(ColorStateList.class));
        verify(mStatusCoordinator).setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        verify(mAutocompleteCoordinator)
                .updateVisualsForState(BrandedColorScheme.DARK_BRANDED_THEME);
    }

    @Test
    public void testUpdateColors_incognito() {
        final int primaryColor = ChromeColors.getDefaultThemeColor(mContext, true);
        doReturn(primaryColor).when(mLocationBarDataProvider).getPrimaryColor();
        doReturn(true).when(mLocationBarDataProvider).isIncognitoBranded();

        mMediator.updateBrandedColorScheme();

        verify(mLocationBarLayout).setDeleteButtonTint(any(ColorStateList.class));
        verify(mStatusCoordinator).setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        verify(mAutocompleteCoordinator).updateVisualsForState(BrandedColorScheme.INCOGNITO);
    }

    @Test
    public void testUpdateColors_default() {
        final int primaryColor = ChromeColors.getDefaultThemeColor(mContext, false);
        doReturn(primaryColor).when(mLocationBarDataProvider).getPrimaryColor();
        doReturn(false).when(mLocationBarDataProvider).isIncognito();

        mMediator.updateBrandedColorScheme();

        verify(mLocationBarLayout).setDeleteButtonTint(any(ColorStateList.class));
        verify(mStatusCoordinator).setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        verify(mAutocompleteCoordinator).updateVisualsForState(BrandedColorScheme.APP_DEFAULT);
    }

    @Test
    public void testUpdateColors_setColorScheme() {
        var url = JUnitTestGURLs.BLUE_1;
        UrlBarData urlBarData = UrlBarData.forUrl(url);
        doReturn(urlBarData).when(mLocationBarDataProvider).getUrlBarData();
        doReturn(url).when(mLocationBarDataProvider).getCurrentGurl();
        doReturn(true).when(mUrlCoordinator).setBrandedColorScheme(anyInt());

        mMediator.updateBrandedColorScheme();
        verify(mLocationBarLayout).setDeleteButtonTint(any());
        verify(mUrlCoordinator)
                .setUrlBarData(
                        urlBarData, UrlBar.ScrollType.SCROLL_TO_TLD, SelectionState.SELECT_ALL);
        verify(mStatusCoordinator).setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        verify(mAutocompleteCoordinator)
                .updateVisualsForState(BrandedColorScheme.DARK_BRANDED_THEME);
    }

    @Test
    public void testSetUrl() {
        var url = JUnitTestGURLs.BLUE_1;
        UrlBarData urlBarData = UrlBarData.forUrl(url);
        mMediator.setUrl(url, urlBarData);

        // Assume that the URL bar is now focused without focus animations.
        doReturn(true).when(mUrlCoordinator).hasFocus();
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mMediator.setUrl(url, urlBarData);

        // Verify that setUrl() never clears focus when the URL bar is focused without animations.
        verify(mUrlCoordinator, never()).clearFocus();

        // Verify that setUrlBarData() was invoked exactly once, after the first invocation of
        // setUrl() when the URL bar was not focused.
        verify(mUrlCoordinator, times(1))
                .setUrlBarData(
                        urlBarData, UrlBar.ScrollType.SCROLL_TO_TLD, SelectionState.SELECT_ALL);
    }

    @Test
    public void testSetUrlBarFocus_focusedFromFakebox() {
        mMediator.setUrlBarFocus(true, null, OmniboxFocusReason.FAKE_BOX_TAP);
        assertTrue(mMediator.didFocusUrlFromFakebox());
        verify(mUrlCoordinator).requestFocus();
    }

    @Test
    public void testSetUrlBarFocus_notFocused() {
        mMediator.setUrlBarFocus(false, null, OmniboxFocusReason.FAKE_BOX_TAP);
        verify(mUrlCoordinator).clearFocus();
    }

    @Test
    public void testSetUrlBarFocus_pastedText() {
        doReturn("text").when(mUrlCoordinator).getTextWithoutAutocomplete();
        doReturn("textWith").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.setUrlBarFocus(true, "pastedText", OmniboxFocusReason.OMNIBOX_TAP);
        verify(mUrlCoordinator)
                .setUrlBarData(
                        argThat(matchesUrlBarDataForQuery("pastedText")),
                        eq(UrlBar.ScrollType.NO_SCROLL),
                        eq(UrlBarCoordinator.SelectionState.SELECT_END));
        verify(mAutocompleteCoordinator).onTextChanged("text");
    }

    @Test
    public void testOnUrlFocusChange() {
        testOnUrlFocusChange(/* expectRetainOmniboxOnFocus= */ false);
    }

    @Test
    public void testOnUrlFocusChange_shouldNotRetainOmniboxOnFocus() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.FALSE);
        testOnUrlFocusChange(/* expectRetainOmniboxOnFocus= */ false);
    }

    @Test
    public void testOnUrlFocusChange_shouldRetainOmniboxOnFocus() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.TRUE);
        testOnUrlFocusChange(/* expectRetainOmniboxOnFocus= */ true);
    }

    private void testOnUrlFocusChange(boolean expectRetainOmniboxOnFocus) {
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.onUrlFocusChange(true);

        assertTrue(mMediator.isUrlBarFocused());
        verify(mStatusCoordinator).setShouldAnimateIconChanges(true);
        verify(mUrlCoordinator, times(expectRetainOmniboxOnFocus ? 0 : 1))
                .setUrlBarData(
                        UrlBarData.EMPTY, UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_END);
        verify(mStatusCoordinator).onUrlFocusChange(true);
        verify(mUrlCoordinator).onUrlFocusChange(true);
        verify(mUrlCoordinator, times(expectRetainOmniboxOnFocus ? 1 : 0))
                .setSelectAllOnFocus(true);

        mMediator.finishUrlFocusChange(true, true);

        verify(mUrlCoordinator, times(1)).setSelectAllOnFocus(false);
    }

    @Test
    public void testOnUrlFocusChange_geolocation() {
        int primeCount = sGeoHeaderPrimeCount;
        mMediator.onFinishNativeInitialization();
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        UrlBarData urlBarData = mock(UrlBarData.class);
        doReturn(urlBarData).when(mLocationBarDataProvider).getUrlBarData();
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        mMediator.onUrlFocusChange(true);

        assertEquals(primeCount + 1, sGeoHeaderPrimeCount);
    }

    @Test
    public void testOnUrlFocusChange_geolocationPreNative() {
        ShadowLooper looper = ShadowLooper.shadowMainLooper();
        OneshotSupplierImpl<TemplateUrlService> templateUrlServiceSupplier =
                new OneshotSupplierImpl<>();
        mMediator =
                new LocationBarMediator(
                        mContext,
                        mLocationBarLayout,
                        mLocationBarDataProvider,
                        mUiOverrides,
                        mProfileSupplier,
                        mOverrideUrlLoadingDelegate,
                        mLocaleManager,
                        templateUrlServiceSupplier,
                        mOverrideBackKeyBehaviorDelegate,
                        mWindowAndroid,
                        /* isTablet= */ false,
                        mLensController,
                        tab -> true,
                        mOmniboxUma,
                        () -> mIsToolbarMicEnabled,
                        mEmbedderImpl,
                        mTabModelSelectorSupplier);
        mMediator.setCoordinators(mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);
        int primeCount = sGeoHeaderPrimeCount;
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        UrlBarData urlBarData = mock(UrlBarData.class);
        doReturn(urlBarData).when(mLocationBarDataProvider).getUrlBarData();
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        doAnswer(
                        invocation -> {
                            ((Runnable) invocation.getArgument(0)).run();
                            return null;
                        })
                .when(mLocationBarLayout)
                .post(any());
        mMediator.onUrlFocusChange(true);

        assertEquals(primeCount, sGeoHeaderPrimeCount);
        templateUrlServiceSupplier.set(mTemplateUrlService);
        looper.idle();
        assertEquals(primeCount + 1, sGeoHeaderPrimeCount);
    }

    @Test
    public void testOnUrlFocusChange_notFocusedTablet() {
        NewTabPageDelegate newTabPageDelegate = mock(NewTabPageDelegate.class);
        doReturn(newTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        mTabletMediator.addUrlFocusChangeListener(mUrlCoordinator);
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        UrlBarData urlBarData = UrlBarData.create(null, "text", 0, 0, "text");
        doReturn(urlBarData).when(mLocationBarDataProvider).getUrlBarData();
        mTabletMediator.onUrlFocusChange(true);
        Mockito.reset(mStatusCoordinator);

        mTabletMediator.onUrlFocusChange(false);

        assertFalse(mTabletMediator.isUrlBarFocused());
        verify(mStatusCoordinator).setShouldAnimateIconChanges(false);
        verify(mStatusCoordinator).onUrlFocusChange(false);
        verify(mUrlCoordinator).onUrlFocusChange(false);
        verify(mUrlCoordinator)
                .setUrlBarData(
                        urlBarData, UrlBar.ScrollType.SCROLL_TO_TLD, SelectionState.SELECT_ALL);
    }

    @Test
    public void testHandleUrlFocusAnimation_tablet() {
        NewTabPageDelegate newTabPageDelegate = mock(NewTabPageDelegate.class);
        doReturn(newTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doAnswer(
                        invocation -> {
                            ((Rect) invocation.getArgument(0)).set(0, 0, 10, 10);
                            return null;
                        })
                .when(mRootView)
                .getLocalVisibleRect(any());

        mTabletMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mTabletMediator.handleUrlFocusAnimation(true);

        verify(mUrlCoordinator).onUrlFocusChange(true);
        verify(mUrlAnimator).start();
        verify(mUrlAnimator).setDuration(anyLong());
        verify(mUrlAnimator).addListener(any());
    }

    @Test
    public void testHandleUrlFocusAnimation_ntp() {
        NewTabPageDelegate newTabPageDelegate = mock(NewTabPageDelegate.class);
        doReturn(true).when(newTabPageDelegate).isCurrentlyVisible();
        doReturn(newTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();

        mTabletMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mTabletMediator.handleUrlFocusAnimation(true);

        verify(mUrlCoordinator).onUrlFocusChange(true);
        verify(mUrlAnimator, never()).start();
        verify(mUrlAnimator, never()).setDuration(anyLong());
        verify(mUrlAnimator, never()).addListener(any());
    }

    @Test
    public void testHandleUrlFocusAnimation_phone() {
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.handleUrlFocusAnimation(true);

        verify(mUrlCoordinator).onUrlFocusChange(true);
        verify(mUrlAnimator, never()).start();
        verify(mUrlAnimator, never()).setDuration(anyLong());
        verify(mUrlAnimator, never()).addListener(any());
    }

    @Test
    public void testSetUrlFocusChangeInProgress() {
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.setUrlFocusChangeInProgress(true);

        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);
        mMediator.setUrlBarFocus(true, null, OmniboxFocusReason.FAKE_BOX_TAP);
        mMediator.onUrlFocusChange(true);
        doReturn("text").when(mUrlCoordinator).getTextWithoutAutocomplete();

        mMediator.setUrlFocusChangeInProgress(false);

        verify(mUrlCoordinator).onUrlAnimationFinished(true);
        verify(mUrlCoordinator).clearFocus();
        // The first invocation of requestFocus() is from setUrlBarFocus, which we use above to set
        // mUrlFocusedFromFakebox to true.
        verify(mUrlCoordinator, times(2)).requestFocus();
        verify(mUrlCoordinator)
                .setUrlBarData(
                        argThat(matchesUrlBarDataForQuery("text")),
                        eq(UrlBar.ScrollType.NO_SCROLL),
                        eq(UrlBarCoordinator.SelectionState.SELECT_END));
    }

    @Test
    public void testMicUpdatedAfterEventTriggered() {
        mMediator.onVoiceAvailabilityImpacted();
        verify(mLocationBarLayout, atLeast(1)).setMicButtonVisibility(false);
        verify(mLocationBarLayout, never()).setMicButtonVisibility(true);

        Mockito.reset(mLocationBarLayout);
        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        mMediator.onFinishNativeInitialization();
        mMediator.onVoiceAvailabilityImpacted();

        verify(mLocationBarLayout, atLeast(1)).setMicButtonVisibility(false);
        verify(mLocationBarLayout, never()).setMicButtonVisibility(true);

        mMediator.onUrlFocusChange(true);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();
        mMediator.onVoiceAvailabilityImpacted();

        verify(mLocationBarLayout).setMicButtonVisibility(true);
    }

    @Test
    public void testButtonVisibility_phone() {
        // Regression test for phones: toolbar mic visibility shouldn't impact the location
        // bar mic.
        verifyPhoneMicButtonVisibility();
    }

    @Test
    public void testButtonVisibility_phone_toolbarMicEnabled() {
        // Regression test for phones: toolbar mic visibility shouldn't impact the location
        // bar mic.
        mIsToolbarMicEnabled = true;
        verifyPhoneMicButtonVisibility();
    }

    private void verifyPhoneMicButtonVisibility() {
        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        mMediator.onFinishNativeInitialization();
        Mockito.reset(mLocationBarLayout);

        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setDeleteButtonVisibility(false);

        mMediator.onUrlFocusChange(true);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(true);
        verify(mLocationBarLayout, never()).setDeleteButtonVisibility(true);

        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setDeleteButtonVisibility(true);
    }

    @Test
    public void testMicButtonVisibility_toolbarMicDisabled_tablet() {
        verifyMicButtonVisibilityWhenFocusChanges(true);
    }

    @Test
    public void testMicButtonVisibility_toolbarMicEnabled_tablet() {
        mIsToolbarMicEnabled = true;
        verifyMicButtonVisibilityWhenFocusChanges(false);
    }

    // Sets up and executes a test for visibility of a mic button on a tablet.
    // The mic button should not be visible if toolbar mic is visible as well.
    private void verifyMicButtonVisibilityWhenFocusChanges(boolean shouldBeVisible) {
        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mTabletMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        mTabletMediator.onFinishNativeInitialization();
        mTabletMediator.setShouldShowButtonsWhenUnfocusedForTablet(true);
        mTabletMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mTabletMediator.onUrlFocusChange(true);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();
        Mockito.reset(mLocationBarTablet);

        mTabletMediator.updateButtonVisibility();
        verify(mLocationBarTablet).setMicButtonVisibility(shouldBeVisible);
    }

    @Test
    public void testLensButtonVisibility_lensDisabled_tablet() {
        doReturn(false).when(mLensController).isLensEnabled(any());
        verifyLensButtonVisibilityWhenFocusChanges(false, "");
    }

    @Test
    public void testLensButtonVisibility_lensEnabled_tablet() {
        doReturn(true).when(mLensController).isLensEnabled(any());
        verifyLensButtonVisibilityWhenFocusChanges(true, "");
    }

    @Test
    public void testLensButtonVisibility_lensDisabledWithInputText_tablet() {
        doReturn(false).when(mLensController).isLensEnabled(any());
        verifyLensButtonVisibilityWhenFocusChanges(false, "text");
    }

    @Test
    public void testLensButtonVisibility_lensEnabledWithInputText_tablet() {
        // Do not show lens when the omnibox already has input.
        doReturn(true).when(mLensController).isLensEnabled(any());
        verifyLensButtonVisibilityWhenFocusChanges(false, "text");
    }

    @Test
    public void testLensButtonVisibility_lensEnabled_suppressedByUiOverrides() {
        mUiOverrides.setLensEntrypointAllowed(false);
        doReturn(true).when(mLensController).isLensEnabled(any());
        verifyLensButtonVisibilityWhenFocusChanges(false, "");
    }

    private void verifyLensButtonVisibilityWhenFocusChanges(
            boolean shouldBeVisible, String inputText) {
        mTabletMediator.resetLastCachedIsLensOnOmniboxEnabledForTesting();
        mTabletMediator.setLensControllerForTesting(mLensController);
        mTabletMediator.onFinishNativeInitialization();
        mTabletMediator.setShouldShowButtonsWhenUnfocusedForTablet(true);
        mTabletMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mTabletMediator.onUrlFocusChange(true);
        doReturn(inputText).when(mUrlCoordinator).getTextWithAutocomplete();
        Mockito.reset(mLocationBarTablet);

        mTabletMediator.updateButtonVisibility();
        verify(mLocationBarTablet).setLensButtonVisibility(shouldBeVisible);
    }

    @Test
    public void testButtonVisibility_showMicUnfocused() {
        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        mMediator.onFinishNativeInitialization();
        mTabletMediator.setShouldShowButtonsWhenUnfocusedForTablet(false);
        mMediator.setShouldShowMicButtonWhenUnfocusedForPhone(true);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();

        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(true);
    }

    @Test
    public void testButtonVisibility_showMicUnfocused_toolbarMicDisabled_tablet() {
        verifyMicButtonVisibilityWhenShowMicUnfocused(true);
    }

    @Test
    public void testButtonVisibility_showMicUnfocused_suppressedByUiOverrides() {
        mUiOverrides.setVoiceEntrypointAllowed(false);
        verifyMicButtonVisibilityWhenShowMicUnfocused(false);
    }

    @Test
    public void testButtonVisibility_showMicUnfocused_toolbarMicEnabled_tablet() {
        mIsToolbarMicEnabled = true;
        verifyMicButtonVisibilityWhenShowMicUnfocused(false);
    }

    private void verifyMicButtonVisibilityWhenShowMicUnfocused(boolean shouldBeVisible) {
        mTabletMediator.onFinishNativeInitialization();
        mTabletMediator.setShouldShowButtonsWhenUnfocusedForTablet(false);
        mTabletMediator.setShouldShowMicButtonWhenUnfocusedForTesting(true);
        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mTabletMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();
        Mockito.reset(mLocationBarTablet);

        mTabletMediator.updateButtonVisibility();
        verify(mLocationBarTablet).setMicButtonVisibility(shouldBeVisible);
    }

    @Test
    public void testButtonVisibility_tablet() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mTabletMediator.onFinishNativeInitialization();
        Mockito.reset(mLocationBarTablet);
        mTabletMediator.updateButtonVisibility();

        verify(mLocationBarTablet).setMicButtonVisibility(false);
        verify(mLocationBarTablet).setBookmarkButtonVisibility(true);
        verify(mLocationBarTablet).setSaveOfflineButtonVisibility(true, true);
    }

    @Test
    public void testButtonVisibility_tabletDontShowUnfocused() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mTabletMediator.onFinishNativeInitialization();
        mTabletMediator.setShouldShowButtonsWhenUnfocusedForTablet(false);
        Mockito.reset(mLocationBarTablet);
        mTabletMediator.updateButtonVisibility();

        verify(mLocationBarTablet).setMicButtonVisibility(false);
        verify(mLocationBarTablet).setBookmarkButtonVisibility(false);
        verify(mLocationBarTablet).setSaveOfflineButtonVisibility(false, true);
    }

    @Test
    public void testRecordHistogramOmniboxClick_Ntp() {
        mMediator.onFinishNativeInitialization();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        // Test clicking omnibox on {@link NewTabPage}.
        doReturn(false)
                .when(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(any(), anyBoolean());

        doReturn(true).when(mTab).isNativePage();
        ShadowUrlUtilities.sIsNtp = true;
        assertTrue(UrlUtilities.isNtpUrl(mTab.getUrl()));
        doReturn(false).when(mTab).isIncognito();
        // Test navigating using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(false)
                        .build());
        verify(mOmniboxUma, times(1)).recordNavigationOnNtp(TEST_URL, PageTransition.TYPED, true);
        // Test searching using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.GENERATED)
                        .setOpenInNewTab(false)
                        .build());
        // The time to be checked for the calling of recordNavigationOnNtp is still 1 here
        // as we verify with the argument PageTransition.GENERATED instead.
        verify(mOmniboxUma, times(1))
                .recordNavigationOnNtp(TEST_URL, PageTransition.GENERATED, true);

        // Test clicking omnibox on other native page.
        // This will run the function recordNavigationOnNtp with isNtp equal to false
        // making it unable to record the histogram.
        ShadowUrlUtilities.sIsNtp = false;
        assertFalse(UrlUtilities.isNtpUrl(mTab.getUrl()));
        // Test navigating using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(false)
                        .build());
        verify(mOmniboxUma, times(1)).recordNavigationOnNtp(TEST_URL, PageTransition.TYPED, true);
        // Test searching using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.GENERATED)
                        .setOpenInNewTab(false)
                        .build());
        verify(mOmniboxUma, times(1))
                .recordNavigationOnNtp(TEST_URL, PageTransition.GENERATED, true);

        // Test clicking omnibox on html/rendered web page.
        doReturn(false).when(mTab).isNativePage();
        // Test navigating using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(false)
                        .build());
        verify(mOmniboxUma, times(1)).recordNavigationOnNtp(TEST_URL, PageTransition.TYPED, true);
        // Test searching using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.GENERATED)
                        .setOpenInNewTab(false)
                        .build());
        verify(mOmniboxUma, times(1))
                .recordNavigationOnNtp(TEST_URL, PageTransition.GENERATED, true);

        // Test clicking omnibox on {@link StartSurface}.
        doReturn(true)
                .when(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(any(), anyBoolean());
        // Test navigating using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(false)
                        .build());
        verify(mOmniboxUma, times(1)).recordNavigationOnNtp(TEST_URL, PageTransition.TYPED, true);
        // Test searching using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.GENERATED)
                        .setOpenInNewTab(false)
                        .build());
        verify(mOmniboxUma, times(1))
                .recordNavigationOnNtp(TEST_URL, PageTransition.GENERATED, true);
    }

    @Test
    public void testClearUrlBarCursorWithoutFocusAnimations() {
        // Assume that the URL bar is focused without animations on the NTP.
        doReturn(true).when(mUrlCoordinator).hasFocus();
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);

        mMediator.clearUrlBarCursorWithoutFocusAnimations();
        // Verify that the omnibox focus is cleared on an exit from the NTP.
        verify(mUrlCoordinator).clearFocus();
    }

    @Test
    public void testOnTouchAfterFocus_triggerUrlFocusChange() {
        doReturn("").when(mUrlCoordinator).getTextWithoutAutocomplete();
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.onUrlFocusChange(true);
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mMediator.onTouchAfterFocus();
        verify(mUrlCoordinator, times(2)).onUrlFocusChange(true);
    }

    @Test
    public void testOnTouchAfterFocus_notHandled() {
        doReturn("", "hello").when(mUrlCoordinator).getTextWithoutAutocomplete();
        // URL bar is not focused without animations.
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(false);
        mMediator.onTouchAfterFocus();

        // URL bar text is not empty.
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mMediator.onTouchAfterFocus();
        verify(mUrlCoordinator, never()).onUrlFocusChange(true);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.USE_FUSED_LOCATION_PROVIDER)
    public void testFusedLocationProvider() {
        ShadowLooper looper = ShadowLooper.shadowMainLooper();
        Profile profile = mock(Profile.class);
        mProfileSupplier.set(profile);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onFinishNativeInitialization();
        looper.idle();

        assertEquals(sGeoHeaderPrimeCount, 1);

        mMediator.onPauseWithNative();
        assertEquals(sGeoHeaderStopCount, 1);
        assertEquals(sGeoHeaderPrimeCount, 1);

        mMediator.onResumeWithNative();
        assertEquals(sGeoHeaderPrimeCount, 2);
        assertEquals(sGeoHeaderStopCount, 1);
    }

    private ArgumentMatcher<UrlBarData> matchesUrlBarDataForQuery(String query) {
        return actual -> {
            UrlBarData expected = UrlBarData.forNonUrlText(query);
            return TextUtils.equals(actual.displayText, expected.displayText);
        };
    }
}
