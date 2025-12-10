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
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.animation.ObjectAnimator;
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

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.banners.AppMenuVerbiage;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.composeplate.ComposeplateUtilsJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.toolbar.ToolbarWidthConsumer;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
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
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.accessibility.AccessibilityFeatureMap;
import org.chromium.components.browser_ui.accessibility.PageZoomIndicatorCoordinator;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.webapps.AddToHomescreenCoordinator;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppBannerManagerJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.content_public.common.ResourceRequestBodyJni;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for LocationBarMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        shadows = {
            LocationBarMediatorTest.ShadowUrlUtilities.class,
            LocationBarMediatorTest.ObjectAnimatorShadow.class
        })
@DisableFeatures({
    ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
    AccessibilityFeatureMap.ANDROID_ZOOM_INDICATOR,
})
@EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
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
    @Rule public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private LocationBarLayout mLocationBarLayout;
    @Mock private LocationBarTablet mLocationBarTablet;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private OverrideUrlLoadingDelegate mOverrideUrlLoadingDelegate;
    @Mock private LocaleManager mLocaleManager;
    @Mock private UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private MultiInstanceManager mMultiInstanceManager;
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
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private AddToHomescreenCoordinator mAddToHomescreenCoordinator;
    @Mock private PageZoomIndicatorCoordinator mPageZoomIndicatorCoordinator;

    @Mock private LensController mLensController;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private Profile mProfile;
    @Mock private PreloadPagesSettingsBridge.Natives mPreloadPagesSettingsJni;
    @Mock private LocationBarMediator.OmniboxUma mOmniboxUma;
    @Mock private OmniboxSuggestionsDropdownEmbedderImpl mEmbedderImpl;
    @Mock private ResourceRequestBody.Natives mResourceRequestBodyJni;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private AppBannerManager mAppBannerManager;
    @Mock private AppBannerManager.Natives mAppBannerManagerJni;
    @Mock private NewTabPageDelegate mNewTabPageDelegate;
    @Mock private FuseboxCoordinator mFuseboxCoordinator;

    @Captor private ArgumentCaptor<Runnable> mRunnableCaptor;
    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Mock private ComposeplateUtils.Natives mMockComposeplateUtilsJni;

    private Context mContext;
    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    private LocationBarMediator mMediator;
    private LocationBarMediator mTabletMediator;
    private UrlBarData mUrlBarData;
    private boolean mIsToolbarMicEnabled;
    private LocationBarEmbedderUiOverrides mUiOverrides;
    private OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier;
    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier =
                    new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH);
    private final ObservableSupplierImpl<@FuseboxState Integer> mFuseboxStateSupplier =
            new ObservableSupplierImpl<>(FuseboxState.EXPANDED);

    // Members capturing final state of the LocationBarLayout elements.
    private boolean mNavigateButtonIsVisible;

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
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTabModelSelector).when(mTabModelSelectorSupplier).get();
        doReturn(mRootView).when(mLocationBarLayout).getRootView();
        doReturn(true).when(mLocationBarLayout).shouldClearTextOnFocus();
        doReturn(mRootView).when(mLocationBarTablet).getRootView();
        doReturn(new WeakReference<>(null)).when(mWindowAndroid).getActivity();
        UrlUtilitiesJni.setInstanceForTesting(mUrlUtilitiesJniMock);
        OmniboxPrerenderJni.setInstanceForTesting(mPrerenderJni);
        PreloadPagesSettingsBridgeJni.setInstanceForTesting(mPreloadPagesSettingsJni);
        ResourceRequestBodyJni.setInstanceForTesting(mResourceRequestBodyJni);
        doReturn(mProfile).when(mTab).getProfile();
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(mProfile);
        doReturn(ControlsPosition.TOP).when(mBrowserControlsStateProvider).getControlsPosition();
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        mTemplateUrlServiceSupplier = new OneshotSupplierImpl<>();
        mTemplateUrlServiceSupplier.set(mTemplateUrlService);
        mUiOverrides = new LocationBarEmbedderUiOverrides();
        ComposeplateUtilsJni.setInstanceForTesting(mMockComposeplateUtilsJni);
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(true);

        doAnswer(i -> mNavigateButtonIsVisible = i.getArgument(0))
                .when(mLocationBarLayout)
                .setNavigateButtonVisibility(anyBoolean());

        doReturn(mFuseboxStateSupplier).when(mFuseboxCoordinator).getFuseboxStateSupplier();
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();

        AppBannerManagerJni.setInstanceForTesting(mAppBannerManagerJni);
        doReturn(mAppBannerManager)
                .when(mAppBannerManagerJni)
                .getJavaBannerManagerForWebContents(mWebContents);

        mMediator =
                new LocationBarMediator(
                        mContext,
                        mLocationBarLayout,
                        mLocationBarDataProvider,
                        mUiOverrides,
                        mProfileSupplier,
                        mOverrideUrlLoadingDelegate,
                        mLocaleManager,
                        mTemplateUrlServiceSupplier,
                        mOverrideBackKeyBehaviorDelegate,
                        mWindowAndroid,
                        /* isTablet= */ false,
                        mLensController,
                        mOmniboxUma,
                        () -> mIsToolbarMicEnabled,
                        mEmbedderImpl,
                        mTabModelSelectorSupplier,
                        mBrowserControlsStateProvider,
                        () -> mModalDialogManager,
                        mAutocompleteRequestTypeSupplier,
                        mPageZoomIndicatorCoordinator,
                        mFuseboxCoordinator,
                        mMultiInstanceManager);
        mMediator.setCoordinators(mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);
        mMediator.setAddToHomescreenCoordinatorForTesting(mAddToHomescreenCoordinator);
        ObjectAnimatorShadow.setUrlAnimator(mUrlAnimator);

        mTabletMediator = createTabletMediator();

        ShadowUrlUtilities.sIsNtp = false;
        sGeoHeaderPrimeCount = 0;
        sGeoHeaderStopCount = 0;
        GeolocationHeader.setPrimeLocationForGeoHeaderIfEnabledForTesting(
                () -> sGeoHeaderPrimeCount++);
        GeolocationHeader.setStopListeningForLocationUpdatesForTesting(() -> sGeoHeaderStopCount++);
    }

    private LocationBarMediator createTabletMediator() {
        var tabletMediator =
                new LocationBarMediator(
                        mContext,
                        mLocationBarTablet,
                        mLocationBarDataProvider,
                        mUiOverrides,
                        mProfileSupplier,
                        mOverrideUrlLoadingDelegate,
                        mLocaleManager,
                        mTemplateUrlServiceSupplier,
                        mOverrideBackKeyBehaviorDelegate,
                        mWindowAndroid,
                        /* isTablet= */ true,
                        mLensController,
                        (tab, transition, isNtp) -> {},
                        () -> mIsToolbarMicEnabled,
                        mEmbedderImpl,
                        mTabModelSelectorSupplier,
                        mBrowserControlsStateProvider,
                        () -> mModalDialogManager,
                        new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH),
                        mPageZoomIndicatorCoordinator,
                        mFuseboxCoordinator,
                        mMultiInstanceManager);
        tabletMediator.setCoordinators(
                mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);
        updateTabletWidthConsumers(tabletMediator);
        return tabletMediator;
    }

    private void updateTabletWidthConsumers(LocationBarMediator locationBarMediator) {
        int buttonWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        locationBarMediator.getMicButtonToolbarWidthConsumer().updateVisibility(buttonWidth);
        locationBarMediator.getLensButtonToolbarWidthConsumer().updateVisibility(buttonWidth);
        locationBarMediator.getInstallButtonToolbarWidthConsumer().updateVisibility(buttonWidth);
        locationBarMediator.getBookmarkButtonToolbarWidthConsumer().updateVisibility(buttonWidth);
        locationBarMediator.getZoomButtonToolbarWidthConsumer().updateVisibility(buttonWidth);
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
                        mUrlBarData, UrlBar.ScrollType.SCROLL_TO_TLD, SelectionState.SELECT_ALL);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnSuggestionsChanged() {
        ArgumentCaptor<OmniboxPrerender> omniboxPrerenderCaptor =
                ArgumentCaptor.forClass(OmniboxPrerender.class);
        doReturn(123L).when(mPrerenderJni).init(omniboxPrerenderCaptor.capture());
        mMediator.onFinishNativeInitialization();
        Profile profile = mock(Profile.class);
        mProfileSupplier.set(profile);
        verify(mPrerenderJni).initializeForProfile(123L, profile);

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
                .prerenderMaybe(anyLong(), anyString(), anyString(), anyLong(), any(), any());
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

        AutocompleteMatch defaultMatch =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("text")
                        .setInlineAutocompletion("textWithAutocomplete")
                        .setAdditionalText("additionalText")
                        .setIsSearch(false)
                        .setAllowedToBeDefaultMatch(true)
                        .build();
        mMediator.onSuggestionsChanged(defaultMatch);
        verify(mPrerenderJni)
                .prerenderMaybe(123L, "text", JUnitTestGURLs.RED_1.getSpec(), 456L, profile, mTab);
        verify(mStatusCoordinator).onDefaultMatchClassified(false);
        verify(mUrlCoordinator)
                .setAutocompleteText("text", "textWithAutocomplete", "additionalText");

        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);
        mMediator.onSuggestionsChanged(defaultMatch);
        verify(mStatusCoordinator, times(2)).onDefaultMatchClassified(true);
    }

    @Test
    public void testOnSuggestionsChanged_nullMatch() {
        doReturn("text").when(mUrlCoordinator).getTextWithoutAutocomplete();
        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();

        mMediator.onSuggestionsChanged(null);
        verify(mStatusCoordinator).onDefaultMatchClassified(true);
        verify(mUrlCoordinator).setAutocompleteText("text", null, null);
    }

    public void testLoadUrl_base() {
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
    @DisableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrlNoPostDelayedTaskFocusTab() {
        testLoadUrl_base();
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrlPostDelayedTaskFocusTab() {
        testLoadUrl_base();
    }

    public void testLoadUrlWithAutocompleteLoadCallback_base() {
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
    @DisableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrlWithAutocompleteLoadCallbackNoPostDelayedTaskFocusTab() {
        testLoadUrlWithAutocompleteLoadCallback_base();
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrlWithAutocompleteLoadCallbackPostDelayedTaskFocusTab() {
        testLoadUrlWithAutocompleteLoadCallback_base();
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
                        .setPostData(data)
                        .setExtraHeaders(Map.of("Content-Type", text))
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
    public void testLoadUrlWithExtraHeaders() {
        mMediator.onFinishNativeInitialization();
        Map<String, String> headers = new HashMap<>();
        headers.put("Authorization", "Bearer token123");
        headers.put("Custom-Header", "custom-value");
        headers.put("Content-Type", "application/json");

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setExtraHeaders(headers)
                        .build());

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
        String verbatimHeaders = mLoadUrlParamsCaptor.getValue().getVerbatimHeaders();
        assertTrue(verbatimHeaders.contains("Authorization: Bearer token123"));
        assertTrue(verbatimHeaders.contains("Custom-Header: custom-value"));
        assertTrue(verbatimHeaders.contains("Content-Type: application/json"));
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
        assertTrue(params.extraHeaders.isEmpty());
        assertFalse(params.openInNewTab);
        verify(mTab, times(0)).loadUrl(any());
    }

    @Test
    public void testLoadUrl_openInNewTab_base() {
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
    public void testLoadUrl_openInNewWindow() {
        mMediator.onFinishNativeInitialization();

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(false).when(mTab).isIncognito();
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewWindow(true)
                        .build());

        verify(mMultiInstanceManager)
                .openUrlInOtherWindow(mLoadUrlParamsCaptor.capture(), anyInt(), eq(true), anyInt());
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
    }

    @Test
    @DisableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrl_openInNewTabNoPostDelayedTaskFocusTab() {
        testLoadUrl_openInNewTab_base();
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrl_openInNewTabPostDelayedTaskFocusTab() {
        testLoadUrl_openInNewTab_base();
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
        Mockito.reset(mLocationBarDataProvider);
        doReturn(mView).when(mTab).getView();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        assertEquals(mView, mMediator.getViewForUrlBackFocus());
        verify(mTab).getView();

        doReturn(null).when(mLocationBarDataProvider).getTab();
        assertNull(mMediator.getViewForUrlBackFocus());
        verify(mLocationBarDataProvider, times(2)).getTab();
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

    public void testPerformSearchQuery_base() {
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
    @DisableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testPerformSearchQueryNoPostDelayedTaskFocusTab() {
        testPerformSearchQuery_base();
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testPerformSearchQueryPostDelayedTaskFocusTab() {
        testPerformSearchQuery_base();
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
                        mUrlBarData, UrlBar.ScrollType.SCROLL_TO_TLD, SelectionState.SELECT_ALL);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnKey_right() {
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(KeyEvent.KEYCODE_DPAD_RIGHT).when(mKeyEvent).getKeyCode();
        doReturn(0).when(mKeyEvent).getModifiers();
        doReturn(true).when(mKeyEvent).hasNoModifiers();
        doReturn("a").when(mView).getText();
        doReturn(0).when(mView).getSelectionStart();
        doReturn(1).when(mView).getSelectionEnd();

        assertFalse(mMediator.onKey(mView, KeyEvent.KEYCODE_DPAD_RIGHT, mKeyEvent));

        doReturn(1).when(mView).getSelectionStart();
        assertTrue(mMediator.onKey(mView, KeyEvent.KEYCODE_DPAD_RIGHT, mKeyEvent));
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnKey_leftRtl() {
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(KeyEvent.KEYCODE_DPAD_LEFT).when(mKeyEvent).getKeyCode();
        doReturn(0).when(mKeyEvent).getModifiers();
        doReturn(true).when(mKeyEvent).hasNoModifiers();
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
        final int primaryColor =
                ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ true);
        doReturn(primaryColor).when(mLocationBarDataProvider).getPrimaryColor();
        doReturn(true).when(mLocationBarDataProvider).isIncognitoBranded();

        mMediator.updateBrandedColorScheme();

        verify(mLocationBarLayout).setDeleteButtonTint(any(ColorStateList.class));
        verify(mStatusCoordinator).setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        verify(mAutocompleteCoordinator).updateVisualsForState(BrandedColorScheme.INCOGNITO);
    }

    @Test
    public void testUpdateColors_default() {
        final int primaryColor =
                ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ false);
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
        mMediator.setUrlBarFocus(
                true, null, OmniboxFocusReason.FAKE_BOX_TAP, AutocompleteRequestType.SEARCH);
        assertTrue(mMediator.didFocusUrlFromFakebox());
        verify(mUrlCoordinator).requestFocus();
    }

    @Test
    public void testSetUrlBarFocus_notFocused() {
        mMediator.setUrlBarFocus(
                false, null, OmniboxFocusReason.FAKE_BOX_TAP, AutocompleteRequestType.SEARCH);
        verify(mUrlCoordinator).clearFocus();
    }

    @Test
    public void testSetUrlBarFocus_NtpAIMode() {
        mMediator.onFinishNativeInitialization();
        Profile profile = mock(Profile.class);
        mMediator.setProfile(profile);
        mMediator.setUrlBarFocus(
                true, null, OmniboxFocusReason.FAKE_BOX_TAP, AutocompleteRequestType.AI_MODE);
        verify(mUrlCoordinator).requestFocus();
        verify(mFuseboxCoordinator).onAiModeActivatedFromNtp();
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testSetUrlBarFocus_pastedText() {
        doReturn("text").when(mUrlCoordinator).getTextWithoutAutocomplete();
        doReturn("textWith").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.setUrlBarFocus(
                true, "pastedText", OmniboxFocusReason.OMNIBOX_TAP, AutocompleteRequestType.SEARCH);
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
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(false);
        testOnUrlFocusChange(/* expectRetainOmniboxOnFocus= */ false);
    }

    @Test
    public void testOnUrlFocusChange_shouldRetainOmniboxOnFocus() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(true);
        testOnUrlFocusChange(/* expectRetainOmniboxOnFocus= */ true);
    }

    @Test
    public void testAnimateIconChanges_bottomToolbar() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsStateProvider).getControlsPosition();
        Mockito.reset(mStatusCoordinator);
        mMediator.onUrlFocusChange(true);
        verify(mStatusCoordinator).setShouldAnimateIconChanges(false);
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
    @SuppressWarnings("DirectInvocationOnMock")
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
    @SuppressWarnings("DirectInvocationOnMock")
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
                        mOmniboxUma,
                        () -> mIsToolbarMicEnabled,
                        mEmbedderImpl,
                        mTabModelSelectorSupplier,
                        mBrowserControlsStateProvider,
                        () -> mModalDialogManager,
                        new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH),
                        mPageZoomIndicatorCoordinator,
                        mFuseboxCoordinator,
                        mMultiInstanceManager);
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
        mMediator.setUrlBarFocus(
                true, null, OmniboxFocusReason.FAKE_BOX_TAP, AutocompleteRequestType.SEARCH);
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
        updateTabletWidthConsumers(mTabletMediator);
        ArgumentCaptor<Boolean> captor = ArgumentCaptor.forClass(Boolean.class);
        verify(mLocationBarTablet, atLeastOnce()).setMicButtonVisibility(captor.capture());
        assertEquals(shouldBeVisible, captor.getValue());
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
        updateTabletWidthConsumers(mTabletMediator);
        ArgumentCaptor<Boolean> captor = ArgumentCaptor.forClass(Boolean.class);
        verify(mLocationBarTablet, atLeastOnce()).setLensButtonVisibility(captor.capture());
        assertEquals(shouldBeVisible, captor.getValue());
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
        updateTabletWidthConsumers(mTabletMediator);
        ArgumentCaptor<Boolean> captor = ArgumentCaptor.forClass(Boolean.class);
        verify(mLocationBarLayout, atLeastOnce()).setMicButtonVisibility(captor.capture());
        assertTrue(captor.getValue());
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
        updateTabletWidthConsumers(mTabletMediator);
        ArgumentCaptor<Boolean> captor = ArgumentCaptor.forClass(Boolean.class);
        verify(mLocationBarTablet, atLeastOnce()).setMicButtonVisibility(captor.capture());
        assertEquals(shouldBeVisible, captor.getValue());
    }

    @Test
    public void testButtonVisibility_tablet() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mTabletMediator.onFinishNativeInitialization();
        Mockito.reset(mLocationBarTablet);
        mTabletMediator.updateButtonVisibility();

        verify(mLocationBarTablet).setMicButtonVisibility(false);
        verify(mLocationBarTablet).setBookmarkButtonVisibility(true);
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
    }

    @SuppressWarnings("DirectInvocationOnMock")
    public void testRecordHistogramOmniboxClick_Ntp_base() {
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
    @DisableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testRecordHistogramOmniboxClick_NtpNoPostDelayedTaskFocusTab() {
        testRecordHistogramOmniboxClick_Ntp_base();
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testRecordHistogramOmniboxClick_NtpPostDelayedTaskFocusTab() {
        testRecordHistogramOmniboxClick_Ntp_base();
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

        assertEquals(1, sGeoHeaderPrimeCount);

        mMediator.onPauseWithNative();
        assertEquals(1, sGeoHeaderStopCount);
        assertEquals(1, sGeoHeaderPrimeCount);

        mMediator.onResumeWithNative();
        assertEquals(2, sGeoHeaderPrimeCount);
        assertEquals(1, sGeoHeaderStopCount);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void navigateButtonVisibility() {
        mMediator.onFinishNativeInitialization();
        Profile profile = mock(Profile.class);
        mMediator.setProfile(profile);
        doReturn(true).when(mUrlCoordinator).isTextWrapped();
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(true);
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);
        assertTrue(mNavigateButtonIsVisible);

        doReturn(false).when(mUrlCoordinator).isTextWrapped();
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);
        assertTrue(mNavigateButtonIsVisible);

        doReturn(false).when(mUrlCoordinator).isTextWrapped();
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.IMAGE_GENERATION);
        assertTrue(mNavigateButtonIsVisible);
    }

    @Test
    public void testDeleteButtonClicked() {
        mMediator.onFinishNativeInitialization();
        mMediator.deleteButtonClicked(null);

        verify(mUrlCoordinator)
                .setUrlBarData(
                        UrlBarData.EMPTY,
                        UrlBar.ScrollType.SCROLL_TO_BEGINNING,
                        SelectionState.SELECT_ALL);
        verify(mUrlCoordinator).requestAccessibilityFocus();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_COMPOSEPLATE + ":v2_enabled/false")
    public void testButtonVisibility_showComposeplateUnfocused() {
        mProfileSupplier.set(mProfile);
        enableBothVoiceAndLensButtons();
        assertTrue(
                mMediator.shouldShowComposeplateButton(
                        /* shouldShowMicButton= */ true, /* shouldShowLensButton= */ true));

        // Verifies that the composeplate button is shown when the url bar is unfocused, and both
        // mic and lens buttons are hidden.
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(eq(false));
        verify(mLocationBarLayout).setLensButtonVisibility(eq(false));
        verify(mLocationBarLayout).setComposeplateButtonVisibility(eq(true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_COMPOSEPLATE)
    public void testButtonVisibility_dontShowComposeplateUnfocused_disabledByPolicy() {
        when(mMockComposeplateUtilsJni.isAimEntrypointEligible(eq(mProfile))).thenReturn(false);
        mProfileSupplier.set(mProfile);

        enableBothVoiceAndLensButtons();
        assertFalse(
                mMediator.shouldShowComposeplateButton(
                        /* shouldShowMicButton= */ true, /* shouldShowLensButton= */ true));

        // Verifies that the composeplate button isn't visible if disabled by policy.
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(eq(true));
        verify(mLocationBarLayout).setLensButtonVisibility(eq(true));
        verify(mLocationBarLayout, never()).setComposeplateButtonVisibility(anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_COMPOSEPLATE + ":v2_enabled/false")
    public void testButtonVisibility_dontShowComposeplateFocused() {
        mProfileSupplier.set(mProfile);
        enableBothVoiceAndLensButtons();
        assertTrue(
                mMediator.shouldShowComposeplateButton(
                        /* shouldShowMicButton= */ true, /* shouldShowLensButton= */ true));

        // Verifies that the composeplate button is hidden when url bar is focused.
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        verify(mLocationBarLayout).setMicButtonVisibility(eq(true));
        verify(mLocationBarLayout).setLensButtonVisibility(eq(true));
        verify(mLocationBarLayout).setComposeplateButtonVisibility(eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_COMPOSEPLATE)
    public void testComposeplateButtonClicked() {
        mMediator.onFinishNativeInitialization();

        GURL url = new GURL("https://foo.com");
        when(mTemplateUrlService.getComposeplateUrl()).thenReturn(url);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTab.isIncognito()).thenReturn(false);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "NewTabPage.Module.Click",
                                BrowserUiUtils.ModuleTypeOnStartAndNtp.COMPOSEPLATE_BUTTON)
                        .build();
        mMediator.composeplateButtonClicked(null);

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(url.getSpec(), mLoadUrlParamsCaptor.getValue().getUrl());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testInstallButtonClicked() {
        doReturn(new GURL(TEST_URL)).when(mWebContents).getVisibleUrl();
        mMediator.installButtonClicked(null);
        verify(mAddToHomescreenCoordinator).showForAppMenu(AppMenuVerbiage.APP_MENU_OPTION_INSTALL);
    }

    private ArgumentMatcher<UrlBarData> matchesUrlBarDataForQuery(String query) {
        return actual -> {
            UrlBarData expected = UrlBarData.forNonUrlText(query);
            return TextUtils.equals(actual.displayText, expected.displayText);
        };
    }

    private void enableBothVoiceAndLensButtons() {
        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        mMediator.onFinishNativeInitialization();
        mMediator.setShouldShowMicButtonWhenUnfocusedForPhone(true);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();
        assertTrue(mMediator.shouldShowMicButton());

        mMediator.resetLastCachedIsLensOnOmniboxEnabledForTesting();
        doReturn(true).when(mLensController).isLensEnabled(any());
        mUiOverrides.setLensEntrypointAllowed(true);
        mMediator.setShouldShowLensButtonWhenUnfocusedForPhone(true);
        mMediator.setLensControllerForTesting(mLensController);
        assertTrue(mMediator.shouldShowLensButton());

        mMediator.setUrlFocusChangeFraction(
                /* ntpSearchBoxScrollFraction= */ 1.0f, /* urlFocusChangeFraction= */ 0f);

        Mockito.reset(mLocationBarLayout);
    }

    @Test
    public void testRestoringText() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(true);
        NewTabPageDelegate newTabPageDelegate = mock(NewTabPageDelegate.class);
        doReturn(newTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();

        UserDataHost tabUserDataHost = new UserDataHost();
        doReturn(tabUserDataHost).when(mTab).getUserDataHost();

        // Prepare a state to be restored for mTab.
        String newText = "new text";
        LocationBarMediator.LocationBarState newState =
                LocationBarMediator.LocationBarState.from(mTab);
        newState.userText = newText;
        newState.isUrlBarFocused = true;

        Tab previousTab = Mockito.mock(Tab.class);
        doReturn(mProfile).when(previousTab).getProfile();
        UserDataHost previousTabUserDataHost = new UserDataHost();
        doReturn(previousTabUserDataHost).when(previousTab).getUserDataHost();

        // Emulate a state where the omnibox is focused and user has typed a text.
        mTabletMediator.onUrlFocusChange(true);
        String previousText = "previous text";
        doReturn(previousText).when(mUrlCoordinator).getTextWithoutAutocomplete();
        doReturn(previousText).when(mUrlCoordinator).getTextWithAutocomplete();

        // Emulate a tab switch from previousTab to mTab.
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mTabletMediator.onTabChanged(previousTab);
        mTabletMediator.onUrlChanged(true);

        // The state for mTab was restored.
        verify(mUrlCoordinator)
                .setUrlBarData(
                        argThat(matchesUrlBarDataForQuery(newText)),
                        eq(UrlBar.ScrollType.NO_SCROLL),
                        eq(SelectionState.SELECT_END));

        // The state for previousTab was saved.
        LocationBarMediator.LocationBarState previousState =
                LocationBarMediator.LocationBarState.from(previousTab);
        assertTrue(previousState.isUrlBarFocused);
        assertEquals(previousText, previousState.userText);
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_IMPROVEMENT_FOR_LFF})
    public void testRestoringTextAndEditingStateOnTablet() {
        OmniboxFeatures.sOmniboxImprovementForLFFPersistEditingState.setForTesting(true);
        // Recreate mediator to respect the overridden feature flag and params.
        mTabletMediator = createTabletMediator();

        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(true);
        NewTabPageDelegate newTabPageDelegate = mock(NewTabPageDelegate.class);
        doReturn(newTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();

        UserDataHost tabUserDataHost = new UserDataHost();
        doReturn(tabUserDataHost).when(mTab).getUserDataHost();

        // Prepare a state to be restored for mTab.
        String newText = "new text";
        final int newSelectionStart = 2;
        final int newSelectionEnd = 6;
        LocationBarMediator.LocationBarState newState =
                LocationBarMediator.LocationBarState.from(mTab);
        newState.userText = newText;
        newState.isUrlBarFocused = true;
        newState.selectionStart = newSelectionStart;
        newState.selectionEnd = newSelectionEnd;

        Tab previousTab = Mockito.mock(Tab.class);
        doReturn(mProfile).when(previousTab).getProfile();
        UserDataHost previousTabUserDataHost = new UserDataHost();
        doReturn(previousTabUserDataHost).when(previousTab).getUserDataHost();

        // Emulate a state where the omnibox is focused and user has typed a text.
        mTabletMediator.onUrlFocusChange(true);
        String previousText = "previous text";
        final int previousSelectionStart = 1;
        final int previousSelectionEnd = 5;
        doReturn(previousText).when(mUrlCoordinator).getTextWithoutAutocomplete();
        doReturn(previousText).when(mUrlCoordinator).getTextWithAutocomplete();
        doReturn(previousSelectionStart).when(mUrlCoordinator).getSelectionStart();
        doReturn(previousSelectionEnd).when(mUrlCoordinator).getSelectionEnd();

        // Emulate a tab switch from previousTab to mTab.
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mTabletMediator.onTabChanged(previousTab);
        mTabletMediator.onUrlChanged(true);

        // The state for mTab was restored.
        verify(mUrlCoordinator)
                .setUrlBarData(
                        argThat(matchesUrlBarDataForQuery(newText)),
                        eq(UrlBar.ScrollType.NO_SCROLL),
                        eq(SelectionState.SELECT_END));
        verify(mUrlCoordinator).setSelection(eq(newSelectionStart), eq(newSelectionEnd));

        // The state for previousTab was saved.
        LocationBarMediator.LocationBarState previousState =
                LocationBarMediator.LocationBarState.from(previousTab);
        assertTrue(previousState.isUrlBarFocused);
        assertEquals(previousText, previousState.userText);
        assertEquals(previousSelectionStart, previousState.selectionStart);
        assertEquals(previousSelectionEnd, previousState.selectionEnd);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNavigateButton_showsWhenExpandedAndFocusedWithText() {
        mMediator.onUrlFocusChange(true);
        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();

        mMediator.updateButtonVisibility();
        assertTrue(mNavigateButtonIsVisible);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNavigateButton_hidesWhenExpandedAndFocusedWithoutText() {
        mMediator.onUrlFocusChange(true);
        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();

        mMediator.updateButtonVisibility();
        assertFalse(mNavigateButtonIsVisible);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNavigateButton_hidesWhenCompact() {
        mMediator.onUrlFocusChange(true);
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();

        mMediator.updateButtonVisibility();
        assertFalse(mNavigateButtonIsVisible);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNavigateButton_hidesWhenNotFocused() {
        mMediator.onUrlFocusChange(false);
        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();

        mMediator.updateButtonVisibility();
        assertFalse(mNavigateButtonIsVisible);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNavigateButton_visibilityUpdatesOnFuseboxStateChange() {
        mMediator.onUrlFocusChange(true);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        mMediator.updateButtonVisibility();
        assertFalse(mNavigateButtonIsVisible);

        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        assertTrue(mNavigateButtonIsVisible);
    }

    @Test
    public void testInstallButton_visibleIfInstallable() {
        doReturn(true).when(mAppBannerManagerJni).isProbablyPromotable(mWebContents);
        mMediator.onUrlFocusChange(false);
        mMediator.setUrlFocusChangeInProgress(false);

        Mockito.reset(mLocationBarLayout);

        mMediator.onInstallabilityUpdated(mAppBannerManager);
        verify(mLocationBarLayout).setInstallButtonVisibility(true);
    }

    @Test
    public void testInstallButton_invisibleIfNotInstallable() {
        doReturn(false).when(mAppBannerManagerJni).isProbablyPromotable(mWebContents);
        Mockito.reset(mLocationBarLayout);

        mMediator.onInstallabilityUpdated(mAppBannerManager);
        verify(mLocationBarLayout).setInstallButtonVisibility(false);
    }

    @Test
    public void testInstallButton_invisibleOmniboxIsFocused() {
        doReturn(true).when(mAppBannerManagerJni).isProbablyPromotable(mWebContents);
        mMediator.onUrlFocusChange(true);
        mMediator.setUrlFocusChangeInProgress(false);

        Mockito.reset(mLocationBarLayout);

        mMediator.onInstallabilityUpdated(mAppBannerManager);
        verify(mLocationBarLayout).setInstallButtonVisibility(false);
    }

    @Test
    public void testHintZeroSuggestRefresh_nullTab() {
        doReturn(null).when(mLocationBarDataProvider).getTab();
        mMediator.hintZeroSuggestRefresh();
        verify(mAutocompleteCoordinator).prefetchZeroSuggestResults(null);
    }

    @Test
    @EnableFeatures(AccessibilityFeatureMap.ANDROID_ZOOM_INDICATOR)
    public void testZoomButtonClicked() {
        mMediator.onFinishNativeInitialization();
        doReturn(mWebContents).when(mTab).getWebContents();
        mMediator.zoomButtonClicked(null);
        verify(mPageZoomIndicatorCoordinator).show(mWebContents);
    }

    @Test
    @EnableFeatures(AccessibilityFeatureMap.ANDROID_ZOOM_INDICATOR)
    public void testShouldShowZoomButton_featureEnabledAndNotDefaultZoom() {
        mMediator.onFinishNativeInitialization();
        doReturn(mWebContents).when(mTab).getWebContents();
        when(mPageZoomIndicatorCoordinator.isZoomLevelDefault()).thenReturn(false);
        verify(mLocationBarLayout, never()).setZoomButtonVisibility(true);
    }

    @Test
    @EnableFeatures(AccessibilityFeatureMap.ANDROID_ZOOM_INDICATOR)
    public void testShouldShowZoomButton_featureEnabledAndDefaultZoom() {
        mMediator.onFinishNativeInitialization();
        doReturn(mWebContents).when(mTab).getWebContents();
        when(mPageZoomIndicatorCoordinator.isZoomLevelDefault()).thenReturn(true);
        verify(mLocationBarLayout, never()).setZoomButtonVisibility(false);
    }

    @Test
    @DisableFeatures(AccessibilityFeatureMap.ANDROID_ZOOM_INDICATOR)
    public void testShouldShowZoomButton_featureDisabled() {
        mMediator.onFinishNativeInitialization();
        doReturn(mWebContents).when(mTab).getWebContents();
        when(mPageZoomIndicatorCoordinator.isZoomLevelDefault()).thenReturn(false);
        verify(mLocationBarLayout, never()).setZoomButtonVisibility(false);
    }

    @Test
    @EnableFeatures(AccessibilityFeatureMap.ANDROID_ZOOM_INDICATOR)
    public void testShouldShowZoomButton_nullWebContents() {
        mMediator.onFinishNativeInitialization();
        doReturn(null).when(mTab).getWebContents();
        verify(mLocationBarLayout, never()).setZoomButtonVisibility(false);
    }

    @Test
    @EnableFeatures(AccessibilityFeatureMap.ANDROID_ZOOM_INDICATOR)
    public void testUpdateZoomButtonVisibility_popupShowing() {
        mMediator.onFinishNativeInitialization();
        doReturn(mWebContents).when(mTab).getWebContents();
        when(mPageZoomIndicatorCoordinator.isZoomLevelDefault()).thenReturn(true);
        when(mPageZoomIndicatorCoordinator.isPopupWindowShowing()).thenReturn(true);
        mMediator.updateZoomButtonVisibilityForTesting();
        verify(mLocationBarLayout).setZoomButtonVisibility(true);
    }

    @Test
    @EnableFeatures(AccessibilityFeatureMap.ANDROID_ZOOM_INDICATOR)
    public void testUpdateZoomButtonVisibility_hideButton() {
        mMediator.onFinishNativeInitialization();
        doReturn(mWebContents).when(mTab).getWebContents();
        when(mPageZoomIndicatorCoordinator.isZoomLevelDefault()).thenReturn(true);
        when(mPageZoomIndicatorCoordinator.isPopupWindowShowing()).thenReturn(false);
        mMediator.updateZoomButtonVisibilityForTesting();
        verify(mLocationBarLayout).setZoomButtonVisibility(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testMicButtonToolbarWidthConsumer() {
        int buttonWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        assertFalse(mTabletMediator.shouldShowMicButton());

        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mTabletMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        mTabletMediator.onFinishNativeInitialization();
        mTabletMediator.setShouldShowButtonsWhenUnfocusedForTablet(true);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();
        mTabletMediator.onUrlFocusChange(true);

        assertTrue(mTabletMediator.shouldShowMicButton());

        ToolbarWidthConsumer micButtonConsumer = mTabletMediator.getMicButtonToolbarWidthConsumer();
        Mockito.clearInvocations(mLocationBarTablet);

        micButtonConsumer.updateVisibility(buttonWidth);
        verify(mLocationBarTablet).setMicButtonVisibility(true);
        Mockito.clearInvocations(mLocationBarTablet);

        micButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setMicButtonVisibility(false);
        Mockito.clearInvocations(mLocationBarTablet);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testLensButtonToolbarWidthConsumer() {
        int buttonWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        assertFalse(mTabletMediator.shouldShowLensButton());

        mTabletMediator.onFinishNativeInitialization();
        mTabletMediator.resetLastCachedIsLensOnOmniboxEnabledForTesting();
        doReturn(true).when(mLensController).isLensEnabled(any());
        mUiOverrides.setLensEntrypointAllowed(true);
        mTabletMediator.setLensControllerForTesting(mLensController);
        mTabletMediator.onUrlFocusChange(true);

        assertTrue(mTabletMediator.shouldShowLensButton());

        ToolbarWidthConsumer lensButtonConsumer =
                mTabletMediator.getLensButtonToolbarWidthConsumer();
        Mockito.clearInvocations(mLocationBarTablet);

        lensButtonConsumer.updateVisibility(buttonWidth);
        verify(mLocationBarTablet).setLensButtonVisibility(true);
        Mockito.clearInvocations(mLocationBarTablet);

        lensButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setLensButtonVisibility(false);
        Mockito.clearInvocations(mLocationBarTablet);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testBookmarkButtonToolbarWidthConsumer() {
        int buttonWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        mTabletMediator.onFinishNativeInitialization();
        assertTrue(mTabletMediator.shouldShowBookmarkButton());

        ToolbarWidthConsumer bookmarkButtonConsumer =
                mTabletMediator.getBookmarkButtonToolbarWidthConsumer();
        Mockito.clearInvocations(mLocationBarTablet);

        bookmarkButtonConsumer.updateVisibility(buttonWidth);
        assertTrue(mTabletMediator.shouldShowBookmarkButton());
        verify(mLocationBarTablet).setBookmarkButtonVisibility(true);
        Mockito.clearInvocations(mLocationBarTablet);

        bookmarkButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setBookmarkButtonVisibility(false);
        Mockito.clearInvocations(mLocationBarTablet);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testInstallButtonToolbarWidthConsumer() {
        int buttonWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        mTabletMediator.onFinishNativeInitialization();
        assertFalse(mTabletMediator.shouldShowInstallButton());

        doReturn(true).when(mAppBannerManagerJni).isProbablyPromotable(mWebContents);
        assertTrue(mTabletMediator.shouldShowInstallButton());

        ToolbarWidthConsumer installButtonConsumer =
                mTabletMediator.getInstallButtonToolbarWidthConsumer();
        Mockito.clearInvocations(mLocationBarTablet);

        installButtonConsumer.updateVisibility(buttonWidth);
        verify(mLocationBarTablet).setInstallButtonVisibility(true);
        Mockito.clearInvocations(mLocationBarTablet);

        installButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setInstallButtonVisibility(false);
        Mockito.clearInvocations(mLocationBarTablet);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR,
        AccessibilityFeatureMap.ANDROID_ZOOM_INDICATOR
    })
    public void testZoomButtonToolbarWidthConsumer_notVisible() {
        int buttonWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        mTabletMediator.onFinishNativeInitialization();
        when(mPageZoomIndicatorCoordinator.isZoomLevelDefault()).thenReturn(true);
        assertFalse(mTabletMediator.shouldShowZoomButton());

        ToolbarWidthConsumer zoomButtonConsumer =
                mTabletMediator.getZoomButtonToolbarWidthConsumer();
        Mockito.clearInvocations(mLocationBarTablet);

        zoomButtonConsumer.updateVisibility(buttonWidth);
        verify(mLocationBarTablet).setZoomButtonVisibility(false);
        Mockito.clearInvocations(mLocationBarTablet);

        zoomButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setZoomButtonVisibility(false);
        Mockito.clearInvocations(mLocationBarTablet);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR,
        AccessibilityFeatureMap.ANDROID_ZOOM_INDICATOR
    })
    public void testZoomButtonToolbarWidthConsumer() {
        int buttonWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        mTabletMediator.onFinishNativeInitialization();
        when(mPageZoomIndicatorCoordinator.isZoomLevelDefault()).thenReturn(false);
        assertTrue(mTabletMediator.shouldShowZoomButton());

        ToolbarWidthConsumer zoomButtonConsumer =
                mTabletMediator.getZoomButtonToolbarWidthConsumer();
        Mockito.clearInvocations(mLocationBarTablet);

        zoomButtonConsumer.updateVisibility(buttonWidth);
        verify(mLocationBarTablet).setZoomButtonVisibility(true);
        Mockito.clearInvocations(mLocationBarTablet);

        zoomButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setZoomButtonVisibility(false);
        Mockito.clearInvocations(mLocationBarTablet);
    }
}
