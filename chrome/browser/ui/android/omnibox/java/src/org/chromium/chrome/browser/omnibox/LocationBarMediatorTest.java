// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
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
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Rect;
import android.util.Property;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.banners.AppMenuVerbiage;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.toolbar.ToolbarWidthConsumer;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate.AutocompleteLoadCallback;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsContainer;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdown;
import org.chromium.chrome.browser.omnibox.suggestions.SiteSearchActivationSource;
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
import org.chromium.chrome.browser.ui.extensions.ExtensionUi;
import org.chromium.chrome.browser.ui.extensions.ExtensionUiBackend;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.accessibility.PageZoomIndicatorCoordinator;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.AutocompleteState;
import org.chromium.components.omnibox.AutocompleteInput.SiteSearchData;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.webapps.AddToHomescreenCoordinator;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppBannerManagerJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/** Unit tests for LocationBarMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {LocationBarMediatorTest.ObjectAnimatorShadow.class})
@DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
@EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
public class LocationBarMediatorTest {

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
    @Mock private ViewGroup mLocationBarParent;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private OverrideUrlLoadingDelegate mOverrideUrlLoadingDelegate;
    @Mock private LocaleManager mLocaleManager;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    @Mock private LocationBarEmbedder mLocationBarEmbedder;
    @Mock private AutocompleteCoordinator mAutocompleteCoordinator;
    @Mock private UrlBarCoordinator mUrlCoordinator;
    @Mock private StatusCoordinator mStatusCoordinator;
    @Mock private OmniboxPrerender.Natives mPrerenderJni;
    @Mock private TextView mView;
    @Mock private KeyEvent mKeyEvent;
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
    @Mock private LocationBarFocusScrimHandler mScrimHandler;
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
    @Mock private AutocompleteController mAutocompleteController;
    @Mock private ComposeboxQueryControllerBridge mComposeboxBridge;
    @Mock private OmniboxSuggestionsContainer mSuggestionsContainer;
    @Mock private OmniboxSuggestionsDropdown mDropdown;

    @Captor private ArgumentCaptor<Runnable> mRunnableCaptor;
    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mOnInteractionCompletedCallbackCaptor;
    private Callback<Boolean> mOnInteractionCompletedCallback;

    private Context mContext;
    private SettableNonNullObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    private LocationBarMediator mMediator;
    private LocationBarMediator mTabletMediator;
    private UrlBarData mUrlBarData;
    private boolean mIsToolbarMicEnabled;
    private LocationBarEmbedderUiOverrides mUiOverrides;
    private OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier;
    private final SettableNonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier =
            ObservableSuppliers.createNonNull(FuseboxState.EXPANDED);
    private final SettableNonNullObservableSupplier<@FuseboxLayoutMode Integer>
            mFuseboxLayoutModeSupplier =
                    ObservableSuppliers.createNonNull(FuseboxLayoutMode.TOOLBAR);
    private final UserDataHost mTabUserDataHost = new UserDataHost();
    private final FuseboxSessionState mSessionState = new FuseboxSessionState();

    // Members capturing final state of the LocationBarLayout elements.
    private boolean mNavigateButtonIsVisible;

    @Before
    @SuppressWarnings("DirectInvocationOnMock")
    public void setUp() {
        // All lenient() mock actions below should be reevaluated at some point. There is a
        // likelihood some of these are not needed anymore. Infrequent actions should ideally
        // be moved to tests that actually need them.
        // The reason we use lenient() mocks is to suppress abundant "unused action on mock"
        // warnings being emitted any time each of the 140+ tests below is not using that
        // action.
        mTabModelSelectorSupplier = ObservableSuppliers.createNonNull(mTabModelSelector);
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        AutocompleteController.setInstanceForTesting(mAutocompleteController);
        ComposeboxQueryControllerBridge.setInstanceForTesting(mComposeboxBridge);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);

        mUrlBarData = UrlBarData.create(null, "text", 0, 0, "text");
        lenient().doReturn(true).when(mSearchEngineUtils).shouldShowSearchEngineLogo();
        SearchEngineUtils.setInstanceForTesting(mSearchEngineUtils);
        lenient().doReturn(mUrlBarData).when(mLocationBarDataProvider).getUrlBarData();
        lenient()
                .doReturn(ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ false))
                .when(mLocationBarDataProvider)
                .getPrimaryColor();
        lenient().doReturn(mTab).when(mLocationBarDataProvider).getTab();
        lenient().doReturn(mSessionState).when(mLocationBarDataProvider).getFuseboxSessionState();
        lenient()
                .doReturn(mNewTabPageDelegate)
                .when(mLocationBarDataProvider)
                .getNewTabPageDelegate();
        lenient().doReturn(JUnitTestGURLs.BLUE_1).when(mLocationBarDataProvider).getCurrentGurl();
        lenient().doReturn(mWebContents).when(mTab).getWebContents();
        lenient().doReturn(GURL.emptyGURL()).when(mTab).getUrl();
        lenient().doReturn(mRootView).when(mLocationBarLayout).getRootView();
        lenient().doReturn(true).when(mLocationBarLayout).shouldClearTextOnFocus();
        lenient().doReturn(mRootView).when(mLocationBarTablet).getRootView();
        lenient().doReturn(new WeakReference<>(null)).when(mWindowAndroid).getActivity();
        OmniboxPrerenderJni.setInstanceForTesting(mPrerenderJni);
        PreloadPagesSettingsBridgeJni.setInstanceForTesting(mPreloadPagesSettingsJni);
        ResourceRequestBody.setNativesForTesting(mResourceRequestBodyJni);
        lenient().doReturn(mProfile).when(mTab).getProfile();
        lenient()
                .doReturn(mIdentityManager)
                .when(mIdentityServicesProvider)
                .getIdentityManager(mProfile);
        lenient()
                .doReturn(ControlsPosition.TOP)
                .when(mBrowserControlsStateProvider)
                .getControlsPosition();
        lenient().doReturn(mTabUserDataHost).when(mTab).getUserDataHost();
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        mTemplateUrlServiceSupplier = new OneshotSupplierImpl<>();
        mTemplateUrlServiceSupplier.set(mTemplateUrlService);
        mUiOverrides = new LocationBarEmbedderUiOverrides();

        doAnswer(i -> mNavigateButtonIsVisible = i.getArgument(0))
                .when(mLocationBarLayout)
                .setNavigateButtonVisibility(anyBoolean());

        doReturn(mFuseboxStateSupplier).when(mFuseboxCoordinator).getFuseboxStateSupplier();
        doReturn(mFuseboxLayoutModeSupplier)
                .when(mFuseboxCoordinator)
                .getFuseboxLayoutModeSupplier();
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();

        ComposeboxQueryControllerBridge.setInstanceForTesting(mComposeboxBridge);

        AppBannerManagerJni.setInstanceForTesting(mAppBannerManagerJni);
        lenient()
                .doReturn(mAppBannerManager)
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
                        mPageZoomIndicatorCoordinator,
                        mFuseboxCoordinator,
                        mLocationBarEmbedder,
                        /* omniboxChipManager= */ null,
                        mScrimHandler);
        verify(mFuseboxCoordinator)
                .setOnInteractionCompletedCallback(mOnInteractionCompletedCallbackCaptor.capture());
        mOnInteractionCompletedCallback = mOnInteractionCompletedCallbackCaptor.getValue();

        mMediator.setCoordinators(mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);
        mMediator.setAddToHomescreenCoordinatorForTesting(mAddToHomescreenCoordinator);
        ObjectAnimatorShadow.setUrlAnimator(mUrlAnimator);

        mTabletMediator = createTabletMediator();
        mProfileSupplier.set(mProfile);

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
                        mPageZoomIndicatorCoordinator,
                        mFuseboxCoordinator,
                        mLocationBarEmbedder,
                        /* omniboxChipManager= */ null,
                        /* scrimHandler= */ null);
        tabletMediator.setCoordinators(
                mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);
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
    public void testDestroyEndsInput() {
        AutocompleteInput input = mSessionState.getAutocompleteInput();

        mMediator.beginInput(input);
        assertTrue(mSessionState.isSessionActive());
        assertTrue(input.getRequestTypeSupplier().hasObservers());

        mMediator.destroy();
        assertFalse(mSessionState.isSessionActive());
        assertFalse(input.getRequestTypeSupplier().hasObservers());
    }

    @Test
    public void testOnTabLoadingNtp() {
        mMediator.onNtpStartedLoading();
        verify(mLocationBarLayout).onNtpStartedLoading();
    }

    @Test
    public void testBeginInput_StandbyNoFocus() {
        var input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.STANDBY_NO_FOCUS);

        mMediator.beginInput(input);
        verify(mUrlCoordinator, never()).requestFocus();
    }

    @Test
    public void testOnFuseboxInteractionCompleted_StandbyNoFocus() {
        var input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.STANDBY_NO_FOCUS);
        mMediator.beginInput(input);
        clearInvocations(mUrlCoordinator);

        // Simulate an action taken by setting request type to AI_MODE.
        input.setRequestType(AutocompleteRequestType.AI_MODE);

        // Trigger the action (dismiss with action taken)
        mOnInteractionCompletedCallback.onResult(true);

        assertEquals(AutocompleteState.ENABLED, input.getAutocompleteState());
        assertEquals(OmniboxFocusReason.FAKE_BOX_TAP, input.getFocusReason());
        verify(mUrlCoordinator).requestFocus();
    }

    @Test
    public void testOnFuseboxPopupDismissed_StandbyNoFocus() {
        var input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.STANDBY_NO_FOCUS);
        mMediator.beginInput(input);

        // Trigger the dismiss (no action taken)
        mOnInteractionCompletedCallback.onResult(false);

        assertFalse(mSessionState.isSessionActive());
    }

    @Test
    public void testRevertChanges_focused() {
        var state = mSessionState;
        var input = state.getAutocompleteInput();
        input.setUserText("modified text").setInitialUserText("initial text");
        mMediator.beginInput(input);

        mMediator.onUrlFocusChange(true);
        clearInvocations(mUrlCoordinator);

        ArgumentCaptor<UrlBarData> captor = ArgumentCaptor.forClass(UrlBarData.class);
        mMediator.revertChanges();

        verify(mUrlCoordinator).setUrlBarData(captor.capture(), anyInt(), any());

        assertEquals(input.getUserText(), input.getInitialUserText());
        assertEquals(captor.getValue().displayText, input.getInitialUserText());
    }

    @Test
    public void testRevertChanges_unFocused() {
        doReturn(JUnitTestGURLs.BLUE_1).when(mLocationBarDataProvider).getCurrentGurl();
        mMediator.revertChanges();
        verify(mUrlCoordinator)
                .setUrlBarData(mUrlBarData, UrlBar.ScrollType.SCROLL_TO_TLD, UrlBarData.SELECT_ALL);
    }

    @Test
    public void testGetUrlBarDataForCurrentInput_UneditedUrl() {
        AutocompleteInput input = new AutocompleteInput();
        GURL url = JUnitTestGURLs.BLUE_1;
        input.setUserText("www.blue.com").setInitialUserText("www.blue.com").setPageUrl(url);

        UrlBarData data = LocationBarMediator.getUrlBarDataForCurrentInput(input);
        assertEquals(url, data.url);
        assertEquals("www.blue.com", data.displayText.toString());
    }

    @Test
    public void testGetUrlBarDataForCurrentInput_EditedText() {
        AutocompleteInput input = new AutocompleteInput();
        GURL url = JUnitTestGURLs.BLUE_1;
        input.setUserText("user text").setInitialUserText("www.blue.com").setPageUrl(url);

        UrlBarData data = LocationBarMediator.getUrlBarDataForCurrentInput(input);
        assertNull(data.url);
        assertEquals("user text", data.displayText.toString());
    }

    @Test
    public void testGetUrlBarDataForCurrentInput_EmptyText() {
        AutocompleteInput input = new AutocompleteInput();
        GURL url = JUnitTestGURLs.BLUE_1;
        input.setUserText("").setInitialUserText("").setPageUrl(url);

        UrlBarData data = LocationBarMediator.getUrlBarDataForCurrentInput(input);
        assertNull(data.url);
        assertEquals("", data.displayText.toString());
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnSuggestionsChanged() {
        ArgumentCaptor<OmniboxPrerender> omniboxPrerenderCaptor =
                ArgumentCaptor.forClass(OmniboxPrerender.class);
        doReturn(123L).when(mPrerenderJni).init(omniboxPrerenderCaptor.capture());
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        verify(mPrerenderJni).initializeForProfile(123L, mProfile);

        doReturn(PreloadPagesState.NO_PRELOADING)
                .when(mPreloadPagesSettingsJni)
                .getState(eq(mProfile));
        mMediator.beginInput(new AutocompleteInput().setUserText("text"));
        mMediator.onSuggestionsChanged(
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("text")
                        .setIsSearch(true)
                        .setAllowedToBeDefaultMatch(true)
                        .build(),
                true);
        verify(mPrerenderJni, never())
                .prerenderMaybe(anyLong(), anyString(), anyString(), anyLong(), any(), any());
        assertNull(mMediator.getExactMatchUrlSupplier().get());

        doReturn(PreloadPagesState.STANDARD_PRELOADING)
                .when(mPreloadPagesSettingsJni)
                .getState(eq(mProfile));
        GURL url = JUnitTestGURLs.RED_1;
        doReturn(url).when(mLocationBarDataProvider).getCurrentGurl();
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
        mMediator.onSuggestionsChanged(defaultMatch, true);
        verify(mPrerenderJni)
                .prerenderMaybe(123L, "text", JUnitTestGURLs.RED_1.getSpec(), 456L, mProfile, mTab);
        assertNotNull(mMediator.getExactMatchUrlSupplier().get());
        verify(mUrlCoordinator)
                .setAutocompleteText("text", "textWithAutocomplete", "additionalText", null);

        var state = mSessionState;
        state.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        mMediator.onSuggestionsChanged(defaultMatch, true);
        assertNull(mMediator.getExactMatchUrlSupplier().get());
    }

    @Test
    public void testOnSuggestionsChanged_nullMatch() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.beginInput(new AutocompleteInput().setUserText("text"));

        doReturn("text").when(mUrlCoordinator).getTextWithoutAutocomplete();
        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();

        mMediator.onSuggestionsChanged(null, false);
        assertNull(mMediator.getExactMatchUrlSupplier().get());
        verify(mUrlCoordinator).setAutocompleteText("text", null, null, null);
    }

    @Test
    public void testOnUrlTextChanged_updatesShouldAutocomplete() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(true);

        var state = mSessionState;
        var input = state.getAutocompleteInput();

        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();
        mMediator.onUrlTextChanged("test");
        assertTrue(input.shouldAllowUserTextAutocompletion());

        doReturn(false).when(mUrlCoordinator).shouldAutocomplete();
        mMediator.onUrlTextChanged("test2");
        assertFalse(input.shouldAllowUserTextAutocompletion());
    }

    /** Verifies that typing a space after text triggers site search. */
    @Test
    public void testOnUrlTextChangedTypedSpaceTriggersSiteSearch() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(true);

        doReturn(true)
                .when(mAutocompleteCoordinator)
                .triggerSiteSearch(SiteSearchActivationSource.SPACE);

        mMediator.onUrlTextRichChanged(new UrlBarTextChangeInfo("youtube", 0, 0, 7));

        // Simulate user typing space and updating cursor location.
        mMediator.onUrlTextRichChanged(new UrlBarTextChangeInfo("youtube ", 7, 0, 1));

        verify(mAutocompleteCoordinator).triggerSiteSearch(SiteSearchActivationSource.SPACE);
    }

    /** Verifies that pasting text that ends with a space does NOT trigger site search. */
    @Test
    public void testOnUrlTextChangedPastedTextWithSpaceDoesNotTriggerSiteSearch() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(true);

        // Paste "youtube " directly.
        mMediator.onUrlTextRichChanged(new UrlBarTextChangeInfo("youtube ", 0, 0, 8));

        verify(mAutocompleteCoordinator, never())
                .triggerSiteSearch(SiteSearchActivationSource.SPACE);
    }

    /** Verifies that backspacing from "query a" to "query " does NOT trigger site search. */
    @Test
    public void testOnUrlTextChangedBackspaceToSpaceDoesNotTriggerSiteSearch() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(true);

        mMediator.onUrlTextRichChanged(new UrlBarTextChangeInfo("youtube a", 0, 0, 9));
        // Backspace deleted "a", leaving "youtube ". Should not trigger.
        mMediator.onUrlTextRichChanged(new UrlBarTextChangeInfo("youtube ", 8, 1, 0));

        verify(mAutocompleteCoordinator, never())
                .triggerSiteSearch(SiteSearchActivationSource.SPACE);
    }

    @Test
    public void testShouldTriggerSiteSearchScenarios() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(true);

        // Scenario 1: Deletion -> False
        UrlBarTextChangeInfo deleteInfo = new UrlBarTextChangeInfo("text", 4, 1, 0);
        assertFalse(mMediator.shouldTriggerSiteSearch(deleteInfo));

        // Scenario 2: Replacement with non-space -> False
        UrlBarTextChangeInfo replaceNonSpaceInfo = new UrlBarTextChangeInfo("texa", 3, 1, 1);
        assertFalse(mMediator.shouldTriggerSiteSearch(replaceNonSpaceInfo));

        // Scenario 3: Multiple character insertion -> False
        UrlBarTextChangeInfo multiInsertInfo = new UrlBarTextChangeInfo("text  ", 4, 0, 2);
        assertFalse(mMediator.shouldTriggerSiteSearch(multiInsertInfo));

        // Scenario 4: Non-space character -> False
        UrlBarTextChangeInfo nonSpaceInfo = new UrlBarTextChangeInfo("texts", 4, 0, 1);
        assertFalse(mMediator.shouldTriggerSiteSearch(nonSpaceInfo));

        // Scenario 5: Space with empty before -> False
        UrlBarTextChangeInfo spaceEmptyBeforeInfo = new UrlBarTextChangeInfo(" ", 0, 0, 1);
        assertFalse(mMediator.shouldTriggerSiteSearch(spaceEmptyBeforeInfo));

        // Scenario 6: Space with multiple words before -> False
        UrlBarTextChangeInfo spaceMultiWordsInfo =
                new UrlBarTextChangeInfo("word1 word2 ", 11, 0, 1);
        assertFalse(mMediator.shouldTriggerSiteSearch(spaceMultiWordsInfo));

        // Scenario 7: Space with single word before -> True
        UrlBarTextChangeInfo spaceSingleWordInfo = new UrlBarTextChangeInfo("word1 ", 5, 0, 1);
        assertTrue(mMediator.shouldTriggerSiteSearch(spaceSingleWordInfo));

        // Scenario 8: Replacement with space (single word before) -> True
        UrlBarTextChangeInfo replaceWithSpaceInfo =
                new UrlBarTextChangeInfo(
                        "word ", 4, 1, 1); // e.g. replacing '1' in "word1" with ' '
        assertTrue(mMediator.shouldTriggerSiteSearch(replaceWithSpaceInfo));

        // Scenario 9: Leading space -> False
        UrlBarTextChangeInfo leadingSpaceInfo = new UrlBarTextChangeInfo(" keyword ", 8, 0, 1);
        assertFalse(mMediator.shouldTriggerSiteSearch(leadingSpaceInfo));

        // Scenario 10: Space after a trailing space -> False
        UrlBarTextChangeInfo spaceAfterTrailingSpaceInfo =
                new UrlBarTextChangeInfo("word1  ", 6, 0, 1);
        assertFalse(mMediator.shouldTriggerSiteSearch(spaceAfterTrailingSpaceInfo));
    }

    public void testLoadUrl_base() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

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

    @Test
    public void testLoadUrlFromVoice_clearsFocus() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mView).when(mTab).getView();

        mMediator.onUrlFocusChange(true);
        assertTrue(mMediator.isUrlBarFocused());

        mMediator.loadUrlFromVoice(new GURL(TEST_URL));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mView).requestFocus();
        verify(mAutocompleteCoordinator).endInput();
    }

    @Test
    public void testLoadUrl_clearsFocus() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mView).when(mTab).getView();

        mMediator.onUrlFocusChange(true);
        assertTrue(mMediator.isUrlBarFocused());

        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(false)
                        .build());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mView).requestFocus();
        verify(mAutocompleteCoordinator).endInput();
    }

    public void testLoadUrlWithAutocompleteLoadCallback_base() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

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
        mProfileSupplier.set(mProfile);
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
        mProfileSupplier.set(mProfile);
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
        mProfileSupplier.set(mProfile);

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
        mProfileSupplier.set(mProfile);

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        Activity sourceActivity = mock(Activity.class);
        doReturn(sourceActivity).when(mTab).getContext();
        doReturn(1).when(mTab).getParentId();
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewWindow(true)
                        .build());

        verify(mMultiInstanceOrchestrator)
                .openUrlInOtherWindow(
                        eq(sourceActivity),
                        mLoadUrlParamsCaptor.capture(),
                        eq(1),
                        eq(true),
                        eq(false));
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
        reset(mLocationBarDataProvider);
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
    public void testOnConfigurationChanged_qwertyKeyboard() {
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.ENABLED);
        Configuration config = new Configuration();
        OmniboxFeatures.setHasDesktopExperienceForTesting(true); // Adopt Desktop functionality.

        mMediator.beginInput(input);
        mMediator.onConfigurationChanged(config);
        // Do not clear focus if autocomplete is engaged (= the user has likely typed text).
        verify(mUrlCoordinator, never()).clearFocus();

        input.setAutocompleteState(AutocompleteState.STANDBY);
        mMediator.onConfigurationChanged(config);
        // Fall back to standby state and never clear focus, allowing the user to resume session by
        // typing.
        verify(mUrlCoordinator, never()).clearFocus();
    }

    @Test
    public void testOnConfigurationChanged_nonQwertyKeyboard() {
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.ENABLED);
        Configuration config = new Configuration();

        OmniboxFeatures.setHasDesktopExperienceForTesting(false); // non-Desktop functionality.
        mMediator.onConfigurationChanged(config);
        verify(mUrlCoordinator, never()).clearFocus();

        mMediator.beginInput(input);
        // Simulate focus change to make mUrlHasFocus true.
        mMediator.onUrlFocusChange(true);
        mMediator.onConfigurationChanged(config);
        verify(mUrlCoordinator, never()).clearFocus();

        input.setAutocompleteState(AutocompleteState.STANDBY);
        mMediator.onConfigurationChanged(config);
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
        assertFalse(mMediator.onKey(mView, KeyEvent.KEYCODE_BACK, mKeyEvent));

        assertFalse(mMediator.onKey(mView, KeyEvent.KEYCODE_BACK, mKeyEvent));

        verify(
                        mOverrideBackKeyBehaviorDelegate,
                        never().description("should not handle KEYCODE_BACK"))
                .handleBackKeyPressed();
    }

    @Test
    public void testOnKey_del_clearsKeyword() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        AutocompleteInput input = new AutocompleteInput();
        input.setSiteSearchData(new SiteSearchData("keyword", "Search keyword"));
        mMediator.beginInput(input);

        doReturn("").when(mUrlCoordinator).getTextWithoutAutocomplete();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();

        clearInvocations(mUrlCoordinator);

        assertTrue(mMediator.onKey(mView, KeyEvent.KEYCODE_DEL, mKeyEvent));
        FuseboxSessionState state = FuseboxSessionState.from(mLocationBarDataProvider);
        assertNull(state.getAutocompleteInput().getSiteSearchData());

        ArgumentCaptor<UrlBarData> urlBarDataCaptor = ArgumentCaptor.forClass(UrlBarData.class);
        verify(mUrlCoordinator)
                .setUrlBarData(
                        urlBarDataCaptor.capture(),
                        eq(UrlBar.ScrollType.NO_SCROLL),
                        eq(UrlBarData.SELECT_END));
        assertEquals("keyword", urlBarDataCaptor.getValue().displayText.toString());
    }

    @Test
    public void testOnKey_del_clearsKeywordEnteredViaSpace() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        AutocompleteInput input = new AutocompleteInput();
        input.setSiteSearchData(
                new SiteSearchData("keyword", "Search keyword", /* enteredViaSpace= */ true));
        mMediator.beginInput(input);

        doReturn("").when(mUrlCoordinator).getTextWithoutAutocomplete();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();

        clearInvocations(mUrlCoordinator);

        assertTrue(mMediator.onKey(mView, KeyEvent.KEYCODE_DEL, mKeyEvent));
        FuseboxSessionState state = FuseboxSessionState.from(mLocationBarDataProvider);
        assertNull(state.getAutocompleteInput().getSiteSearchData());

        ArgumentCaptor<UrlBarData> urlBarDataCaptor = ArgumentCaptor.forClass(UrlBarData.class);
        verify(mUrlCoordinator)
                .setUrlBarData(
                        urlBarDataCaptor.capture(),
                        eq(UrlBar.ScrollType.NO_SCROLL),
                        eq(UrlBarData.SELECT_END));
        assertEquals("keyword ", urlBarDataCaptor.getValue().displayText.toString());
    }

    @Test
    public void testOnKey_del_withText() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        AutocompleteInput input = new AutocompleteInput();
        input.setSiteSearchData(new SiteSearchData("keyword", "Search keyword"));
        mMediator.beginInput(input);

        doReturn("text").when(mUrlCoordinator).getTextWithoutAutocomplete();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();

        assertFalse(mMediator.onKey(mView, KeyEvent.KEYCODE_DEL, mKeyEvent));
        assertEquals("keyword", input.getSiteSearchData().keyword);
    }

    @Test
    public void testOnKey_del_noKeyword() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        AutocompleteInput input = new AutocompleteInput();
        mMediator.beginInput(input);

        assertFalse(mMediator.onKey(mView, KeyEvent.KEYCODE_DEL, mKeyEvent));
        assertNull(input.getSiteSearchData());
    }

    @Test
    public void testOnKey_escape() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(true);

        var input = mSessionState.getAutocompleteInput();
        input.setUserText("some text");
        input.setInitialUserText("initial text");

        {
            // Step 1: expect suggestions to be cleared if user presses <esc>.
            doReturn(true).when(mAutocompleteCoordinator).isServingSuggestions();
            assertTrue(mMediator.handleEscPress());
            verify(mAutocompleteCoordinator).stopAutocomplete();
            verify(mAutocompleteCoordinator, never()).endInput();
        }

        {
            // Step 2: expect content to be reverted if suggestions are already cleared.
            doReturn(false).when(mAutocompleteCoordinator).isServingSuggestions();
            clearInvocations(mLocationBarLayout);
            assertTrue(mMediator.handleEscPress());
            verify(mLocationBarLayout).setDeleteButtonVisibility(false);
            assertEquals(input.getUserText(), input.getInitialUserText());
            verify(mAutocompleteCoordinator, never()).endInput();
        }

        {
            // Step 3: if both user text and initial user text are same, expect the input to be
            // canceled.
            assertTrue(mMediator.handleEscPress());
            verify(mAutocompleteCoordinator).endInput();
        }

        {
            // Step 4: no other actions can be taken: bail
            assertFalse(mMediator.handleEscPress());
        }
    }

    @Test
    public void testHandleEscPress_fuseboxPopupShowing() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(true);

        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setUserText("some text");

        when(mFuseboxCoordinator.handleHidePopup()).thenReturn(true);

        assertTrue(mMediator.handleEscPress());
        verify(mFuseboxCoordinator).handleHidePopup();
        verify(mAutocompleteCoordinator, never()).stopAutocomplete();
    }

    @Test
    public void testBackKeyPressed_fuseboxPopupShowing() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(true);

        when(mFuseboxCoordinator.handleHidePopup()).thenReturn(true);

        mMediator.backKeyPressed();
        verify(mFuseboxCoordinator).handleHidePopup();
        verify(mOverrideBackKeyBehaviorDelegate, never()).handleBackKeyPressed();
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnKey_right() {
        doReturn(KeyEvent.KEYCODE_DPAD_RIGHT).when(mKeyEvent).getKeyCode();
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
        doReturn(KeyEvent.KEYCODE_DPAD_LEFT).when(mKeyEvent).getKeyCode();
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

        // Typing started will emit suggestions changed.
        mMediator.onSuggestionsChanged(null, false);

        verify(mUrlCoordinator, times(2)).onUrlFocusChange(true);
    }

    @Test
    public void testUpdateColors_lightBrandedColor() {
        doReturn(Color.parseColor("#eaecf0" /*Light grey color*/))
                .when(mLocationBarDataProvider)
                .getPrimaryColor();

        mMediator.updateBrandedColorScheme();

        verify(mLocationBarLayout).setDeleteButtonTint(any(ColorStateList.class));
        verify(mStatusCoordinator).setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        verify(mAutocompleteCoordinator)
                .updateVisualsForState(BrandedColorScheme.LIGHT_BRANDED_THEME);
    }

    @Test
    public void testUpdateColors_darkBrandedColor() {
        doReturn(Color.BLACK).when(mLocationBarDataProvider).getPrimaryColor();

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
        mMediator.updateBrandedColorScheme();

        verify(mLocationBarLayout).setDeleteButtonTint(any(ColorStateList.class));
        verify(mStatusCoordinator).setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        verify(mAutocompleteCoordinator).updateVisualsForState(BrandedColorScheme.APP_DEFAULT);
    }

    @Test
    public void testUpdateColors_setColorScheme() {
        doReturn(Color.BLACK).when(mLocationBarDataProvider).getPrimaryColor();
        var url = JUnitTestGURLs.BLUE_1;
        UrlBarData urlBarData = UrlBarData.forUrl(url);
        doReturn(urlBarData).when(mLocationBarDataProvider).getUrlBarData();
        doReturn(url).when(mLocationBarDataProvider).getCurrentGurl();

        mMediator.updateBrandedColorScheme();
        verify(mLocationBarLayout).setDeleteButtonTint(any());
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
                .setUrlBarData(urlBarData, UrlBar.ScrollType.SCROLL_TO_TLD, UrlBarData.SELECT_ALL);
    }

    @Test
    public void testBeginInput_focusedFromFakebox() {
        mMediator.onFinishNativeInitialization();
        mMediator.beginInput(
                new AutocompleteInput().setFocusReason(OmniboxFocusReason.FAKE_BOX_TAP));
        assertTrue(mMediator.didFocusUrlFromFakebox());
        verify(mUrlCoordinator).requestFocus();
    }

    @Test
    public void testEndInput_notFocused() {
        mMediator.endInput();
        verify(mUrlCoordinator, never()).clearFocus();
    }

    @Test
    public void testBeginInput_NtpAIMode() {
        mMediator.onFinishNativeInitialization();
        mMediator.setProfile(mProfile);

        mMediator.beginInput(
                new AutocompleteInput()
                        .setFocusReason(OmniboxFocusReason.NTP_AI_MODE)
                        .setRequestType(AutocompleteRequestType.AI_MODE));
        verify(mUrlCoordinator).requestFocus();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        ArgumentCaptor<FuseboxSessionState> captor =
                ArgumentCaptor.forClass(FuseboxSessionState.class);
        verify(mFuseboxCoordinator).beginInput(captor.capture());
        verify(mStatusCoordinator).beginInput(captor.getValue());

        assertEquals(
                OmniboxFocusReason.NTP_AI_MODE,
                captor.getValue().getAutocompleteInput().getFocusReason());
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testBeginInput_pastedText() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        mMediator.beginInput(new AutocompleteInput().setUserText("pastedText"));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUrlCoordinator).requestFocus();

        ArgumentCaptor<FuseboxSessionState> captor =
                ArgumentCaptor.forClass(FuseboxSessionState.class);
        verify(mAutocompleteCoordinator).beginInput(captor.capture());
        assertEquals("pastedText", captor.getValue().getAutocompleteInput().getUserText());
    }

    @Test
    public void testOnUrlFocusChange() {
        testOnUrlFocusChange(/* expectDesktopMode= */ false);
    }

    @Test
    public void testOnUrlFocusChange_isNotDesktopMode() {
        OmniboxFeatures.setHasDesktopExperienceForTesting(false);
        testOnUrlFocusChange(/* expectDesktopMode= */ false);
    }

    @Test
    public void testOnUrlFocusChange_hasDesktopExperience() {
        OmniboxFeatures.setHasDesktopExperienceForTesting(true);
        testOnUrlFocusChange(/* expectDesktopMode= */ true);
    }

    @Test
    public void testAnimateIconChanges_bottomToolbar() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsStateProvider).getControlsPosition();
        reset(mStatusCoordinator);
        mMediator.onUrlFocusChange(true);
        verify(mStatusCoordinator).setShouldAnimateIconChanges(false);
    }

    private void testOnUrlFocusChange(boolean expectDesktopMode) {
        mProfileSupplier.set(mProfile);
        doReturn(JUnitTestGURLs.BLUE_1).when(mLocationBarDataProvider).getCurrentGurl();
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.onUrlFocusChange(true);

        assertTrue(mMediator.isUrlBarFocused());
        verify(mStatusCoordinator).setShouldAnimateIconChanges(true);
        verify(mUrlCoordinator)
                .setUrlBarData(
                        any(),
                        eq(UrlBar.ScrollType.NO_SCROLL),
                        eq(expectDesktopMode ? UrlBarData.SELECT_ALL : UrlBarData.SELECT_END));
        verify(mUrlCoordinator).onUrlFocusChange(true);

        mMediator.finishUrlFocusChange(true, true);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnUrlFocusChange_geolocation() {
        int primeCount = sGeoHeaderPrimeCount;
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        mMediator.onUrlFocusChange(true);

        assertEquals(primeCount + 1, sGeoHeaderPrimeCount);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnUrlFocusChange_geolocationPreNative() {
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
                        mPageZoomIndicatorCoordinator,
                        mFuseboxCoordinator,
                        mLocationBarEmbedder,
                        /* omniboxChipManager= */ null,
                        mScrimHandler);
        mMediator.setCoordinators(mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);
        int primeCount = sGeoHeaderPrimeCount;
        mProfileSupplier.set(mProfile);
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        mMediator.onUrlFocusChange(true);

        assertEquals(primeCount, sGeoHeaderPrimeCount);
        templateUrlServiceSupplier.set(mTemplateUrlService);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(primeCount + 1, sGeoHeaderPrimeCount);
    }

    @Test
    public void testOnUrlFocusChange_notFocusedTablet() {
        mProfileSupplier.set(mProfile);
        NewTabPageDelegate newTabPageDelegate = mock(NewTabPageDelegate.class);
        doReturn(newTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        mTabletMediator.addUrlFocusChangeListener(mUrlCoordinator);
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        UrlBarData urlBarData = UrlBarData.create(null, "text", 0, 0, "text");
        doReturn(urlBarData).when(mLocationBarDataProvider).getUrlBarData();
        mTabletMediator.onUrlFocusChange(true);
        reset(mStatusCoordinator);

        mTabletMediator.onUrlFocusChange(false);

        assertFalse(mTabletMediator.isUrlBarFocused());
        verify(mStatusCoordinator).setShouldAnimateIconChanges(false);
        verify(mStatusCoordinator).endInput();
        verify(mUrlCoordinator).onUrlFocusChange(false);
        verify(mUrlCoordinator, atLeastOnce())
                .setUrlBarData(urlBarData, UrlBar.ScrollType.SCROLL_TO_TLD, UrlBarData.SELECT_ALL);
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
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);
        mMediator.beginInput(
                new AutocompleteInput()
                        .setUserText("text")
                        .setFocusReason(OmniboxFocusReason.FAKE_BOX_TAP));
        mMediator.onUrlFocusChange(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        mMediator.setUrlFocusChangeInProgress(false);

        verify(mUrlCoordinator).onUrlAnimationFinished(true);
        verify(mUrlCoordinator).clearFocus();
        // The first invocation of requestFocus() is from setUrlBarFocus, which we use above to set
        // mUrlFocusedFromFakebox to true.
        verify(mUrlCoordinator, times(2)).requestFocus();

        ArgumentCaptor<FuseboxSessionState> captor =
                ArgumentCaptor.forClass(FuseboxSessionState.class);
        verify(mAutocompleteCoordinator, atLeastOnce()).beginInput(captor.capture());
        assertEquals("text", captor.getValue().getAutocompleteInput().getUserText());
    }

    @Test
    public void testMicUpdatedAfterEventTriggered() {
        mMediator.onVoiceAvailabilityImpacted();
        verify(mLocationBarLayout, atLeast(1)).setMicButtonVisibility(false);
        verify(mLocationBarLayout, never()).setMicButtonVisibility(true);

        reset(mLocationBarLayout);
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

    @Test
    public void testOnUrlFocusChange_setEmptyUrl_deleteButtonNotVisible() {
        mMediator.onFinishNativeInitialization();
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(false);
        mMediator.onUrlFocusChange(false);

        InOrder inOrder = inOrder(mUrlCoordinator, mLocationBarLayout);

        mMediator.onUrlFocusChange(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        inOrder.verify(mUrlCoordinator)
                .setUrlBarData(argThat(data -> data.displayText.isEmpty()), anyInt(), any());
        inOrder.verify(mLocationBarLayout).setDeleteButtonVisibility(false);
        inOrder.verify(mLocationBarLayout, never()).setDeleteButtonVisibility(true);
    }

    @Test
    public void testOnUrlFocusChange_setNonEmptyUrl_deleteButtonVisible() {
        mMediator.onFinishNativeInitialization();
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(false);
        mMediator.onUrlFocusChange(false);

        /* Simulate desktop-like behaviour, where the userText is filled in. */
        mSessionState.getAutocompleteInput().setUserText("google.com");
        doReturn("google.com").when(mUrlCoordinator).getTextWithAutocomplete();

        InOrder inOrder = inOrder(mUrlCoordinator, mLocationBarLayout);

        mMediator.onUrlFocusChange(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        inOrder.verify(mUrlCoordinator)
                .setUrlBarData(
                        argThat(data -> "google.com".equals(data.displayText)), anyInt(), any());
        inOrder.verify(mLocationBarLayout).setDeleteButtonVisibility(true);
        inOrder.verify(mLocationBarLayout, never()).setDeleteButtonVisibility(false);
    }

    private void verifyPhoneMicButtonVisibility() {
        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        mMediator.onFinishNativeInitialization();
        reset(mLocationBarLayout);

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
        reset(mLocationBarTablet);

        mTabletMediator.updateButtonVisibility();
        updateTabletWidthConsumers(mTabletMediator);
        ArgumentCaptor<Boolean> captor = ArgumentCaptor.forClass(Boolean.class);
        verify(mLocationBarTablet, atLeastOnce()).setMicButtonVisibility(captor.capture());
        verify(mLocationBarEmbedder, atLeastOnce()).onWidthConsumerVisibilityChanged();
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
        reset(mLocationBarTablet);

        mTabletMediator.updateButtonVisibility();
        updateTabletWidthConsumers(mTabletMediator);
        ArgumentCaptor<Boolean> captor = ArgumentCaptor.forClass(Boolean.class);
        verify(mLocationBarTablet, atLeastOnce()).setLensButtonVisibility(captor.capture());
        verify(mLocationBarEmbedder, atLeastOnce()).onWidthConsumerVisibilityChanged();
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
        verify(mLocationBarEmbedder, atLeastOnce()).onWidthConsumerVisibilityChanged();
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
        reset(mLocationBarTablet);

        mTabletMediator.updateButtonVisibility();
        updateTabletWidthConsumers(mTabletMediator);
        ArgumentCaptor<Boolean> captor = ArgumentCaptor.forClass(Boolean.class);
        verify(mLocationBarTablet, atLeastOnce()).setMicButtonVisibility(captor.capture());
        verify(mLocationBarEmbedder, atLeastOnce()).onWidthConsumerVisibilityChanged();
        assertEquals(shouldBeVisible, captor.getValue());
    }

    @Test
    public void testButtonVisibility_tablet() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mTabletMediator.onFinishNativeInitialization();
        reset(mLocationBarTablet);
        int buttonWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        mTabletMediator
                .getBookmarkButtonToolbarWidthConsumerForTesting()
                .updateVisibility(buttonWidth);
        mTabletMediator.updateButtonVisibility();

        verify(mLocationBarTablet).setMicButtonVisibility(false);
        verify(mLocationBarTablet, times(2)).setBookmarkButtonVisibility(true);
        verify(mLocationBarEmbedder, atLeastOnce()).onWidthConsumerVisibilityChanged();
    }

    @Test
    public void testButtonVisibility_tabletDontShowUnfocused() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mTabletMediator.onFinishNativeInitialization();
        mTabletMediator.setShouldShowButtonsWhenUnfocusedForTablet(false);
        reset(mLocationBarTablet);
        mTabletMediator.updateButtonVisibility();

        verify(mLocationBarTablet).setMicButtonVisibility(false);
        verify(mLocationBarTablet).setBookmarkButtonVisibility(false);
        verify(mLocationBarEmbedder, atLeastOnce()).onWidthConsumerVisibilityChanged();
    }

    @SuppressWarnings("DirectInvocationOnMock")
    public void testRecordHistogramOmniboxClick_Ntp_base() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        // Test clicking omnibox on {@link NewTabPage}.
        doReturn(false)
                .when(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(any(), anyBoolean());

        doReturn(true).when(mTab).isNativePage();
        doReturn(JUnitTestGURLs.NTP_URL).when(mTab).getUrl();
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
        doReturn(JUnitTestGURLs.BLUE_1).when(mTab).getUrl();
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
    public void testOnTouchAfterFocus_triggerUrlFocusChange() {
        mMediator.onFinishNativeInitialization();
        doReturn("").when(mUrlCoordinator).getTextWithoutAutocomplete();
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.onUrlFocusChange(true);
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mMediator.onTouchAfterFocus();
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
        mProfileSupplier.set(mProfile);
        mMediator.onFinishNativeInitialization();
        RobolectricUtil.runAllBackgroundAndUi();

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
        mMediator.setProfile(mProfile);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(true);

        var state = mSessionState;
        state.getAutocompleteInput().setRequestType(AutocompleteRequestType.SEARCH);
        assertTrue(mNavigateButtonIsVisible);

        state.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        assertTrue(mNavigateButtonIsVisible);

        state.getAutocompleteInput().setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        assertTrue(mNavigateButtonIsVisible);
    }

    @Test
    public void testBeginOrResumeInput_updatesModeImmediately() {
        mMediator.onFinishNativeInitialization();
        mMediator.setProfile(mProfile);
        FuseboxSessionState state = mSessionState;

        state.getAutocompleteInput().setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        mMediator.onUrlFocusChange(true);
        verify(mLocationBarLayout).onSpecializedFuseboxModeActivated(true);

        mMediator.onUrlFocusChange(false);
        state.getAutocompleteInput().setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.onUrlFocusChange(true);
        verify(mLocationBarLayout).onSpecializedFuseboxModeActivated(false);
    }

    @Test
    public void testDeleteButtonClicked() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        ArgumentCaptor<FuseboxSessionState> captor =
                ArgumentCaptor.forClass(FuseboxSessionState.class);
        AutocompleteInput input = new AutocompleteInput().setUserText("test query");
        mMediator.beginInput(input);
        mMediator.onUrlFocusChange(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mAutocompleteCoordinator).beginInput(captor.capture());
        assertEquals("test query", captor.getValue().getAutocompleteInput().getUserText());
        clearInvocations(mAutocompleteCoordinator, mUrlCoordinator);

        mMediator.deleteButtonClicked(null);
        assertEquals("", captor.getValue().getAutocompleteInput().getUserText());
        ArgumentCaptor<UrlBarData> urlBarDataCaptor = ArgumentCaptor.forClass(UrlBarData.class);
        verify(mUrlCoordinator).setUrlBarData(urlBarDataCaptor.capture(), anyInt(), any());
        assertTrue(urlBarDataCaptor.getValue().displayText.isEmpty());
        verify(mUrlCoordinator).requestAccessibilityFocus();
    }

    @Test
    public void testInstallButtonClicked() {
        mMediator.installButtonClicked(null);
        verify(mAddToHomescreenCoordinator).showForAppMenu(AppMenuVerbiage.APP_MENU_OPTION_INSTALL);
    }

    @Test
    public void testRestoringText() {
        OmniboxFeatures.setHasDesktopExperienceForTesting(true);
        doReturn(JUnitTestGURLs.NTP_URL).when(mLocationBarDataProvider).getCurrentGurl();
        mTabletMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        // Simulate typing in current tab.
        String newText = "new text";
        mMediator.beginInput(new AutocompleteInput().setUserText(newText));
        ShadowLooper.runUiThreadTasks();

        // Set up and switch to a different tab (we technically only need fusebox session state).
        FuseboxSessionState previousTabSessionState = new FuseboxSessionState();
        doReturn(previousTabSessionState).when(mLocationBarDataProvider).getFuseboxSessionState();
        mTabletMediator.onTabChanged(null);

        // Simulate typing in the other tab.
        String previousText = "previous text";
        mMediator.beginInput(new AutocompleteInput().setUserText(previousText));
        ShadowLooper.runUiThreadTasks();

        // Emulate a tab switch back to original tab (again, fusebox session state suffices).
        doReturn(mSessionState).when(mLocationBarDataProvider).getFuseboxSessionState();
        mTabletMediator.onTabChanged(null);
        ShadowLooper.runUiThreadTasks();

        ArgumentCaptor<FuseboxSessionState> captor =
                ArgumentCaptor.forClass(FuseboxSessionState.class);
        verify(mAutocompleteCoordinator, atLeastOnce()).beginInput(captor.capture());
        assertEquals(newText, captor.getValue().getAutocompleteInput().getUserText());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_IMPROVEMENT_FOR_LFF})
    public void testRestoringTextAndEditingStateOnTablet() {
        OmniboxFeatures.sOmniboxImprovementForLFFPersistEditingState.setForTesting(true);

        // Recreate mediator to respect the overridden feature flag and params.
        mTabletMediator = createTabletMediator();
        mTabletMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        OmniboxFeatures.setHasDesktopExperienceForTesting(true);
        NewTabPageDelegate newTabPageDelegate = mock(NewTabPageDelegate.class);
        doReturn(newTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(JUnitTestGURLs.NTP_URL).when(mLocationBarDataProvider).getCurrentGurl();

        // Prepare a session state to be restored.
        String newText = "new text";
        final int newSelectionStart = 2;
        final int newSelectionEnd = 6;
        var newState = mSessionState;
        newState.getAutocompleteInput().setUserText(newText);
        newState.getAutocompleteInput().setSelection(newSelectionStart, newSelectionEnd);
        newState.activate(mContext, null, mProfileSupplier, null);

        FuseboxSessionState previousState = new FuseboxSessionState();
        doReturn(previousState).when(mLocationBarDataProvider).getFuseboxSessionState();

        // Emulate a state where the omnibox is focused and user has typed a text.
        mTabletMediator.onUrlFocusChange(true);
        String previousText = "previous text";
        final int previousSelectionStart = 1;
        final int previousSelectionEnd = 5;

        // Note: input state is tracked by autocomplete.
        previousState.getAutocompleteInput().setUserText(previousText);
        doReturn(previousSelectionStart).when(mUrlCoordinator).getSelectionStart();
        doReturn(previousSelectionEnd).when(mUrlCoordinator).getSelectionEnd();

        // Restore the original session state.
        doReturn(newState).when(mLocationBarDataProvider).getFuseboxSessionState();
        mTabletMediator.onTabChanged(null);
        mTabletMediator.onUrlChanged(true);

        ArgumentCaptor<FuseboxSessionState> captor =
                ArgumentCaptor.forClass(FuseboxSessionState.class);
        verify(mAutocompleteCoordinator, atLeastOnce()).beginInput(captor.capture());
        assertEquals(newText, captor.getValue().getAutocompleteInput().getUserText());

        // The state for previousTab was saved.
        assertTrue(previousState.isSessionActive());
        assertEquals(previousText, previousState.getAutocompleteInput().getUserText());
        assertEquals(
                previousSelectionStart,
                (int) previousState.getAutocompleteInput().getSelection().getLower());
        assertEquals(
                previousSelectionEnd,
                (int) previousState.getAutocompleteInput().getSelection().getUpper());
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

        reset(mLocationBarLayout, mLocationBarEmbedder);

        mMediator.onInstallabilityUpdated(mAppBannerManager);
        verify(mLocationBarLayout).setInstallButtonVisibility(true);
        verify(mLocationBarEmbedder).onWidthConsumerVisibilityChanged();
    }

    @Test
    public void testInstallButton_invisibleIfNotInstallable() {
        doReturn(false).when(mAppBannerManagerJni).isProbablyPromotable(mWebContents);
        reset(mLocationBarLayout, mLocationBarEmbedder);

        mMediator.onInstallabilityUpdated(mAppBannerManager);
        verify(mLocationBarLayout).setInstallButtonVisibility(false);
        verify(mLocationBarEmbedder).onWidthConsumerVisibilityChanged();
    }

    @Test
    public void testInstallButton_invisibleOmniboxIsFocused() {
        mMediator.onUrlFocusChange(true);
        mMediator.setUrlFocusChangeInProgress(false);

        reset(mLocationBarLayout, mLocationBarEmbedder);

        mMediator.onInstallabilityUpdated(mAppBannerManager);
        verify(mLocationBarLayout).setInstallButtonVisibility(false);
        verify(mLocationBarEmbedder).onWidthConsumerVisibilityChanged();
    }

    @Test
    public void testHintZeroSuggestRefresh_nullTab() {
        doReturn(null).when(mLocationBarDataProvider).getTab();
        mMediator.hintZeroSuggestRefresh();
        verify(mAutocompleteCoordinator).prefetchZeroSuggestResults(null);
    }

    @Test
    public void testZoomButtonClicked() {
        mMediator.onFinishNativeInitialization();
        doReturn(mWebContents).when(mTab).getWebContents();
        mMediator.zoomButtonClicked(null);
        verify(mPageZoomIndicatorCoordinator).show(mWebContents);
    }

    @Test
    public void testShouldShowZoomButton_featureEnabledAndNotDefaultZoom() {
        mMediator.onFinishNativeInitialization();
        verify(mLocationBarLayout, never()).setZoomButtonVisibility(true);
    }

    @Test
    public void testShouldShowZoomButton_featureEnabledAndDefaultZoom() {
        mMediator.onFinishNativeInitialization();
        verify(mLocationBarLayout, never()).setZoomButtonVisibility(false);
    }

    @Test
    public void testShouldShowZoomButton_nullWebContents() {
        mMediator.onFinishNativeInitialization();
        verify(mLocationBarLayout, never()).setZoomButtonVisibility(false);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testUpdateZoomButtonVisibility_popupShowing() {
        mTabletMediator.onFinishNativeInitialization();
        doReturn(mWebContents).when(mTab).getWebContents();
        when(mPageZoomIndicatorCoordinator.isZoomLevelDefault()).thenReturn(false);
        mTabletMediator.updateZoomButtonVisibilityForTesting();

        verify(mLocationBarTablet, atLeastOnce()).setZoomButtonVisibility(true);
        verify(mLocationBarEmbedder, atLeastOnce()).onWidthConsumerVisibilityChanged();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testUpdateZoomButtonVisibility_hideButton() {
        mMediator.onFinishNativeInitialization();
        clearInvocations(mLocationBarEmbedder);

        mMediator.updateZoomButtonVisibilityForTesting();
        verify(mLocationBarLayout).setZoomButtonVisibility(false);
        verify(mLocationBarEmbedder).onWidthConsumerVisibilityChanged();
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
        clearInvocations(mLocationBarTablet);

        micButtonConsumer.updateVisibility(buttonWidth);
        verify(mLocationBarTablet).setMicButtonVisibility(true);
        clearInvocations(mLocationBarTablet);

        micButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setMicButtonVisibility(false);
        clearInvocations(mLocationBarTablet);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_SearchMode_noQuery_showMic() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.SEARCH);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ true);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_SearchMode_withQuery_hideMic() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.SEARCH);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ false);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_AimMode_noQuery_showMic() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ true);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_AimMode_withQuery_hideMic() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ false);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_ImageGenMode_noQuery_showMic() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState
                .getAutocompleteInput()
                .setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ true);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_ImageGenMode_withQuery_hideMic() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState
                .getAutocompleteInput()
                .setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ false);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_DeepSearchMode_noQuery_showMic() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.DEEP_SEARCH);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ true);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_DeepSearchMode_withQuery_hideMic() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.DEEP_SEARCH);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ false);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_CanvasMode_noQuery_showMic() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.CANVAS);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ true);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_CanvasMode_withQuery_hideMic() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.CANVAS);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ false);
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
        clearInvocations(mLocationBarTablet);

        lensButtonConsumer.updateVisibility(buttonWidth);
        verify(mLocationBarTablet).setLensButtonVisibility(true);
        clearInvocations(mLocationBarTablet);

        lensButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setLensButtonVisibility(false);
        clearInvocations(mLocationBarTablet);
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
        clearInvocations(mLocationBarTablet);

        bookmarkButtonConsumer.updateVisibility(buttonWidth);
        assertTrue(mTabletMediator.shouldShowBookmarkButton());
        verify(mLocationBarTablet).setBookmarkButtonVisibility(true);
        clearInvocations(mLocationBarTablet);

        bookmarkButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setBookmarkButtonVisibility(false);
        clearInvocations(mLocationBarTablet);
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
        clearInvocations(mLocationBarTablet);

        installButtonConsumer.updateVisibility(buttonWidth);
        verify(mLocationBarTablet).setInstallButtonVisibility(true);
        clearInvocations(mLocationBarTablet);

        installButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setInstallButtonVisibility(false);
        clearInvocations(mLocationBarTablet);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testZoomButtonToolbarWidthConsumer_notVisible() {
        int buttonWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        mTabletMediator.onFinishNativeInitialization();
        when(mPageZoomIndicatorCoordinator.isZoomLevelDefault()).thenReturn(true);
        assertFalse(mTabletMediator.shouldShowZoomButton());

        ToolbarWidthConsumer zoomButtonConsumer =
                mTabletMediator.getZoomButtonToolbarWidthConsumer();
        clearInvocations(mLocationBarTablet);

        zoomButtonConsumer.updateVisibility(buttonWidth);
        verify(mLocationBarTablet).setZoomButtonVisibility(false);
        clearInvocations(mLocationBarTablet);

        zoomButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setZoomButtonVisibility(false);
        clearInvocations(mLocationBarTablet);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testZoomButtonToolbarWidthConsumer() {
        int buttonWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        mTabletMediator.onFinishNativeInitialization();
        when(mPageZoomIndicatorCoordinator.isZoomLevelDefault()).thenReturn(false);
        assertTrue(mTabletMediator.shouldShowZoomButton());

        ToolbarWidthConsumer zoomButtonConsumer =
                mTabletMediator.getZoomButtonToolbarWidthConsumer();
        clearInvocations(mLocationBarTablet, mLocationBarEmbedder);

        zoomButtonConsumer.updateVisibility(buttonWidth);
        verify(mLocationBarTablet).setZoomButtonVisibility(true);
        verify(mLocationBarEmbedder, never()).onWidthConsumerVisibilityChanged();
        clearInvocations(mLocationBarTablet, mLocationBarEmbedder);

        zoomButtonConsumer.updateVisibility(0);
        verify(mLocationBarTablet).setZoomButtonVisibility(false);
        verify(mLocationBarEmbedder, never()).onWidthConsumerVisibilityChanged();
        clearInvocations(mLocationBarTablet, mLocationBarEmbedder);
    }

    @Test
    public void testOnSearchBoxHintTextChanged_UpdatesHintText() {
        mProfileSupplier.set(mProfile);
        mMediator.onFinishNativeInitialization();
        RobolectricUtil.runAllBackgroundAndUi();

        doReturn("search engine hint text")
                .when(mSearchEngineUtils)
                .getOmniboxHintText(anyInt(), any());

        mMediator.onSearchBoxHintTextChanged();

        verify(mUrlCoordinator).setUrlBarHintText(eq("search engine hint text"));

        mMediator.onUrlFocusChange(false);
        doReturn("search engine hint text unfocused")
                .when(mSearchEngineUtils)
                .getOmniboxHintText(anyInt(), any());
    }

    @Test
    public void testOnSearchBoxHintTextChanged_EmbedderControlledHint_DoesNotUpdateHintText() {
        mUiOverrides.setEmbedderControlledHint(true);

        mProfileSupplier.set(mProfile);
        mMediator.onFinishNativeInitialization();
        RobolectricUtil.runAllBackgroundAndUi();

        mMediator.onSearchBoxHintTextChanged();

        verify(mUrlCoordinator, never()).setUrlBarHintText(any());
    }

    @Test
    public void testEndInputResetsHint() {
        mProfileSupplier.set(mProfile);
        mMediator.onFinishNativeInitialization();
        RobolectricUtil.runAllBackgroundAndUi();

        String searchHint = "search or something";
        doReturn(searchHint)
                .when(mSearchEngineUtils)
                .getOmniboxHintText(eq(AutocompleteRequestType.SEARCH), any());
        String aiHint = "ai or something";
        doReturn(aiHint)
                .when(mSearchEngineUtils)
                .getOmniboxHintText(eq(AutocompleteRequestType.AI_MODE), any());

        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        clearInvocations(mUrlCoordinator);

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        verify(mUrlCoordinator).setUrlBarHintText(eq(aiHint));

        clearInvocations(mUrlCoordinator);
        mMediator.onUrlFocusChange(/* hasFocus= */ false);
        verify(mUrlCoordinator).setUrlBarHintText(eq(searchHint));
    }

    @Test
    public void testLoadUrl_chromeExtensionScheme() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        ExtensionUiBackend mockExtensionUiBackend = mock(ExtensionUiBackend.class);
        ExtensionUi.setBackendForTesting(mockExtensionUiBackend);

        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        String url = UrlConstants.CHROME_EXTENSION_SCHEME + "://id/?q=test";
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(url, PageTransition.TYPED)
                        .setOpenInNewTab(true)
                        .build());

        verify(mockExtensionUiBackend)
                .onOmniboxExtensionInputEntered(mWebContents, url, true, false);
        verify(mTab, never()).loadUrl(any());
        verify(mTabModelSelector, never()).openNewTab(any(), anyInt(), any(), anyBoolean());
        verify(mMultiInstanceOrchestrator, never())
                .openUrlInOtherWindow(any(), any(), anyInt(), anyBoolean(), anyBoolean());

        ExtensionUi.setBackendForTesting(null);
    }

    @Test
    public void testOnBackButtonClicked() {
        doReturn(true).when(mTab).canGoBack();
        mMediator.onBackButtonClicked();
        verify(mTab).goBack();
    }

    @Test
    public void testBackButtonClicked_cannotGoBack() {
        doReturn(false).when(mTab).canGoBack();
        mMediator.onBackButtonClicked();
        verify(mTab, never()).goBack();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testUpdateBackButtonVisibility_visible() {
        clearInvocations(mLocationBarLayout);
        mMediator.updateBackButtonVisibility();
        verify(mLocationBarLayout).setBackButtonVisibility(true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testUpdateBackButtonVisibility_hidden() {
        clearInvocations(mLocationBarLayout);
        mMediator.updateBackButtonVisibility();
        verify(mLocationBarLayout).setBackButtonVisibility(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testUpdateBackButtonVisibility_hiddenWhenFocused() {
        mMediator.onUrlFocusChange(true);
        clearInvocations(mLocationBarLayout);
        mMediator.updateBackButtonVisibility();
        verify(mLocationBarLayout).setBackButtonVisibility(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testUpdateBackButtonVisibility_hiddenOnNtp() {
        doReturn(new GURL("chrome://newtab/")).when(mTab).getUrl();
        clearInvocations(mLocationBarLayout);
        mMediator.updateBackButtonVisibility();
        verify(mLocationBarLayout).setBackButtonVisibility(false);
    }

    @Test
    public void testOnSuggestionsChanged_triggersScrimVisibility() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.beginInput(new AutocompleteInput());

        // Show scrim in all contexts if there are any suggestions to show.
        mMediator.onSuggestionsChanged(null, true);
        verify(mScrimHandler).setVisibility(true);
        clearInvocations(mScrimHandler);

        // Show scrim on mobile devices even if there are no suggestions to show.
        OmniboxFeatures.setHasDesktopExperienceForTesting(false);
        mMediator.onSuggestionsChanged(null, false);
        verify(mScrimHandler).setVisibility(true);
        clearInvocations(mScrimHandler);

        // On desktop, we show no suggestions in select cases, e.g. on the NTP where the omnibox is
        // prefocused. We don't want to show the scrim in that scenario either.
        OmniboxFeatures.setHasDesktopExperienceForTesting(true);
        mMediator.beginInput(
                new AutocompleteInput().setAutocompleteState(AutocompleteState.STANDBY));
        verify(mScrimHandler).setVisibility(false);
        clearInvocations(mScrimHandler);
        mMediator.onSuggestionsChanged(null, false);
        verify(mScrimHandler).setVisibility(false);
        clearInvocations(mScrimHandler);
    }

    @Test
    public void testBeginInput_triggersScrimUpdate() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        mMediator.beginInput(new AutocompleteInput().setUserText("test"));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mScrimHandler).updateScrimVisualState();
    }

    @Test
    public void testOnPrimaryColorChanged_updatesBackButtonAndOptionalButtonColors() {
        int[] callCount = new int[] {0};
        Callback<ColorStateList> callback = (colorStateList) -> callCount[0]++;
        mMediator.setOptionalButtonColorChangeCallback(callback);

        verify(mLocationBarLayout).setBackButtonTint(any());
        assertEquals(1, callCount[0]);
    }

    @Test
    public void testReparenting_notEnabled() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        assertFalse(mMediator.isParentedToSuggestionsContainer());
        mMediator.handleUrlFocusAnimation(true);
        assertFalse(mMediator.isParentedToSuggestionsContainer());
        mMediator.handleUrlFocusAnimation(false);
        assertFalse(mMediator.isParentedToSuggestionsContainer());
    }

    @Test
    public void testReparenting() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);

        doReturn(mLocationBarParent).when(mLocationBarLayout).getParent();
        doReturn(mSuggestionsContainer).when(mAutocompleteCoordinator).getSuggestionsContainer();
        doReturn(mDropdown).when(mSuggestionsContainer).takeDropdownView();
        MarginLayoutParams layoutParams = new MarginLayoutParams(-2, -2);
        doReturn(layoutParams).when(mLocationBarLayout).getLayoutParams();
        View placeholder = Mockito.mock(View.class);
        doReturn(placeholder)
                .when(mLocationBarLayout)
                .findViewById(R.id.suggestions_container_placeholder);
        int placeholderIndex = 2;
        doReturn(placeholderIndex).when(mLocationBarLayout).indexOfChild(placeholder);

        mMediator.handleUrlFocusAnimation(true);
        assertTrue(mMediator.isParentedToSuggestionsContainer());
        assertEquals(MarginLayoutParams.MATCH_PARENT, layoutParams.width);
        assertEquals(MarginLayoutParams.MATCH_PARENT, layoutParams.height);
        verify(mSuggestionsContainer).addView(mLocationBarLayout, 0, layoutParams);
        verify(mLocationBarLayout).addView(mDropdown, placeholderIndex);
        verify(mUrlCoordinator).startReparenting();
        verify(mUrlCoordinator).finishReparenting(true);

        mMediator.endInput();
        assertFalse(mMediator.isParentedToSuggestionsContainer());
        verify(mSuggestionsContainer).removeView(mLocationBarLayout);
        verify(mLocationBarParent).addView(mLocationBarLayout, 0, layoutParams);
        assertEquals(MarginLayoutParams.MATCH_PARENT, layoutParams.width);
        assertEquals(MarginLayoutParams.MATCH_PARENT, layoutParams.height);
        verify(mLocationBarLayout).removeView(mDropdown);
        verify(mUrlCoordinator, times(2)).startReparenting();
        verify(mUrlCoordinator).finishReparenting(false);
    }
}
