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
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
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
import android.view.Window;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

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
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.banners.AppMenuVerbiage;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksUtils;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksUtilsJni;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.toolbar.ToolbarWidthConsumer;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider.AppInstallState;
import org.chromium.chrome.browser.omnibox.LocationBarMediator.OmniboxUma;
import org.chromium.chrome.browser.omnibox.OmniboxPrerender.Natives;
import org.chromium.chrome.browser.omnibox.SearchEngineService.SearchEngineNameObserver;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridgeJni;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.PopupState;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator.NavigationTarget;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate.AutocompleteLoadCallback;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxAnimator;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsContainer;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdown;
import org.chromium.chrome.browser.omnibox.suggestions.SelectionController.TraversalMode;
import org.chromium.chrome.browser.omnibox.suggestions.SiteSearchActivationSource;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.preferences.Pref;
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
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.accessibility.PageZoomIndicatorCoordinator;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.AutocompleteState;
import org.chromium.components.omnibox.AutocompleteInput.DisplayState;
import org.chromium.components.omnibox.AutocompleteInput.SiteSearchData;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.webapps.AddToHomescreenCoordinator;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppBannerManagerJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.accessibility.AccessibilityStateTestHelper;
import org.chromium.ui.base.DeviceInput;
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
@Config(
        shadows = {LocationBarMediatorUnitTest.ObjectAnimatorShadow.class},
        qualifiers = "w1000dp")
@DisableFeatures({OmniboxFeatureList.OMNIBOX_SEARCH_PREFETCH_ON_ENTER_KEY_DOWN})
@EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
public class LocationBarMediatorUnitTest {

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
    private static final String TEST_USER_TEXT = "user query";
    private static final String TEST_INITIAL_USER_TEXT = "inital text";

    private static int sGeoHeaderPrimeCount;
    private static int sGeoHeaderStopCount;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

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
    @Mock private Natives mPrerenderJni;
    @Mock private TextView mView;
    @Mock private View mTabView;
    @Mock private KeyEvent mKeyEvent;
    @Mock private BackKeyBehaviorDelegate mOverrideBackKeyBehaviorDelegate;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ObjectAnimator mUrlAnimator;
    @Mock private View mRootView;
    @Mock private SearchEngineService mSearchEngineService;
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
    @Mock private PrefService mPrefService;
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJni;
    @Mock private PreloadPagesSettingsBridge.Natives mPreloadPagesSettingsJni;
    @Mock private OmniboxUma mOmniboxUma;
    @Mock private OmniboxSuggestionsDropdownEmbedderImpl mEmbedderImpl;
    @Mock private ResourceRequestBody.Natives mResourceRequestBodyJni;
    @Mock private ContextualTasksUtils.Natives mContextualTasksUtilsJni;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private AppBannerManager mAppBannerManager;
    @Mock private AppBannerManager.Natives mAppBannerManagerJni;
    @Mock private NewTabPageDelegate mNewTabPageDelegate;
    @Mock private FuseboxCoordinator mFuseboxCoordinator;
    @Mock private FuseboxAttachmentModelList mFuseboxAttachmentModelList;
    @Mock private AutocompleteController mAutocompleteController;
    @Mock private ComposeboxQueryControllerBridge mComposeboxBridge;
    @Mock private ComposeboxQueryControllerBridge.Natives mComposeboxBridgeJni;
    @Mock private OmniboxSuggestionsContainer mSuggestionsContainer;
    @Mock private OmniboxSuggestionsDropdown mDropdown;
    @Mock private VoiceRecognitionHandler mVoiceRecognitionHandler;
    @Mock private View mUrlBar;
    @Mock private View mDeleteButton;
    @Mock private ChipView mActivationChip;
    @Mock private View mMicButton;
    @Mock private View mNavigateButton;
    @Mock private View mPlusButton;
    @Mock private View mFocusThief;
    @Mock private Activity mActivity;
    @Mock private Window mWindow;
    @Mock private ExtensionUiBackend mExtensionUiBackend;
    @Mock private View mPlaceholder;
    @Mock private AutocompleteMatch mAutocompleteMatch;
    @Mock private Tracker mTracker;

    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mOnInteractionCompletedCallbackCaptor;
    @Captor private ArgumentCaptor<UrlBarData> mUrlBarDataCaptor;
    @Captor private ArgumentCaptor<OmniboxPrerender> mOmniboxPrerenderCaptor;
    @Captor private ArgumentCaptor<OmniboxLoadUrlParams> mOmniboxLoadUrlParamsCaptor;
    @Captor private ArgumentCaptor<FuseboxSessionState> mFuseboxSessionStateCaptor;
    @Captor private ArgumentCaptor<Boolean> mBooleanCaptor;
    @Captor private ArgumentCaptor<SearchEngineNameObserver> mObserverCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mCallbackCaptor;
    @Captor private ArgumentCaptor<View.OnLayoutChangeListener> mOnLayoutChangeListenerCaptor;

    private Callback<Boolean> mOnInteractionCompletedCallback;
    private Context mContext;
    private OmniboxResourceProvider mOmniboxResourceProvider;
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
    private final SettableNonNullObservableSupplier<Boolean> mWindowHasFocusSupplier =
            ObservableSuppliers.createNonNull(true);
    private final SettableNonNullObservableSupplier<Boolean> mHasAttachmentsSupplier =
            ObservableSuppliers.createNonNull(false);
    private final UserDataHost mTabUserDataHost = new UserDataHost();
    private final FuseboxSessionState mSessionState = new FuseboxSessionState();
    private final SettableNonNullObservableSupplier<Boolean> mScrimVisibilitySupplier =
            ObservableSuppliers.createNonNull(false);
    private final OmniboxAnimator mOmniboxAnimator = new OmniboxAnimator(1.0f, 0);

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
        mOmniboxAnimator.setFloatValues(1.0f);
        mTabModelSelectorSupplier = ObservableSuppliers.createNonNull(mTabModelSelector);
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mOmniboxResourceProvider =
                new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);

        UserPrefs.setPrefServiceForTesting(mPrefService);
        ComposeplateUtils.setIsEnabledForTesting(true);
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJni);
        lenient().doReturn(1L).when(mPrefChangeRegistrarJni).init(any(), any());
        lenient().doReturn(mProfile).when(mProfile).getOriginalProfile();
        lenient().doReturn(true).when(mPrefService).getBoolean(Pref.SHOW_AI_MODE_OMNIBOX_BUTTON);

        AutocompleteController.setInstanceForTesting(mAutocompleteController);
        TrackerFactory.setTrackerForTests(mTracker);
        ComposeboxQueryControllerBridge.setInstanceForTesting(mComposeboxBridge);
        ComposeboxQueryControllerBridgeJni.setInstanceForTesting(mComposeboxBridgeJni);
        lenient().doReturn(true).when(mComposeboxBridgeJni).isFuseboxEligibleForProfile(any());
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);

        mUrlBarData =
                UrlBarData.create(
                        /* url= */ null,
                        /* displayText= */ "text",
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 0,
                        /* editingText= */ "text");
        lenient().doReturn(true).when(mSearchEngineService).shouldShowSearchEngineLogo();
        lenient().doReturn(true).when(mSearchEngineService).isDefaultSearchEngineGoogle();
        lenient().doReturn("Google").when(mSearchEngineService).getSearchEngineName();
        lenient()
                .doReturn("Search Google or type URL")
                .when(mSearchEngineService)
                .getOmniboxHintString();
        SearchEngineService.setInstanceForTesting(mSearchEngineService);
        lenient().doReturn(mUrlBarData).when(mLocationBarDataProvider).getUrlBarData();
        lenient()
                .doReturn(ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ false))
                .when(mLocationBarDataProvider)
                .getPrimaryColor();
        lenient().doReturn(mTab).when(mLocationBarDataProvider).getTab();
        lenient().doReturn(true).when(mLocationBarDataProvider).hasTab();
        lenient().doReturn(mTabView).when(mTab).getView();
        lenient()
                .doReturn(PageClassification.OTHER)
                .when(mLocationBarDataProvider)
                .getPageClassification(anyBoolean());
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
        lenient().doReturn(mWindow).when(mActivity).getWindow();
        lenient().doReturn(true).when(mWindow).isActive();
        lenient().doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();
        OmniboxPrerenderJni.setInstanceForTesting(mPrerenderJni);
        PreloadPagesSettingsBridgeJni.setInstanceForTesting(mPreloadPagesSettingsJni);
        ContextualTasksUtilsJni.setInstanceForTesting(mContextualTasksUtilsJni);
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

        lenient()
                .doAnswer(i -> mNavigateButtonIsVisible = i.getArgument(0))
                .when(mLocationBarLayout)
                .setNavigateButtonVisibility(anyBoolean());

        lenient()
                .doReturn(mFuseboxStateSupplier)
                .when(mFuseboxCoordinator)
                .getFuseboxStateSupplier();
        lenient()
                .doReturn(mHasAttachmentsSupplier)
                .when(mFuseboxCoordinator)
                .getHasAttachmentsSupplier();
        lenient()
                .doReturn(mFuseboxLayoutModeSupplier)
                .when(mFuseboxCoordinator)
                .getFuseboxLayoutModeSupplier();
        lenient().doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();

        ComposeboxQueryControllerBridge.setInstanceForTesting(mComposeboxBridge);

        AppBannerManagerJni.setInstanceForTesting(mAppBannerManagerJni);
        lenient()
                .doReturn(mAppBannerManager)
                .when(mAppBannerManagerJni)
                .getJavaBannerManagerForWebContents(mWebContents);
        lenient()
                .doReturn(mScrimVisibilitySupplier)
                .when(mScrimHandler)
                .getScrimVisibilitySupplier();
        lenient().doReturn(mUrlBar).when(mLocationBarLayout).getUrlBar();
        lenient().doReturn(mDeleteButton).when(mLocationBarLayout).getDeleteButton();
        lenient().doReturn(mActivationChip).when(mLocationBarLayout).getActivationChip();
        lenient()
                .doReturn(mPlusButton)
                .when(mLocationBarLayout)
                .findViewById(R.id.fusebox_plus_button);
        lenient().doReturn(mMicButton).when(mLocationBarLayout).getMicButton();
        lenient().doReturn(mNavigateButton).when(mLocationBarLayout).getNavigateButton();
        lenient().doReturn(mFocusThief).when(mLocationBarLayout).getFocusThief();

        mMediator =
                new LocationBarMediator(
                        mContext,
                        mLocationBarLayout,
                        mLocationBarDataProvider,
                        mOmniboxResourceProvider,
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
                        mScrimHandler,
                        mWindowHasFocusSupplier);
        verify(mFuseboxCoordinator)
                .setOnInteractionCompletedCallback(mOnInteractionCompletedCallbackCaptor.capture());
        mOnInteractionCompletedCallback = mOnInteractionCompletedCallbackCaptor.getValue();
        lenient().doReturn(true).when(mScrimHandler).isScrimShown();
        lenient()
                .doReturn(mOmniboxAnimator)
                .when(mAutocompleteCoordinator)
                .setupSuggestionsListShowAnimation();

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
        mSessionState
                .getAutocompleteInput()
                .getAutocompleteStateSupplier()
                .addSyncObserver(
                        state -> {
                            if (mAutocompleteCoordinator == null) return;
                            if (state == AutocompleteState.STANDBY) {
                                mAutocompleteCoordinator.stopAutocomplete();
                                mAutocompleteCoordinator.endInput();
                                if (mSessionState.getAutocompleteInput().getDisplayState()
                                        == DisplayState.SUGGESTIONS) {
                                    mMediator.onSuggestionsChanged(
                                            null, /* hasSuggestions= */ false);
                                }
                            } else if (state == AutocompleteState.DISABLED) {
                                mAutocompleteCoordinator.endInput();
                            }
                        });
    }

    private LocationBarMediator createTabletMediator() {
        lenient().doReturn(mUrlBar).when(mLocationBarTablet).getUrlBar();
        lenient().doReturn(mDeleteButton).when(mLocationBarTablet).getDeleteButton();
        lenient().doReturn(mActivationChip).when(mLocationBarTablet).getActivationChip();
        lenient()
                .doReturn(mPlusButton)
                .when(mLocationBarTablet)
                .findViewById(R.id.fusebox_plus_button);
        lenient().doReturn(mMicButton).when(mLocationBarTablet).getMicButton();
        lenient().doReturn(mNavigateButton).when(mLocationBarTablet).getNavigateButton();
        lenient().doReturn(mFocusThief).when(mLocationBarTablet).getFocusThief();

        var tabletMediator =
                new LocationBarMediator(
                        mContext,
                        mLocationBarTablet,
                        mLocationBarDataProvider,
                        mOmniboxResourceProvider,
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
                        /* scrimHandler= */ null,
                        mWindowHasFocusSupplier);
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

    private void setUpMediatorAndCoordinator() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        lenient().doReturn(true).when(mAutocompleteCoordinator).hasAutocompleteController();
        lenient().doReturn(null).when(mAutocompleteCoordinator).getSuggestionsContainer();
    }

    private void setUpEnterKeyEvent(long eventTime) {
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(KeyEvent.KEYCODE_ENTER).when(mKeyEvent).getKeyCode();
        lenient().doReturn(eventTime).when(mKeyEvent).getEventTime();
    }

    private void assertAutocompleteState(@AutocompleteState int state) {
        assertEquals(state, mSessionState.getAutocompleteInput().getAutocompleteState());
    }

    private void assertDisplayState(@DisplayState int state) {
        assertEquals(state, mSessionState.getAutocompleteInput().getDisplayState());
    }

    private void assertUserText(String text) {
        assertEquals(text, mSessionState.getAutocompleteInput().getUserText());
    }

    private void assertDraftingNoFocusProperties() {
        assertTrue(mSessionState.isSessionActive());
        assertDisplayState(DisplayState.DRAFTING_NO_FOCUS);
        assertAutocompleteState(AutocompleteState.STANDBY);
        assertFalse(mMediator.isUrlBarFocused());
    }

    private void beginInput(AutocompleteInput input) {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.beginInput(input);
    }

    private void setupSession(@DisplayState int displayState, boolean textDiffers) {
        String userText = TEST_USER_TEXT;
        String initialText = textDiffers ? TEST_INITIAL_USER_TEXT : userText;
        AutocompleteInput input = new AutocompleteInput().setDisplayState(displayState);
        beginInput(input);
        // We set the text after beginInput so that they aren't overwritten on activation.
        mSessionState.getAutocompleteInput().setUserText(userText).setInitialUserText(initialText);
        assertEquals(textDiffers, mMediator.userTextDiffersFromInitial());
        assertDisplayState(displayState);
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
    public void testDisplayStateChanged_updatesSelectionMode() {
        LocationBarSelectionController selectionController =
                mMediator.getSelectionControllerForTesting();
        assertEquals(TraversalMode.SATURATING, selectionController.getSelectionModeForTesting());

        AutocompleteInput input = mSessionState.getAutocompleteInput();
        mMediator.beginInput(input);
        assertEquals(TraversalMode.SATURATING, selectionController.getSelectionModeForTesting());

        input.setDisplayState(DisplayState.SUGGESTIONS);
        assertEquals(TraversalMode.WRAPPING, selectionController.getSelectionModeForTesting());

        input.setDisplayState(DisplayState.DRAFTING);
        assertEquals(TraversalMode.SATURATING, selectionController.getSelectionModeForTesting());
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

        assertAutocompleteState(AutocompleteState.ENABLED);
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

        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        clearInvocations(mUrlCoordinator);

        mMediator.revertChanges();

        verify(mUrlCoordinator).setUrlBarData(mUrlBarDataCaptor.capture(), anyInt(), any());

        assertEquals(input.getUserText(), input.getInitialUserText());
        assertEquals(mUrlBarDataCaptor.getValue().displayText, input.getInitialUserText());
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());
    }

    @Test
    public void testRevertChanges_unFocused() {
        doReturn(JUnitTestGURLs.BLUE_1).when(mLocationBarDataProvider).getCurrentGurl();
        mMediator.revertChanges();
        verify(mUrlCoordinator)
                .setUrlBarData(mUrlBarData, ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_ALL);
    }

    @Test
    public void testGetUrlBarDataForCurrentInput_UneditedUrl() {
        AutocompleteInput input = new AutocompleteInput();
        GURL url = JUnitTestGURLs.BLUE_1;
        input.setUserText("www.blue.com").setInitialUserText("www.blue.com").setPageUrl(url);

        UrlBarData data = mMediator.getUrlBarDataForCurrentInput(input);
        assertEquals(url, data.url);
        assertEquals("www.blue.com", data.displayText.toString());
    }

    @Test
    public void testGetUrlBarDataForCurrentInput_EditedText() {
        AutocompleteInput input = new AutocompleteInput();
        GURL url = JUnitTestGURLs.BLUE_1;
        input.setUserText("user text").setInitialUserText("www.blue.com").setPageUrl(url);

        UrlBarData data = mMediator.getUrlBarDataForCurrentInput(input);
        assertNull(data.url);
        assertEquals("user text", data.displayText.toString());
    }

    @Test
    public void testGetUrlBarDataForCurrentInput_EmptyText() {
        AutocompleteInput input = new AutocompleteInput();
        GURL url = JUnitTestGURLs.BLUE_1;
        input.setUserText("").setInitialUserText("").setPageUrl(url);

        UrlBarData data = mMediator.getUrlBarDataForCurrentInput(input);
        assertNull(data.url);
        assertEquals("", data.displayText.toString());
    }

    @Test
    public void testContextualTasks_UrlBarData_PrettyUrl() {
        GURL contextualTasksUrl = new GURL("chrome://contextual-tasks");
        GURL prettyUrl = new GURL("chrome://google.com/search");
        doReturn(contextualTasksUrl).when(mLocationBarDataProvider).getCurrentGurl();
        doReturn(mWebContents).when(mLocationBarDataProvider).getWebContents();
        doReturn(prettyUrl)
                .when(mContextualTasksUtilsJni)
                .getContextualTasksDisplayUrl(mWebContents);

        AutocompleteInput input = new AutocompleteInput();
        input.setUserText("chrome://google.com/search")
                .setInitialUserText("chrome://google.com/search")
                .setPageUrl(contextualTasksUrl);

        UrlBarData data = mMediator.getUrlBarDataForCurrentInput(input);
        assertEquals(contextualTasksUrl, data.url);
        assertEquals(prettyUrl.getSpec(), data.displayText.toString());
    }

    @Test
    public void testContextualTasks_Copy() {
        GURL contextualTasksUrl = new GURL("chrome://contextual-tasks");
        GURL functionalUrl = new GURL("https://www.google.com/search");
        String text = "chrome://www.google.com/search";

        doReturn(contextualTasksUrl).when(mLocationBarDataProvider).getCurrentGurl();
        doReturn(mWebContents).when(mLocationBarDataProvider).getWebContents();
        doReturn(functionalUrl)
                .when(mContextualTasksUtilsJni)
                .getContextualTasksFunctionalURL(mWebContents);
        doReturn(functionalUrl.getSpec())
                .when(mContextualTasksUtilsJni)
                .getReplacementUrl(eq(text), anyInt(), anyInt(), eq(functionalUrl));

        assertEquals(
                functionalUrl.getSpec(),
                mMediator.getReplacementCutCopyText(text, new TextSelection(0, text.length())));
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnSuggestionsChanged() {
        doReturn(123L).when(mPrerenderJni).init(mOmniboxPrerenderCaptor.capture());
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

        doReturn(PreloadPagesState.STANDARD_PRELOADING)
                .when(mPreloadPagesSettingsJni)
                .getState(eq(mProfile));
        GURL url = JUnitTestGURLs.RED_1;
        doReturn(url).when(mLocationBarDataProvider).getCurrentGurl();
        mMediator.setUrl(url, null);
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(456L).when(mAutocompleteCoordinator).getCurrentNativeAutocompleteResult();
        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        AutocompleteMatch defaultMatch =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("text")
                        .setInlineAutocompletion("textWithAutocomplete")
                        .setAdditionalText("additionalText")
                        .setIsSearch(false)
                        .setAllowedToBeDefaultMatch(true)
                        .build();
        mMediator.onSuggestionsChanged(defaultMatch, /* hasSuggestions= */ true);
        verify(mPrerenderJni)
                .prerenderMaybe(123L, "text", JUnitTestGURLs.RED_1.getSpec(), 456L, mProfile, mTab);
        verify(mUrlCoordinator)
                .setAutocompleteText("text", "textWithAutocomplete", "additionalText", null);

        var state = mSessionState;
        state.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        mMediator.onSuggestionsChanged(defaultMatch, /* hasSuggestions= */ true);
    }

    @Test
    public void testOnSuggestionsChanged_nullMatch() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.beginInput(new AutocompleteInput().setUserText("text"));

        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();

        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ false);
        verify(mUrlCoordinator).setAutocompleteText("text", null, null, null);
    }

    @Test
    public void testSuspendInput_enabledState_transitionsToStandby() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.ENABLED);
        mMediator.beginInput(input);

        mMediator.suspendInput();
        assertAutocompleteState(AutocompleteState.STANDBY);
    }

    @Test
    public void testOnUrlTextChanged_updatesShouldAutocomplete() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        var state = mSessionState;
        var input = state.getAutocompleteInput();

        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();
        mMediator.onUrlTextChanged("test");
        assertTrue(input.shouldAllowUserTextAutocompletion());

        doReturn(false).when(mUrlCoordinator).shouldAutocomplete();
        mMediator.onUrlTextChanged("test2");
        assertFalse(input.shouldAllowUserTextAutocompletion());
    }

    @Test
    public void testOnUrlTextChanged_preservesSelection() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        FuseboxSessionState state = mSessionState;
        AutocompleteInput input = state.getAutocompleteInput();

        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();
        doReturn(3).when(mUrlCoordinator).getSelectionStart();
        doReturn(3).when(mUrlCoordinator).getSelectionEnd();

        mMediator.onUrlTextChanged("tezst");
        assertEquals(3, input.getSelection().from);
        assertEquals(3, input.getSelection().to);
    }

    @Test
    public void testOnUrlTextChanged_resetsActivationChipFocus() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        doReturn(View.VISIBLE).when(mActivationChip).getVisibility();

        var input = mSessionState.getAutocompleteInput();
        mMediator.beginInput(input);

        LocationBarSelectionController selectionController =
                mMediator.getSelectionControllerForTesting();
        assertTrue(selectionController.selectNextItem());
        verify(mActivationChip).setSelected(true);

        clearInvocations(mActivationChip);
        mMediator.onUrlTextChanged("test");

        verify(mActivationChip, atLeastOnce()).setSelected(false);
    }

    /** Verifies that typing a space after text triggers site search. */
    @Test
    public void testOnUrlTextChangedTypedSpaceTriggersSiteSearch() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

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
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

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
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

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
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

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
                        .setOpenInNewTab(/* openInNewTab= */ false)
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
    public void testLoadUrlFromVoice_delegatesToAutocompleteCoordinator() {
        String sampleVoiceQuery = "sample voice query";
        mMediator.loadUrlFromVoice(sampleVoiceQuery);
        verify(mAutocompleteCoordinator).loadUrlFromVoice(sampleVoiceQuery);
    }

    @Test
    public void testLoadUrl_clearsFocus() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        mScrimVisibilitySupplier.set(true);
        assertTrue(mMediator.isUrlBarFocused());

        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Simulate scrim dismissal to restore focus to the tab.
        mScrimVisibilitySupplier.set(false);

        verify(mTabView).requestFocus();
        verify(mAutocompleteCoordinator, atLeastOnce()).endInput();
        verify(mUrlCoordinator).endInput();
    }

    public void testLoadUrlWithAutocompleteLoadCallback_base() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
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
                                .setOpenInNewTab(/* openInNewTab= */ false)
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
        doReturn(true)
                .when(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(any(), anyBoolean());
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());

        verify(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(mOmniboxLoadUrlParamsCaptor.capture(), anyBoolean());

        var params = mOmniboxLoadUrlParamsCaptor.getValue();
        assertEquals(TEST_URL, params.url);
        assertEquals(PageTransition.TYPED, params.transitionType);
        assertEquals(0, params.inputStartTimestamp);
        assertNull(null, params.postData);
        assertTrue(params.extraHeaders.isEmpty());
        assertFalse(params.openInNewTab);
        verify(mTab, never()).loadUrl(any());
    }

    private void testLoadUrl_openInNewTab_base() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(false).when(mTab).isIncognito();
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ true)
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
        doReturn(mActivity).when(mTab).getContext();
        doReturn(1).when(mTab).getParentId();
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewWindow(true)
                        .build());

        verify(mMultiInstanceOrchestrator)
                .openUrlInOtherWindow(
                        eq(mActivity), mLoadUrlParamsCaptor.capture(), eq(1), eq(true), eq(false));
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
        clearInvocations(mLocationBarDataProvider);
        doReturn(mView).when(mTab).getView();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        assertEquals(mView, mMediator.getViewForUrlBackFocus());
        verify(mTab).getView();

        doReturn(null).when(mLocationBarDataProvider).getTab();
        assertNull(mMediator.getViewForUrlBackFocus());
        verify(mLocationBarDataProvider, times(2)).getTab();
        verify(mTab).getView();
    }

    @Test
    public void testOnConfigurationChanged_qwertyKeyboard() {
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.ENABLED);
        Configuration config = new Configuration();
        OmniboxCapabilities.setHasDesktopExperienceForTesting(
                /* hasDesktopExperience= */ true); // Adopt Desktop functionality.

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

        OmniboxCapabilities.setHasDesktopExperienceForTesting(
                /* hasDesktopExperience= */ false); // non-Desktop functionality.
        mMediator.onConfigurationChanged(config);
        verify(mUrlCoordinator, never()).clearFocus();

        mMediator.beginInput(input);
        // Simulate focus change to make mUrlHasFocus true.
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        mMediator.onConfigurationChanged(config);
        verify(mUrlCoordinator, never()).clearFocus();

        input.setAutocompleteState(AutocompleteState.STANDBY);
        mMediator.onConfigurationChanged(config);
        verify(mUrlCoordinator).clearFocus();
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

        verify(mUrlCoordinator)
                .setUrlBarData(
                        mUrlBarDataCaptor.capture(),
                        eq(ScrollType.NO_SCROLL),
                        eq(TextSelection.SELECT_END));
        assertEquals("keyword", mUrlBarDataCaptor.getValue().displayText.toString());
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

        verify(mUrlCoordinator)
                .setUrlBarData(
                        mUrlBarDataCaptor.capture(),
                        eq(ScrollType.NO_SCROLL),
                        eq(TextSelection.SELECT_END));
        assertEquals("keyword ", mUrlBarDataCaptor.getValue().displayText.toString());
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
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        var input = mSessionState.getAutocompleteInput();
        input.setUserText("some text");
        input.setInitialUserText("initial text");
        input.setDisplayState(DisplayState.SUGGESTIONS);

        {
            // Step 1: expect suggestions to be cleared (transition to STANDBY) if user presses
            // <esc>.
            doReturn(true).when(mAutocompleteCoordinator).isServingSuggestions();
            assertTrue(mMediator.handleEscPress());
            assertEquals(DisplayState.DRAFTING, input.getDisplayState());
            verify(mAutocompleteCoordinator).stopAutocomplete();
            verify(mAutocompleteCoordinator).endInput();
            verify(mUrlCoordinator, never()).endInput();
        }

        {
            // Step 2: expect content to be reverted if suggestions are already cleared.
            assertEquals(DisplayState.DRAFTING, input.getDisplayState());
            doReturn(false).when(mAutocompleteCoordinator).isServingSuggestions();
            clearInvocations(mLocationBarLayout);
            clearInvocations(mAutocompleteCoordinator);
            assertTrue(mMediator.handleEscPress());
            verify(mLocationBarLayout).setDeleteButtonVisibility(/* shouldShow= */ false);
            assertEquals(input.getUserText(), input.getInitialUserText());
            verify(mAutocompleteCoordinator, never()).endInput();
            verify(mUrlCoordinator, never()).endInput();
        }

        {
            // Step 3: if both user text and initial user text are same, expect the input to be
            // canceled.
            clearInvocations(mAutocompleteCoordinator);
            clearInvocations(mUrlCoordinator);
            assertTrue(mMediator.handleEscPress());
            verify(mAutocompleteCoordinator, atLeastOnce()).endInput();
            verify(mUrlCoordinator).endInput();
        }

        {
            // Step 4: no other actions can be taken: bail
            assertFalse(mMediator.handleEscPress());
        }
    }

    @Test
    public void testEscapePress_stepsDownDisplayState() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setDisplayState(DisplayState.SUGGESTIONS);

        assertTrue(mMediator.handleEscPress());
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());
    }

    @Test
    public void testEscapePress_restoresFocusRingAfterNavigation() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setDisplayState(DisplayState.SUGGESTIONS);

        // Simulate navigation to suggestions.
        doReturn(true).when(mAutocompleteCoordinator).isServingSuggestions();
        doReturn(KeyEvent.KEYCODE_DPAD_DOWN).when(mKeyEvent).getKeyCode();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_DPAD_DOWN, mKeyEvent));

        // Verify focus ring is NOT shown (because URL bar is not selected).
        verify(mLocationBarLayout, atLeastOnce()).setShowFocusRing(/* showFocusRing= */ false);
        clearInvocations(mLocationBarLayout);

        // Press ESC
        assertTrue(mMediator.handleEscPress());

        // Verify focus ring is restored.
        verify(mLocationBarLayout).setShowFocusRing(/* showFocusRing= */ true);
    }

    @Test
    public void testEscapePress_noTab() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        doReturn(false).when(mLocationBarDataProvider).hasTab();
        doReturn(true).when(mOverrideBackKeyBehaviorDelegate).handleBackKeyPressed();

        var input = mSessionState.getAutocompleteInput();
        input.setUserText("some text");
        input.setInitialUserText("some text");
        input.setAutocompleteState(AutocompleteState.STANDBY);

        assertTrue(mMediator.handleEscPress());
        verify(mOverrideBackKeyBehaviorDelegate).handleBackKeyPressed();
    }

    @Test
    public void testEscapePress_resetsRequestTypeToSearch() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.ENABLED);
        input.setDisplayState(DisplayState.SUGGESTIONS);
        input.setRequestType(AutocompleteRequestType.AI_MODE);

        assertTrue(mMediator.handleEscPress());
        assertEquals(AutocompleteRequestType.SEARCH, input.getRequestType());
        assertAutocompleteState(AutocompleteState.STANDBY);
    }

    @Test
    public void testEscapePress_revertChangesResetsRequestTypeToSearch() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setInitialUserText("initial text");
        input.setUserText("modified text");
        input.setAutocompleteState(AutocompleteState.STANDBY);
        input.setRequestType(AutocompleteRequestType.AI_MODE);

        assertTrue(mMediator.handleEscPress());
        assertEquals(AutocompleteRequestType.SEARCH, input.getRequestType());
        assertEquals(input.getInitialUserText(), input.getUserText());
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());
    }

    @Test
    public void testEscapePress_restoresFocusToTabAfterScrimDismissed() {
        AutocompleteInput input = new AutocompleteInput();
        input.setDisplayState(DisplayState.SUGGESTIONS);
        doReturn(false).when(mAutocompleteCoordinator).isServingSuggestions();
        mMediator.beginInput(input);
        mScrimVisibilitySupplier.set(true);

        // 1st ESC: state -> STANDBY. Focus should NOT be restored.
        assertTrue(mMediator.handleEscPress());
        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ false);
        verify(mTabView, never()).requestFocus();

        // 2nd ESC: state -> STANDBY and text == initial -> defocus.
        assertTrue(mMediator.handleEscPress());

        // Focus should still NOT be restored yet (waiting for scrim hide).
        verify(mTabView, never()).requestFocus();

        // Simulate scrim dismissal.
        mScrimVisibilitySupplier.set(false);

        // Focus should now be restored to the tab view.
        verify(mTabView).requestFocus();
    }

    @Test
    public void testEscapePress_scrimShownAgain_cancelsFocusRestoration() {
        AutocompleteInput input = new AutocompleteInput();
        input.setDisplayState(DisplayState.SUGGESTIONS);
        doReturn(false).when(mAutocompleteCoordinator).isServingSuggestions();
        mMediator.beginInput(input);

        // 1st ESC: state -> STANDBY.
        assertTrue(mMediator.handleEscPress());
        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ false);

        // 2nd ESC: defocus.
        assertTrue(mMediator.handleEscPress());

        // Simulate scrim shown again (user re-interaction).
        mScrimVisibilitySupplier.set(true);

        // Simulate scrim dismissal (animation finally completes).
        mScrimVisibilitySupplier.set(false);

        // Focus should NOT be restored because it was canceled by the show event.
        verify(mTabView, never()).requestFocus();
    }

    @Test
    public void testEscapePress_scrimNotShown_focusesImmediately() {
        doReturn(false).when(mScrimHandler).isScrimShown();

        AutocompleteInput input = new AutocompleteInput();
        input.setAutocompleteState(AutocompleteState.STANDBY);
        doReturn(false).when(mAutocompleteCoordinator).isServingSuggestions();
        mMediator.beginInput(input);

        // Press Escape (simulates 3rd press).
        assertTrue(mMediator.handleEscPress());

        // Focus should be restored immediately because no scrim is showing.
        verify(mTabView).requestFocus();
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
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_SEARCH_PREFETCH_ON_ENTER_KEY_DOWN)
    public void testOnKey_enter_noSuggestionSelected() {
        setUpMediatorAndCoordinator();
        setUpEnterKeyEvent(9999L);

        assertTrue(mMediator.onKey(mView, KeyEvent.KEYCODE_ENTER, mKeyEvent));

        // Verify prefetch is triggered.
        verify(mAutocompleteCoordinator).prefetchDefaultMatch(eq(9999L));

        // Verify navigation is triggered.
        verify(mAutocompleteCoordinator)
                .loadTypedOmniboxText(eq(9999L), eq(NavigationTarget.CURRENT_TAB));
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_SEARCH_PREFETCH_ON_ENTER_KEY_DOWN)
    public void testOnKey_enter_noSuggestionSelected_featureDisabled() {
        setUpMediatorAndCoordinator();
        setUpEnterKeyEvent(9999L);

        assertTrue(mMediator.onKey(mView, KeyEvent.KEYCODE_ENTER, mKeyEvent));

        // Verify prefetch is NOT triggered.
        verify(mAutocompleteCoordinator, never()).prefetchDefaultMatch(anyLong());

        // Verify navigation is triggered.
        verify(mAutocompleteCoordinator)
                .loadTypedOmniboxText(eq(9999L), eq(NavigationTarget.CURRENT_TAB));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_SEARCH_PREFETCH_ON_ENTER_KEY_DOWN)
    public void testOnKey_enter_deleteButtonSelected() {
        setUpMediatorAndCoordinator();

        // Make only UrlBar and Delete button visible to simplify selection.
        doReturn(View.GONE).when(mActivationChip).getVisibility();
        doReturn(View.GONE).when(mPlusButton).getVisibility();
        doReturn(View.GONE).when(mMicButton).getVisibility();
        doReturn(View.GONE).when(mNavigateButton).getVisibility();
        doReturn(View.VISIBLE).when(mDeleteButton).getVisibility();

        // 1. Send TAB to move selection from UrlBar (index 0) to Delete button (index 1 in visible
        // views).
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(KeyEvent.KEYCODE_TAB).when(mKeyEvent).getKeyCode();
        doReturn(true).when(mKeyEvent).hasNoModifiers();

        assertTrue(mMediator.onKey(mView, KeyEvent.KEYCODE_TAB, mKeyEvent));

        // 2. Send ENTER KeyDown event to activate the selected Delete button.
        setUpEnterKeyEvent(0L);

        assertTrue(mMediator.onKey(mView, KeyEvent.KEYCODE_ENTER, mKeyEvent));

        // Verify Delete button activation was triggered (performClick).
        verify(mDeleteButton).performClick();

        // Verify prefetch was NOT triggered.
        verify(mAutocompleteCoordinator, never()).prefetchDefaultMatch(anyLong());

        // Verify UrlBar navigation was NOT triggered.
        verify(mAutocompleteCoordinator, never()).loadTypedOmniboxText(anyLong(), anyInt());
    }

    @Test
    public void testHandleTypingStarted_triggersFocusAnimation() {
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);

        // Typing started will emit suggestions changed.
        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ false);

        verify(mUrlCoordinator, times(2)).onUrlFocusChange(/* hasFocus= */ true);
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
        assertEquals(
                BrandedColorScheme.LIGHT_BRANDED_THEME,
                mOmniboxResourceProvider.getBrandedColorScheme());
    }

    @Test
    public void testUpdateColors_darkBrandedColor() {
        doReturn(Color.BLACK).when(mLocationBarDataProvider).getPrimaryColor();

        mMediator.updateBrandedColorScheme();

        verify(mLocationBarLayout).setDeleteButtonTint(any(ColorStateList.class));
        verify(mStatusCoordinator).setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        verify(mAutocompleteCoordinator)
                .updateVisualsForState(BrandedColorScheme.DARK_BRANDED_THEME);
        assertEquals(
                BrandedColorScheme.DARK_BRANDED_THEME,
                mOmniboxResourceProvider.getBrandedColorScheme());
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
        assertEquals(
                BrandedColorScheme.INCOGNITO, mOmniboxResourceProvider.getBrandedColorScheme());
    }

    @Test
    public void testUpdateColors_default() {
        mMediator.updateBrandedColorScheme();

        verify(mLocationBarLayout).setDeleteButtonTint(any(ColorStateList.class));
        verify(mStatusCoordinator).setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        verify(mAutocompleteCoordinator).updateVisualsForState(BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                BrandedColorScheme.APP_DEFAULT, mOmniboxResourceProvider.getBrandedColorScheme());
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
        assertEquals(
                BrandedColorScheme.DARK_BRANDED_THEME,
                mOmniboxResourceProvider.getBrandedColorScheme());
    }

    @Test
    public void testSetUrl() {
        mProfileSupplier.set(mProfile);
        mMediator.onFinishNativeInitialization();

        var url = JUnitTestGURLs.BLUE_1;
        UrlBarData urlBarData = UrlBarData.forUrl(url);
        mMediator.setUrl(url, urlBarData);

        // Assume that the URL bar is now focused without focus animations.
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mMediator.beginOrResumeInput(/* activateNewSession= */ true);
        mMediator.setUrl(url, urlBarData);

        // Verify that setUrl() never clears focus when the URL bar is focused without animations.
        verify(mUrlCoordinator, never()).clearFocus();

        // Verify that setUrlBarData() was invoked exactly once, after the first invocation of
        // setUrl() when the URL bar was not focused.
        verify(mUrlCoordinator)
                .setUrlBarData(urlBarData, ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_ALL);
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
    public void testEndInput_focusMovesToThief() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        mMediator.beginInput(input);
        mMediator.endInput();

        verify(mUrlCoordinator).clearFocus();
        verify(mFocusThief).requestFocus();
    }

    @Test
    public void testEndInputDeactivatesSession() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        mMediator.beginInput(input);
        assertTrue(mSessionState.isSessionActive());

        mMediator.endInput();
        assertFalse(mSessionState.isSessionActive());
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

        verify(mFuseboxCoordinator).beginInput(mFuseboxSessionStateCaptor.capture());
        verify(mStatusCoordinator).beginInput(mFuseboxSessionStateCaptor.getValue());
        verify(mUrlCoordinator).beginInput(mFuseboxSessionStateCaptor.getValue());
        assertEquals(
                OmniboxFocusReason.NTP_AI_MODE,
                mFuseboxSessionStateCaptor.getValue().getAutocompleteInput().getFocusReason());
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testBeginInput_pastedText() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        mMediator.beginInput(new AutocompleteInput().setUserText("pastedText"));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUrlCoordinator).requestFocus();

        verify(mAutocompleteCoordinator).beginInput(mFuseboxSessionStateCaptor.capture());
        verify(mUrlCoordinator).beginInput(mFuseboxSessionStateCaptor.getValue());
        assertEquals(
                "pastedText",
                mFuseboxSessionStateCaptor.getValue().getAutocompleteInput().getUserText());
    }

    @Test
    public void testBeginInput_initializationOrder() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        mMediator.beginInput(new AutocompleteInput());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUrlCoordinator).beginInput(any());
        verify(mAutocompleteCoordinator).beginInput(any());
    }

    @Test
    public void testOnUrlFocusChange() {
        testOnUrlFocusChange(/* expectDesktopMode= */ false);
    }

    @Test
    public void testOnUrlFocusChange_isNotDesktopMode() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ false);
        testOnUrlFocusChange(/* expectDesktopMode= */ false);
    }

    @Test
    public void testOnUrlFocusChange_hasDesktopExperience() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        testOnUrlFocusChange(/* expectDesktopMode= */ true);
    }

    @Test
    public void testAnimateIconChanges_bottomToolbar() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsStateProvider).getControlsPosition();
        clearInvocations(mStatusCoordinator);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        verify(mStatusCoordinator).setShouldAnimateIconChanges(false);
    }

    @Test
    public void testAnimateIconChanges_desktopPlatform() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        clearInvocations(mStatusCoordinator);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        verify(mStatusCoordinator).setShouldAnimateIconChanges(false);
    }

    private void testOnUrlFocusChange(boolean expectDesktopMode) {
        mProfileSupplier.set(mProfile);
        doReturn(JUnitTestGURLs.BLUE_1).when(mLocationBarDataProvider).getCurrentGurl();
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        assertTrue(mMediator.isUrlBarFocused());
        verify(mStatusCoordinator).setShouldAnimateIconChanges(true);
        verify(mUrlCoordinator).beginInput(any());
        verify(mUrlCoordinator).onUrlFocusChange(/* hasFocus= */ true);

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

        mMediator.onUrlFocusChange(/* hasFocus= */ true);

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
                        mOmniboxResourceProvider,
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
                        mScrimHandler,
                        mWindowHasFocusSupplier);
        mMediator.setCoordinators(mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);
        int primeCount = sGeoHeaderPrimeCount;
        mProfileSupplier.set(mProfile);
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        assertEquals(primeCount, sGeoHeaderPrimeCount);
        templateUrlServiceSupplier.set(mTemplateUrlService);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(primeCount + 1, sGeoHeaderPrimeCount);
    }

    @Test
    public void testOnUrlFocusChange_notFocusedTablet() {
        mProfileSupplier.set(mProfile);
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        mTabletMediator.addUrlFocusChangeListener(mUrlCoordinator);
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        UrlBarData urlBarData =
                UrlBarData.create(
                        /* url= */ null,
                        /* displayText= */ "text",
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 0,
                        /* editingText= */ "text");
        doReturn(urlBarData).when(mLocationBarDataProvider).getUrlBarData();
        mTabletMediator.onUrlFocusChange(/* hasFocus= */ true);
        clearInvocations(mStatusCoordinator);

        mTabletMediator.onUrlFocusChange(/* hasFocus= */ false);

        assertFalse(mTabletMediator.isUrlBarFocused());
        verify(mStatusCoordinator).setShouldAnimateIconChanges(false);
        verify(mStatusCoordinator).endInput();
        verify(mUrlCoordinator).endInput();
        verify(mUrlCoordinator).onUrlFocusChange(/* hasFocus= */ false);
        verify(mUrlCoordinator, atLeastOnce())
                .setUrlBarData(urlBarData, ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_ALL);
    }

    @Test
    public void testHandleUrlFocusAnimation_tablet() {
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doAnswer(
                        invocation -> {
                            ((Rect) invocation.getArgument(0)).set(0, 0, 10, 10);
                            return null;
                        })
                .when(mRootView)
                .getLocalVisibleRect(any());

        mTabletMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mTabletMediator.handleUrlFocusAnimation(/* hasFocus= */ true);

        verify(mUrlCoordinator).onUrlFocusChange(/* hasFocus= */ true);
        verify(mUrlAnimator).start();
        verify(mUrlAnimator).setDuration(anyLong());
        verify(mUrlAnimator).addListener(any());
    }

    @Test
    public void testHandleUrlFocusAnimation_ntp() {
        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();

        mTabletMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mTabletMediator.handleUrlFocusAnimation(/* hasFocus= */ true);

        verify(mUrlCoordinator).onUrlFocusChange(/* hasFocus= */ true);
        verify(mUrlAnimator, never()).start();
        verify(mUrlAnimator, never()).setDuration(anyLong());
        verify(mUrlAnimator, never()).addListener(any());
    }

    @Test
    public void testHandleUrlFocusAnimation_phone() {
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.handleUrlFocusAnimation(/* hasFocus= */ true);

        verify(mUrlCoordinator).onUrlFocusChange(/* hasFocus= */ true);
        verify(mUrlAnimator, never()).start();
        verify(mUrlAnimator, never()).setDuration(anyLong());
        verify(mUrlAnimator, never()).addListener(any());
    }

    @Test
    public void testSetUrlFocusChangeInProgress() {
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.setUrlFocusChangeInProgress(/* inProgress= */ true);
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        AccessibilityStateTestHelper.setAccessibilityEnabledForTesting(true);
        mMediator.beginInput(
                new AutocompleteInput()
                        .setUserText("text")
                        .setFocusReason(OmniboxFocusReason.FAKE_BOX_TAP));
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        mMediator.setUrlFocusChangeInProgress(/* inProgress= */ false);

        verify(mUrlCoordinator).onUrlAnimationFinished(true);
        verify(mUrlCoordinator).clearFocus();
        // The first invocation of requestFocus() is from setUrlBarFocus, which we use above to set
        // mUrlFocusedFromFakebox to true.
        verify(mUrlCoordinator, times(2)).requestFocus();

        verify(mAutocompleteCoordinator, atLeastOnce())
                .beginInput(mFuseboxSessionStateCaptor.capture());
        assertEquals(
                "text", mFuseboxSessionStateCaptor.getValue().getAutocompleteInput().getUserText());
    }

    @Test
    public void testSetUrlFocusChangeInProgress_accessibilityWorkaroundPreservesSession() {
        mMediator.addUrlFocusChangeListener(mUrlCoordinator);
        mMediator.setUrlFocusChangeInProgress(/* inProgress= */ true);
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        AccessibilityStateTestHelper.setAccessibilityEnabledForTesting(true);
        mMediator.beginInput(
                new AutocompleteInput()
                        .setUserText("text")
                        .setFocusReason(OmniboxFocusReason.FAKE_BOX_TAP));
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        doAnswer(
                        invocation -> {
                            mMediator.onUrlFocusChange(/* hasFocus= */ false);
                            return null;
                        })
                .when(mUrlCoordinator)
                .clearFocus();

        doAnswer(
                        invocation -> {
                            mMediator.onUrlFocusChange(/* hasFocus= */ true);
                            return null;
                        })
                .when(mUrlCoordinator)
                .requestFocus();

        clearInvocations(mAutocompleteCoordinator);

        doReturn(mOmniboxAnimator)
                .when(mAutocompleteCoordinator)
                .setupSuggestionsListShowAnimation();
        mMediator.setUrlFocusChangeInProgress(/* inProgress= */ false);

        verify(mUrlCoordinator).clearFocus();
        verify(mUrlCoordinator, times(2)).requestFocus();
        verify(mAutocompleteCoordinator, never()).endInput();
    }

    @Test
    public void testMicUpdatedAfterEventTriggered() {
        mMediator.onVoiceAvailabilityImpacted();
        verify(mLocationBarLayout, atLeast(1)).setMicButtonVisibility(false);
        verify(mLocationBarLayout, never()).setMicButtonVisibility(true);

        clearInvocations(mLocationBarLayout);
        doReturn(mDeleteButton).when(mLocationBarLayout).getDeleteButton();
        doReturn(mUrlBar).when(mLocationBarLayout).getUrlBar();
        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        mMediator.onFinishNativeInitialization();
        mMediator.onVoiceAvailabilityImpacted();

        verify(mLocationBarLayout, atLeast(1)).setMicButtonVisibility(false);
        verify(mLocationBarLayout, never()).setMicButtonVisibility(true);

        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
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
        mMediator.onUrlFocusChange(/* hasFocus= */ false);
        clearInvocations(mLocationBarLayout, mUrlCoordinator);

        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUrlCoordinator).beginInput(any());
        verify(mLocationBarLayout, atLeastOnce())
                .setDeleteButtonVisibility(/* shouldShow= */ false);
        verify(mLocationBarLayout, never()).setDeleteButtonVisibility(/* shouldShow= */ true);
    }

    @Test
    public void testOnUrlFocusChange_setNonEmptyUrl_deleteButtonVisible() {
        mMediator.onFinishNativeInitialization();
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(false);
        mMediator.onUrlFocusChange(/* hasFocus= */ false);

        /* Simulate desktop-like behaviour, where the userText is filled in. */
        mSessionState.getAutocompleteInput().setUserText("google.com");
        doReturn("google.com").when(mUrlCoordinator).getTextWithAutocomplete();
        clearInvocations(mLocationBarLayout, mUrlCoordinator);

        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mUrlCoordinator).beginInput(any());
        verify(mLocationBarLayout, atLeastOnce()).setDeleteButtonVisibility(/* shouldShow= */ true);
        verify(mLocationBarLayout, never()).setDeleteButtonVisibility(/* shouldShow= */ false);
    }

    private void verifyPhoneMicButtonVisibility() {
        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        mMediator.onFinishNativeInitialization();
        clearInvocations(mLocationBarLayout);
        doReturn(mDeleteButton).when(mLocationBarLayout).getDeleteButton();
        doReturn(mUrlBar).when(mLocationBarLayout).getUrlBar();

        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setDeleteButtonVisibility(/* shouldShow= */ false);

        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(true);
        verify(mLocationBarLayout, never()).setDeleteButtonVisibility(/* shouldShow= */ true);

        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setDeleteButtonVisibility(/* shouldShow= */ true);
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

    @Test
    public void testMicButtonVisibility_toolbarMicEnabled_tablet_aimRequest() {
        mIsToolbarMicEnabled = true;
        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        verifyMicButtonVisibilityWhenFocusChanges(true);
    }

    // Sets up and executes a test for visibility of a mic button on a tablet.
    // The mic button should not be visible if toolbar mic is visible as well.
    private void verifyMicButtonVisibilityWhenFocusChanges(boolean shouldBeVisible) {
        VoiceRecognitionHandler voiceRecognitionHandler = mock(VoiceRecognitionHandler.class);
        mTabletMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
        mTabletMediator.onFinishNativeInitialization();
        mTabletMediator.setShouldShowButtonsWhenUnfocusedForTablet(true);
        mTabletMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mTabletMediator.beginOrResumeInput(/* activateNewSession= */ true);
        mTabletMediator.onUrlFocusChange(/* hasFocus= */ true);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();
        doReturn(true).when(voiceRecognitionHandler).isVoiceSearchEnabled();
        clearInvocations(mLocationBarTablet);
        doReturn(mDeleteButton).when(mLocationBarTablet).getDeleteButton();
        doReturn(mUrlBar).when(mLocationBarTablet).getUrlBar();

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
        mTabletMediator.onUrlFocusChange(/* hasFocus= */ true);
        doReturn(inputText).when(mUrlCoordinator).getTextWithAutocomplete();
        clearInvocations(mLocationBarTablet);
        doReturn(mDeleteButton).when(mLocationBarTablet).getDeleteButton();
        doReturn(mUrlBar).when(mLocationBarTablet).getUrlBar();

        mTabletMediator.updateButtonVisibility();
        updateTabletWidthConsumers(mTabletMediator);
        ArgumentCaptor<Boolean> captor = ArgumentCaptor.forClass(Boolean.class);
        verify(mLocationBarTablet, atLeastOnce()).setLensButtonVisibility(captor.capture());
        verify(mLocationBarEmbedder, atLeastOnce()).onWidthConsumerVisibilityChanged();
        assertEquals(shouldBeVisible, captor.getValue());
    }

    @Test
    public void testButtonVisibility_showMicUnfocused() {
        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        mMediator.onFinishNativeInitialization();
        mTabletMediator.setShouldShowButtonsWhenUnfocusedForTablet(false);
        mMediator.setShouldShowMicButtonWhenUnfocusedForPhone(true);
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        mMediator.updateButtonVisibility();
        updateTabletWidthConsumers(mTabletMediator);
        verify(mLocationBarLayout, atLeastOnce()).setMicButtonVisibility(mBooleanCaptor.capture());
        verify(mLocationBarEmbedder, atLeastOnce()).onWidthConsumerVisibilityChanged();
        assertTrue(mBooleanCaptor.getValue());
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
        clearInvocations(mLocationBarTablet);
        doReturn(mDeleteButton).when(mLocationBarTablet).getDeleteButton();
        doReturn(mUrlBar).when(mLocationBarTablet).getUrlBar();

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
        clearInvocations(mLocationBarTablet);
        doReturn(mDeleteButton).when(mLocationBarTablet).getDeleteButton();
        doReturn(mUrlBar).when(mLocationBarTablet).getUrlBar();
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
        clearInvocations(mLocationBarTablet);
        doReturn(mDeleteButton).when(mLocationBarTablet).getDeleteButton();
        doReturn(mUrlBar).when(mLocationBarTablet).getUrlBar();
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
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());
        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.TYPED, /* isNtp= */ true);
        // Test searching using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.GENERATED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());
        // The time to be checked for the calling of recordNavigationOnNtp is still 1 here
        // as we verify with the argument PageTransition.GENERATED instead.
        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.GENERATED, /* isNtp= */ true);

        // Test clicking omnibox on other native page.
        // This will run the function recordNavigationOnNtp with isNtp equal to false
        // making it unable to record the histogram.
        doReturn(JUnitTestGURLs.BLUE_1).when(mTab).getUrl();
        assertFalse(UrlUtilities.isNtpUrl(mTab.getUrl()));
        // Test navigating using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());
        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.TYPED, /* isNtp= */ true);
        // Test searching using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.GENERATED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());
        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.GENERATED, /* isNtp= */ true);

        // Test clicking omnibox on html/rendered web page.
        doReturn(false).when(mTab).isNativePage();
        // Test navigating using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());
        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.TYPED, /* isNtp= */ true);
        // Test searching using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.GENERATED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());
        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.GENERATED, /* isNtp= */ true);

        // Test clicking omnibox on {@link StartSurface}.
        doReturn(true)
                .when(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(any(), anyBoolean());
        // Test navigating using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());
        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.TYPED, /* isNtp= */ true);
        // Test searching using omnibox.
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.GENERATED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());
        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.GENERATED, /* isNtp= */ true);
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
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        mMediator.setIsUrlBarFocusedWithoutAnimationsForTesting(true);
        mMediator.onTouchAfterFocus();
    }

    @Test
    public void testOnTouchAfterFocus_notHandled_focusedViaTap() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        assertTrue(mMediator.isUrlBarFocused());
        assertEquals(
                OmniboxFocusReason.OMNIBOX_TAP,
                mSessionState.getAutocompleteInput().getFocusReason());
        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.STANDBY);

        mMediator.onTouchAfterFocus();

        assertAutocompleteState(AutocompleteState.STANDBY);
    }

    @Test
    public void testOnTouchAfterFocus_notHandled_notInStandby() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(true);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);

        mMediator.showUrlBarCursorWithoutFocusAnimations();
        assertEquals(
                OmniboxFocusReason.DEFAULT_WITH_HARDWARE_KEYBOARD,
                mSessionState.getAutocompleteInput().getFocusReason());
        assertAutocompleteState(AutocompleteState.STANDBY);

        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.ENABLED);

        mMediator.onTouchAfterFocus();

        assertAutocompleteState(AutocompleteState.ENABLED);
    }

    @Test
    public void testOnTouchAfterFocus_withHardwareKeyboard_triggersSuggestions() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(true);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);

        // Start session with hardware keyboard focus (goes to STANDBY)
        mMediator.showUrlBarCursorWithoutFocusAnimations();
        assertTrue(mSessionState.isSessionActive());
        assertAutocompleteState(AutocompleteState.STANDBY);
        assertEquals(
                OmniboxFocusReason.DEFAULT_WITH_HARDWARE_KEYBOARD,
                mSessionState.getAutocompleteInput().getFocusReason());

        // Verify autocomplete hasn't started suggestions yet (we are in STANDBY)
        // Wait, beginInput was called once during showUrlBarCursorWithoutFocusAnimations.
        // It should have called mAutocompleteCoordinator.beginInput.
        verify(mAutocompleteCoordinator).beginInput(any());

        // Now touch it. This should trigger suggestions (transition to ENABLED and call
        // beginOrResumeInput)
        mMediator.onTouchAfterFocus();

        // It should have transitioned to ENABLED
        assertAutocompleteState(AutocompleteState.ENABLED);
        // Focus reason should be updated
        assertEquals(
                OmniboxFocusReason.TAP_AFTER_FOCUS_FROM_KEYBOARD,
                mSessionState.getAutocompleteInput().getFocusReason());

        // In the streamlined architecture, onTouchAfterFocus transitions state without
        // calling beginOrResumeInput again.
        verify(mAutocompleteCoordinator).beginInput(any());
    }

    @Test
    public void testOnTouchAfterFocus_withoutHardwareKeyboard_doesNotTriggerSuggestions() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        // Start a regular session (focus reason will be OMNIBOX_TAP by default, but we need it to
        // be in STANDBY to not return early on that check)
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setFocusReason(OmniboxFocusReason.OMNIBOX_TAP);
        input.setAutocompleteState(AutocompleteState.STANDBY);
        mMediator.beginInput(input);

        verify(mAutocompleteCoordinator).beginInput(any());

        // Touch it. It should return early because focus reason is not
        // DEFAULT_WITH_HARDWARE_KEYBOARD.
        mMediator.onTouchAfterFocus();

        // AutocompleteState should remain STANDBY
        assertAutocompleteState(AutocompleteState.STANDBY);

        // mAutocompleteCoordinator.beginInput should NOT have been called again.
        verify(mAutocompleteCoordinator).beginInput(any());
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
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

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
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        verify(mLocationBarLayout).onSpecializedFuseboxModeActivated(true);

        mMediator.onUrlFocusChange(/* hasFocus= */ false);
        state.getAutocompleteInput().setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        verify(mLocationBarLayout).onSpecializedFuseboxModeActivated(false);
    }

    @Test
    public void testDeleteButtonClicked() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        AutocompleteInput input = new AutocompleteInput().setUserText("test query");
        mMediator.beginInput(input);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mAutocompleteCoordinator).beginInput(mFuseboxSessionStateCaptor.capture());
        assertEquals(
                "test query",
                mFuseboxSessionStateCaptor.getValue().getAutocompleteInput().getUserText());
        clearInvocations(mAutocompleteCoordinator, mUrlCoordinator);

        mMediator.deleteButtonClicked(null);
        assertEquals(
                "", mFuseboxSessionStateCaptor.getValue().getAutocompleteInput().getUserText());
        verify(mUrlCoordinator).setUrlBarData(mUrlBarDataCaptor.capture(), anyInt(), any());
        assertTrue(mUrlBarDataCaptor.getValue().displayText.isEmpty());
        verify(mUrlCoordinator).requestAccessibilityFocus();
    }

    @Test
    public void testInstallButtonClicked() {
        mMediator.installButtonClicked(null);
        verify(mAddToHomescreenCoordinator).showForAppMenu(AppMenuVerbiage.APP_MENU_OPTION_INSTALL);
    }

    @Test
    public void testRestoringText() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
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

        verify(mAutocompleteCoordinator, atLeastOnce())
                .beginInput(mFuseboxSessionStateCaptor.capture());
        assertEquals(
                newText,
                mFuseboxSessionStateCaptor.getValue().getAutocompleteInput().getUserText());
    }

    @Test
    public void testRestoringTextAndEditingStateOnTablet() {
        // Recreate mediator to respect the overridden feature flag and params.
        mTabletMediator = createTabletMediator();
        mTabletMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        RobolectricUtil.runAllBackgroundAndUi();

        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(JUnitTestGURLs.NTP_URL).when(mLocationBarDataProvider).getCurrentGurl();

        // Prepare a session state to be restored.
        String newText = "new text";
        doReturn(newText).when(mUrlCoordinator).getTextWithoutAutocomplete();
        final int newSelectionStart = 2;
        final int newSelectionEnd = 6;
        var newState = mSessionState;
        newState.getAutocompleteInput().setUserText(newText);
        newState.getAutocompleteInput()
                .setSelection(new TextSelection(newSelectionStart, newSelectionEnd));
        newState.activate(mContext, null, mProfileSupplier, null);

        FuseboxSessionState previousState = new FuseboxSessionState();
        doReturn(previousState).when(mLocationBarDataProvider).getFuseboxSessionState();

        // Emulate a state where the omnibox is focused and user has typed a text.
        mTabletMediator.onUrlFocusChange(/* hasFocus= */ true);
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

        verify(mAutocompleteCoordinator, atLeastOnce())
                .beginInput(mFuseboxSessionStateCaptor.capture());
        assertEquals(
                newText,
                mFuseboxSessionStateCaptor.getValue().getAutocompleteInput().getUserText());

        // The state for previousTab was saved.
        assertTrue(previousState.isSessionActive());
        assertEquals(previousText, previousState.getAutocompleteInput().getUserText());
        assertEquals(
                previousSelectionStart, previousState.getAutocompleteInput().getSelection().from);
        assertEquals(previousSelectionEnd, previousState.getAutocompleteInput().getSelection().to);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNavigateButton_showsWhenExpandedAndFocusedWithText() {
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();

        mMediator.updateButtonVisibility();
        assertTrue(mNavigateButtonIsVisible);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNavigateButton_hidesWhenExpandedAndFocusedWithoutText() {
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();
        mHasAttachmentsSupplier.set(false);

        mMediator.updateButtonVisibility();
        assertFalse(mNavigateButtonIsVisible);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNavigateButton_showsWhenExpandedAndFocusedWithoutTextButWithAttachments() {
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();
        mHasAttachmentsSupplier.set(true);

        mMediator.updateButtonVisibility();
        assertTrue(mNavigateButtonIsVisible);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNavigateButton_hidesWhenCompact() {
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();

        mMediator.updateButtonVisibility();
        assertFalse(mNavigateButtonIsVisible);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNavigateButton_hidesWhenNotFocused() {
        mMediator.onUrlFocusChange(/* hasFocus= */ false);
        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();

        mMediator.updateButtonVisibility();
        assertFalse(mNavigateButtonIsVisible);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testNavigateButton_visibilityUpdatesOnFuseboxStateChange() {
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
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
        mMediator.onUrlFocusChange(/* hasFocus= */ false);
        mMediator.setUrlFocusChangeInProgress(/* inProgress= */ false);

        clearInvocations(mLocationBarLayout, mLocationBarEmbedder);

        mMediator.onInstallabilityUpdated(mAppBannerManager);
        verify(mLocationBarLayout).setInstallButtonVisibility(true);
        verify(mLocationBarEmbedder).onWidthConsumerVisibilityChanged();
    }

    @Test
    public void testInstallButton_invisibleIfNotInstallable() {
        doReturn(false).when(mAppBannerManagerJni).isProbablyPromotable(mWebContents);
        clearInvocations(mLocationBarLayout, mLocationBarEmbedder);

        mMediator.onInstallabilityUpdated(mAppBannerManager);
        verify(mLocationBarLayout).setInstallButtonVisibility(false);
        verify(mLocationBarEmbedder).onWidthConsumerVisibilityChanged();
    }

    @Test
    public void testInstallButton_invisibleOmniboxIsFocused() {
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        mMediator.setUrlFocusChangeInProgress(/* inProgress= */ false);

        clearInvocations(mLocationBarLayout, mLocationBarEmbedder);

        mMediator.onInstallabilityUpdated(mAppBannerManager);
        verify(mLocationBarLayout).setInstallButtonVisibility(false);
        verify(mLocationBarEmbedder).onWidthConsumerVisibilityChanged();
    }

    @Test
    public void testInstallButton_visibilityRespondsToAppInstallationStateChange() {
        doReturn(true).when(mAppBannerManagerJni).isProbablyPromotable(mWebContents);
        mMediator.onUrlFocusChange(/* hasFocus= */ false);
        mMediator.setUrlFocusChangeInProgress(/* inProgress= */ false);

        clearInvocations(mLocationBarLayout, mLocationBarEmbedder);

        // 1. App is not installed yet -> Install button should show
        doReturn(AppInstallState.NOT_INSTALLED).when(mLocationBarDataProvider).getAppInstallState();
        mMediator.onAppInstallationStateChanged();
        verify(mLocationBarLayout).setInstallButtonVisibility(true);

        clearInvocations(mLocationBarLayout, mLocationBarEmbedder);

        // 2. App becomes installed -> Install button should hide
        doReturn(AppInstallState.INSTALLED).when(mLocationBarDataProvider).getAppInstallState();
        mMediator.onAppInstallationStateChanged();
        verify(mLocationBarLayout).setInstallButtonVisibility(false);
    }

    @Test
    public void testInstallButton_visibilityRespondsToPendingAppInstallation() {
        doReturn(true).when(mAppBannerManagerJni).isProbablyPromotable(mWebContents);
        mMediator.onUrlFocusChange(false);
        mMediator.setUrlFocusChangeInProgress(false);

        clearInvocations(mLocationBarLayout, mLocationBarEmbedder);

        doReturn(AppInstallState.NOT_INSTALLED).when(mLocationBarDataProvider).getAppInstallState();
        mMediator.onAppInstallationStateChanged();
        verify(mLocationBarLayout).setInstallButtonVisibility(true);

        clearInvocations(mLocationBarLayout, mLocationBarEmbedder);

        doReturn(AppInstallState.PENDING_INSTALL)
                .when(mLocationBarDataProvider)
                .getAppInstallState();
        mMediator.onAppInstallationStateChanged();
        verify(mLocationBarLayout).setInstallButtonVisibility(false);

        clearInvocations(mLocationBarLayout, mLocationBarEmbedder);

        doReturn(AppInstallState.NOT_INSTALLED).when(mLocationBarDataProvider).getAppInstallState();
        mMediator.onAppInstallationStateChanged();
        verify(mLocationBarLayout).setInstallButtonVisibility(true);
    }

    @Test
    public void testInstallButton_restoredWhenPendingInstallationFails() {
        doReturn(true).when(mAppBannerManagerJni).isProbablyPromotable(mWebContents);
        mMediator.onUrlFocusChange(false);
        mMediator.setUrlFocusChangeInProgress(false);

        clearInvocations(mLocationBarLayout, mLocationBarEmbedder);

        // 1. Pending installation in progress -> install button is suppressed.
        doReturn(AppInstallState.PENDING_INSTALL)
                .when(mLocationBarDataProvider)
                .getAppInstallState();
        mMediator.onAppInstallationStateChanged();
        verify(mLocationBarLayout).setInstallButtonVisibility(false);

        clearInvocations(mLocationBarLayout, mLocationBarEmbedder);

        // 2. Pending installation fails / is cancelled -> install button is restored.
        doReturn(AppInstallState.NOT_INSTALLED).when(mLocationBarDataProvider).getAppInstallState();
        mMediator.onAppInstallationStateChanged();
        verify(mLocationBarLayout).setInstallButtonVisibility(true);
    }

    @Test
    public void testZoomButtonClicked() {
        mMediator.onFinishNativeInitialization();
        mMediator.zoomButtonClicked(null);
        verify(mPageZoomIndicatorCoordinator).show();
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
    public void testShouldShowZoomButton_defaultZoom_popupShowing() {
        mTabletMediator.onFinishNativeInitialization();
        when(mPageZoomIndicatorCoordinator.isZoomLevelDefault()).thenReturn(true);
        when(mPageZoomIndicatorCoordinator.isPopupWindowShowing()).thenReturn(true);
        assertTrue(mTabletMediator.shouldShowZoomButton());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testMicButtonToolbarWidthConsumer() {
        int buttonWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        assertFalse(mTabletMediator.shouldShowMicButton());

        mTabletMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        mTabletMediator.onFinishNativeInitialization();
        mTabletMediator.setShouldShowButtonsWhenUnfocusedForTablet(true);
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        mTabletMediator.onUrlFocusChange(/* hasFocus= */ true);

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

        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

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

        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);

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

        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

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

        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ false);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_suggestionsPopover() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        mProfileSupplier.set(mProfile);
        mMediator.onFinishNativeInitialization();
        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ true);

        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        clearInvocations(mLocationBarLayout);
        mMediator.updateButtonVisibility();
        verify(mLocationBarLayout).setMicButtonVisibility(/* shouldShow= */ false);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_suggestionsPopover_withQuery() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        mProfileSupplier.set(mProfile);
        mTabletMediator.onFinishNativeInitialization();
        mTabletMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mTabletMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarTablet);
        updateTabletWidthConsumers(mTabletMediator);
        verify(mLocationBarTablet).setMicButtonVisibility(/* shouldShow= */ true);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_suggestionsPopover_toolbarMicEnabled() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        mProfileSupplier.set(mProfile);
        mTabletMediator.onFinishNativeInitialization();
        mTabletMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        mIsToolbarMicEnabled = true;

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        doReturn("text").when(mUrlCoordinator).getTextWithAutocomplete();
        mTabletMediator.onUrlFocusChange(/* hasFocus= */ true);

        clearInvocations(mLocationBarTablet);
        updateTabletWidthConsumers(mTabletMediator);
        verify(mLocationBarTablet).setMicButtonVisibility(/* shouldShow= */ true);
    }

    @Test
    public void testFuseboxLayoutMode_PropagatedOnInitialization() {
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        verify(mLocationBarLayout).setFuseboxLayoutMode(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateButtonVisibility_ImageGenMode_noQuery_showMic() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

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

        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);

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

        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

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

        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);

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

        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

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

        mMediator.setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);

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
        mTabletMediator.onUrlFocusChange(/* hasFocus= */ true);

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
    public void testBookmarkButton_ntp() {
        mTabletMediator.onFinishNativeInitialization();
        doReturn(JUnitTestGURLs.NTP_URL).when(mLocationBarDataProvider).getCurrentGurl();
        assertFalse(mTabletMediator.shouldShowBookmarkButton());
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
    public void testInstallButtonSuppressedWhenAppInstalled() {
        mTabletMediator.onFinishNativeInitialization();

        // 1. If no app is installed, shouldShowInstallButton() can be true
        doReturn(true).when(mAppBannerManagerJni).isProbablyPromotable(mWebContents);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mTabletMediator.shouldShowInstallButton());

        // 2. Mock the data provider to return installed app
        doReturn(AppInstallState.INSTALLED).when(mLocationBarDataProvider).getAppInstallState();
        mTabletMediator.onUrlChanged(/* isTabChanging= */ false);

        // 3. Verify shouldShowInstallButton() is now false
        assertFalse(mTabletMediator.shouldShowInstallButton());
    }

    @Test
    public void testInstallButtonSuppressedWhenPendingAppInstall() {
        mTabletMediator.onFinishNativeInitialization();

        doReturn(true).when(mAppBannerManagerJni).isProbablyPromotable(mWebContents);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mTabletMediator.shouldShowInstallButton());

        doReturn(AppInstallState.PENDING_INSTALL)
                .when(mLocationBarDataProvider)
                .getAppInstallState();
        mTabletMediator.onUrlChanged(/* isTabChanging= */ false);

        assertFalse(mTabletMediator.shouldShowInstallButton());
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
    public void testOnSearchEngineName_UpdatesHintText() {
        mMediator.onFinishNativeInitialization();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mSearchEngineService).addSearchEngineNameObserver(mObserverCaptor.capture());
        SearchEngineNameObserver observer = mObserverCaptor.getValue();

        // Case 1: Google
        verify(mUrlCoordinator).setUrlBarHintText(eq("Search Google or type URL"));

        // Case 2: Yahoo (Non-Google)
        clearInvocations(mUrlCoordinator);
        doReturn("Yahoo").when(mSearchEngineService).getSearchEngineName();
        doReturn("Search Yahoo or type URL").when(mSearchEngineService).getOmniboxHintString();
        observer.onSearchEngineNameChanged();
        verify(mUrlCoordinator).setUrlBarHintText(eq("Search Yahoo or type URL"));
    }

    @Test
    public void testOnSearchEngineName_EmbedderControlledHint_DoesNotUpdateHintText() {
        mUiOverrides.setEmbedderControlledHint(true);

        mMediator.onFinishNativeInitialization();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mSearchEngineService).addSearchEngineNameObserver(mObserverCaptor.capture());
        SearchEngineNameObserver observer = mObserverCaptor.getValue();

        clearInvocations(mUrlCoordinator);
        observer.onSearchEngineNameChanged();

        verify(mUrlCoordinator, never()).setUrlBarHintText(any());
    }

    @Test
    public void testOnSearchBoxHintTextChanged_SiteSearchActive_HidesHintText() {
        mMediator.onFinishNativeInitialization();
        RobolectricUtil.runAllBackgroundAndUi();

        // Begin input to set mCurrentInput.
        var input = mSessionState.getAutocompleteInput();
        mMediator.beginInput(input);
        RobolectricUtil.runAllBackgroundAndUi();

        // Set site search data.
        input.setSiteSearchData(new SiteSearchData("keyword", "Search keyword"));

        // Verify hint text is set to empty.
        verify(mUrlCoordinator).setUrlBarHintText(eq(""));
    }

    @Test
    public void testEndInputResetsHint() {
        mProfileSupplier.set(mProfile);
        mMediator.onFinishNativeInitialization();
        RobolectricUtil.runAllBackgroundAndUi();

        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        clearInvocations(mUrlCoordinator);

        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        verify(mUrlCoordinator).setUrlBarHintText(eq("Ask anything"));

        clearInvocations(mUrlCoordinator);
        mMediator.onUrlFocusChange(/* hasFocus= */ false);
        verify(mUrlCoordinator).setUrlBarHintText(eq("Search Google or type URL"));
    }

    @Test
    public void testLoadUrl_chromeExtensionScheme() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        ExtensionUi.setBackendForTesting(mExtensionUiBackend);

        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        String url = UrlConstants.CHROME_EXTENSION_SCHEME + "://id/?q=test";
        mMediator.loadUrl(
                new OmniboxLoadUrlParams.Builder(url, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ true)
                        .build());

        verify(mExtensionUiBackend).onOmniboxExtensionInputEntered(mWebContents, url, true, false);
        verify(mTab, never()).loadUrl(any());
        verify(mTabModelSelector, never()).openNewTab(any(), anyInt(), any(), anyBoolean());
        verify(mMultiInstanceOrchestrator, never())
                .openUrlInOtherWindow(any(), any(), anyInt(), anyBoolean(), anyBoolean());

        ExtensionUi.setBackendForTesting(null);
    }

    @Test
    public void testOnBackButtonClicked() {
        UserActionTester actionTester = new UserActionTester();
        doReturn(true).when(mTab).canGoBack();
        mMediator.onBackButtonClicked();
        verify(mTab).goBack();
        assertEquals(1, actionTester.getActionCount("MobileOmnibox.Back"));
    }

    @Test
    public void testBackButtonClicked_cannotGoBack() {
        UserActionTester actionTester = new UserActionTester();
        doReturn(false).when(mTab).canGoBack();
        mMediator.onBackButtonClicked();
        verify(mTab, never()).goBack();
        assertEquals(1, actionTester.getActionCount("MobileOmnibox.Back"));
    }

    @Test
    public void testBackButtonClicked_nullTab() {
        UserActionTester actionTester = new UserActionTester();
        doReturn(null).when(mLocationBarDataProvider).getTab();
        doReturn(true).when(mOverrideBackKeyBehaviorDelegate).handleBackKeyPressed();
        mMediator.onBackButtonClicked();
        verify(mOverrideBackKeyBehaviorDelegate).handleBackKeyPressed();
        assertEquals(1, actionTester.getActionCount("MobileOmnibox.Back"));
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
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
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
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testUpdateBackButtonVisibility_hiddenInMiniOriginMode() {
        mMediator.setMiniOriginMode(true);
        clearInvocations(mLocationBarLayout);
        mMediator.updateBackButtonVisibility();
        verify(mLocationBarLayout).setBackButtonVisibility(false);

        mMediator.setMiniOriginMode(false);
        verify(mLocationBarLayout).setBackButtonVisibility(true);
    }

    @Test
    public void testOnSuggestionsChanged_triggersScrimVisibility() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mMediator.beginInput(new AutocompleteInput());
        verify(mScrimHandler).setVisibility(true);
        clearInvocations(mScrimHandler);

        // Show scrim in all contexts if there are any suggestions to show.
        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ true);
        verify(mScrimHandler).setVisibility(true);
        clearInvocations(mScrimHandler);

        // Show scrim on mobile devices even if there are no suggestions to show.
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ false);
        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ false);
        verify(mScrimHandler).setVisibility(true);
        clearInvocations(mScrimHandler);

        // On desktop, we show no suggestions in select cases, e.g. on the NTP where the omnibox is
        // prefocused. We don't want to show the scrim in that scenario either.
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        mMediator.suspendInput();
        mMediator.beginInput(
                new AutocompleteInput().setAutocompleteState(AutocompleteState.STANDBY));
        verify(mScrimHandler).setVisibility(false);
        clearInvocations(mScrimHandler);
        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ false);
        verify(mScrimHandler).setVisibility(false);
        clearInvocations(mScrimHandler);
    }

    @Test
    public void testOnSuggestionsChanged_aimRequest_keepsSuggestionsDisplayState() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.AI_MODE);
        mMediator.beginInput(input);

        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ true);
        assertEquals(DisplayState.SUGGESTIONS, input.getDisplayState());

        // When 0 suggestions arrive (hasSuggestions = false), AI mode should stay in SUGGESTIONS
        // mode.
        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ false);
        assertEquals(DisplayState.SUGGESTIONS, input.getDisplayState());
    }

    @Test
    public void testDisplayStateTransitions_conventionalSearch() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        assertEquals(DisplayState.WEBSITE, input.getDisplayState());

        mMediator.beginInput(input);
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());

        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ true);
        assertEquals(DisplayState.SUGGESTIONS, input.getDisplayState());

        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ false);
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());

        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ true);
        assertEquals(DisplayState.SUGGESTIONS, input.getDisplayState());

        mMediator.endInput();
        assertEquals(DisplayState.WEBSITE, input.getDisplayState());

        mMediator.beginInput(input);
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());
    }

    @Test
    public void testDisplayStateTransitions_suspendInput() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        AutocompleteInput input = mSessionState.getAutocompleteInput();

        mMediator.beginInput(input);
        mMediator.onSuggestionsChanged(null, /* hasSuggestions= */ true);
        assertEquals(DisplayState.SUGGESTIONS, input.getDisplayState());

        mMediator.suspendInput();
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());
    }

    @Test
    public void testOnFuseboxStateChanged_expanded_setsSuggestionsDisplayState() {
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        mMediator.beginInput(input);
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());

        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        assertEquals(DisplayState.SUGGESTIONS, input.getDisplayState());
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
    public void testReparenting_notEnabled() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        assertFalse(mMediator.isParentedToSuggestionsContainer());
        mMediator.handleUrlFocusAnimation(/* hasFocus= */ true);
        assertFalse(mMediator.isParentedToSuggestionsContainer());
        mMediator.handleUrlFocusAnimation(/* hasFocus= */ false);
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
        doReturn(mPlaceholder)
                .when(mLocationBarLayout)
                .findViewById(R.id.suggestions_container_placeholder);
        int placeholderIndex = 2;
        doReturn(placeholderIndex).when(mLocationBarLayout).indexOfChild(mPlaceholder);

        mSessionState.getAutocompleteInput().setDisplayState(DisplayState.SUGGESTIONS);
        mMediator.handleUrlFocusAnimation(/* hasFocus= */ true);
        assertTrue(mMediator.isParentedToSuggestionsContainer());
        assertEquals(MarginLayoutParams.MATCH_PARENT, layoutParams.width);
        assertEquals(MarginLayoutParams.MATCH_PARENT, layoutParams.height);
        verify(mSuggestionsContainer).addView(mLocationBarLayout, 0, layoutParams);
        verify(mLocationBarLayout).addView(mDropdown, placeholderIndex);
        verify(mUrlCoordinator).startReparenting();
        verify(mUrlCoordinator).finishReparenting(true);

        clearInvocations(mUrlCoordinator);
        mMediator.endInput();
        assertFalse(mMediator.isParentedToSuggestionsContainer());
        verify(mSuggestionsContainer).removeView(mLocationBarLayout);
        verify(mLocationBarParent).addView(mLocationBarLayout, 0, layoutParams);
        assertEquals(MarginLayoutParams.MATCH_PARENT, layoutParams.width);
        assertEquals(MarginLayoutParams.MATCH_PARENT, layoutParams.height);
        verify(mLocationBarLayout).removeView(mDropdown);
        verify(mUrlCoordinator).startReparenting();
        verify(mUrlCoordinator).finishReparenting(false);
    }

    @Test
    public void testReparentToToolbar_preservesFocusInStandby() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);

        doReturn(mLocationBarParent).when(mLocationBarLayout).getParent();
        doReturn(mSuggestionsContainer).when(mAutocompleteCoordinator).getSuggestionsContainer();
        doReturn(mDropdown).when(mSuggestionsContainer).takeDropdownView();
        MarginLayoutParams layoutParams = new MarginLayoutParams(-2, -2);
        doReturn(layoutParams).when(mLocationBarLayout).getLayoutParams();
        doReturn(mPlaceholder)
                .when(mLocationBarLayout)
                .findViewById(R.id.suggestions_container_placeholder);
        int placeholderIndex = 2;
        doReturn(placeholderIndex).when(mLocationBarLayout).indexOfChild(mPlaceholder);

        mSessionState.getAutocompleteInput().setDisplayState(DisplayState.SUGGESTIONS);
        mSessionState.activate(mContext, mWebContents, mProfileSupplier, null);
        mMediator.beginInput(mSessionState.getAutocompleteInput());
        assertTrue(mMediator.isParentedToSuggestionsContainer());
        verify(mUrlCoordinator).finishReparenting(true);

        clearInvocations(mUrlCoordinator);
        mSessionState.getAutocompleteInput().setDisplayState(DisplayState.DRAFTING);
        assertFalse(mMediator.isParentedToSuggestionsContainer());
        verify(mUrlCoordinator).finishReparenting(true);
    }

    private AutocompleteInput setupDesktopSuggestionsSession() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);

        doReturn(mLocationBarParent).when(mLocationBarLayout).getParent();
        doReturn(mSuggestionsContainer).when(mAutocompleteCoordinator).getSuggestionsContainer();
        doReturn(mDropdown).when(mSuggestionsContainer).takeDropdownView();
        MarginLayoutParams layoutParams = new MarginLayoutParams(-2, -2);
        doReturn(layoutParams).when(mLocationBarLayout).getLayoutParams();
        doReturn(mPlaceholder)
                .when(mLocationBarLayout)
                .findViewById(R.id.suggestions_container_placeholder);
        int placeholderIndex = 2;
        doReturn(placeholderIndex).when(mLocationBarLayout).indexOfChild(mPlaceholder);

        mSessionState.getAutocompleteInput().setDisplayState(DisplayState.SUGGESTIONS);
        mMediator.handleUrlFocusAnimation(/* hasFocus= */ true);
        assertTrue(mMediator.isParentedToSuggestionsContainer());

        clearInvocations(mLocationBarLayout);
        doReturn(mDeleteButton).when(mLocationBarLayout).getDeleteButton();
        doReturn(mUrlBar).when(mLocationBarLayout).getUrlBar();

        return mSessionState.getAutocompleteInput();
    }

    @Test
    public void testDesktopDeleteButton() {
        AutocompleteInput input = setupDesktopSuggestionsSession();
        input.setUserText("modified text").setInitialUserText("initial text");
        input.setRequestType(AutocompleteRequestType.AI_MODE);

        mMediator.deleteButtonClicked(null);
        assertEquals("", input.getUserText());
        assertEquals(AutocompleteRequestType.AI_MODE, input.getRequestType());
        verify(mLocationBarLayout, never()).setDeleteButtonVisibility(/* shouldShow= */ false);

        mMediator.deleteButtonClicked(null);
        assertEquals("initial text", input.getUserText());
        assertEquals(TextSelection.SELECT_END, input.getSelection());
        assertEquals(AutocompleteState.STANDBY, input.getAutocompleteState());
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());
        assertEquals(AutocompleteRequestType.SEARCH, input.getRequestType());
        verify(mLocationBarLayout, atLeastOnce())
                .setDeleteButtonVisibility(/* shouldShow= */ false);
    }

    @Test
    public void testDesktopDeleteButton_withCustomTool_revertsToAiMode() {
        AutocompleteInput input = setupDesktopSuggestionsSession();
        input.setUserText("").setInitialUserText("initial text");
        input.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);

        mMediator.deleteButtonClicked(null);
        assertEquals("", input.getUserText());
        assertEquals(AutocompleteRequestType.AI_MODE, input.getRequestType());

        mMediator.deleteButtonClicked(null);
        assertEquals("initial text", input.getUserText());
        assertEquals(TextSelection.SELECT_END, input.getSelection());
        assertEquals(AutocompleteState.STANDBY, input.getAutocompleteState());
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());
        assertEquals(AutocompleteRequestType.SEARCH, input.getRequestType());
    }

    @Test
    public void testDesktopDeleteButton_withAttachments_clearsAttachments() {
        AutocompleteInput input = setupDesktopSuggestionsSession();
        doReturn(false).doReturn(true).when(mFuseboxAttachmentModelList).isEmpty();
        mMediator.setAttachmentModelList(mFuseboxAttachmentModelList);

        input.setUserText("").setInitialUserText("initial text");
        input.setRequestType(AutocompleteRequestType.AI_MODE);

        mMediator.deleteButtonClicked(null);
        verify(mFuseboxAttachmentModelList).clear();
        assertEquals(AutocompleteRequestType.AI_MODE, input.getRequestType());

        mMediator.deleteButtonClicked(null);
        assertEquals("initial text", input.getUserText());
        assertEquals(TextSelection.SELECT_END, input.getSelection());
        assertEquals(AutocompleteState.STANDBY, input.getAutocompleteState());
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());
        assertEquals(AutocompleteRequestType.SEARCH, input.getRequestType());
    }

    @Test
    public void testDesktopDeleteButton_withTextToolAndAttachments() {
        AutocompleteInput input = setupDesktopSuggestionsSession();
        doReturn(false).doReturn(true).when(mFuseboxAttachmentModelList).isEmpty();
        mMediator.setAttachmentModelList(mFuseboxAttachmentModelList);

        input.setUserText("deep search query").setInitialUserText("initial text");
        input.setRequestType(AutocompleteRequestType.DEEP_SEARCH);

        mMediator.deleteButtonClicked(null);
        assertEquals("", input.getUserText());
        assertEquals(AutocompleteRequestType.AI_MODE, input.getRequestType());
        verify(mFuseboxAttachmentModelList).clear();

        mMediator.deleteButtonClicked(null);
        assertEquals("initial text", input.getUserText());
        assertEquals(TextSelection.SELECT_END, input.getSelection());
        assertEquals(AutocompleteState.STANDBY, input.getAutocompleteState());
        assertEquals(DisplayState.DRAFTING, input.getDisplayState());
        assertEquals(AutocompleteRequestType.SEARCH, input.getRequestType());
    }

    @Test
    public void testOnAttachmentListChanged_withAttachments_promotesDisplayStateToSuggestions() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        setupSession(DisplayState.DRAFTING, /* textDiffers= */ false);

        doReturn(false).when(mFuseboxAttachmentModelList).isEmpty();
        mMediator.setAttachmentModelList(mFuseboxAttachmentModelList);

        mMediator.onAttachmentListChanged();
        assertDisplayState(DisplayState.SUGGESTIONS);
    }

    @Test
    public void testOnAttachmentListChanged_emptyAttachments_doesNotPromoteDisplayState() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        setupSession(DisplayState.DRAFTING, /* textDiffers= */ false);

        doReturn(true).when(mFuseboxAttachmentModelList).isEmpty();
        mMediator.setAttachmentModelList(mFuseboxAttachmentModelList);

        mMediator.onAttachmentListChanged();
        assertDisplayState(DisplayState.DRAFTING);
    }

    @Test
    public void testDeleteButton_mobile_doesNotRevertCustomToolToAiMode() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setUserText("generate an image");
        input.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);

        mMediator.deleteButtonClicked(null);
        assertEquals("", input.getUserText());
        assertEquals(AutocompleteRequestType.IMAGE_GENERATION, input.getRequestType());

        mMediator.deleteButtonClicked(null);
        assertEquals(AutocompleteRequestType.IMAGE_GENERATION, input.getRequestType());
    }

    @Test
    public void testDeleteButtonVisibility_hasDesktopExperience() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        mMediator.onFinishNativeInitialization();
        doReturn("google.com").when(mUrlCoordinator).getTextWithAutocomplete();

        mMediator.beginInput(new AutocompleteInput().setUserText("google.com"));
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLocationBarLayout, never()).setDeleteButtonVisibility(/* shouldShow= */ true);
    }

    @Test
    public void testDeleteButtonVisibility_hasDesktopExperience_aiMode_draftingNoPopover() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        mMediator.onFinishNativeInitialization();
        doReturn("").when(mUrlCoordinator).getTextWithAutocomplete();

        mMediator.beginInput(
                new AutocompleteInput()
                        .setUserText("")
                        .setRequestType(AutocompleteRequestType.AI_MODE));
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLocationBarLayout, never()).setDeleteButtonVisibility(/* shouldShow= */ true);
    }

    @Test
    public void testDeleteButtonVisibility_hasDesktopExperience_aiMode_reparenting() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        doReturn(mLocationBarParent).when(mLocationBarLayout).getParent();
        doReturn(mSuggestionsContainer).when(mAutocompleteCoordinator).getSuggestionsContainer();
        doReturn(mDropdown).when(mSuggestionsContainer).takeDropdownView();
        MarginLayoutParams layoutParams = new MarginLayoutParams(-2, -2);
        doReturn(layoutParams).when(mLocationBarLayout).getLayoutParams();
        doReturn(mPlaceholder)
                .when(mLocationBarLayout)
                .findViewById(R.id.suggestions_container_placeholder);
        doReturn(2).when(mLocationBarLayout).indexOfChild(mPlaceholder);

        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        input.setDisplayState(DisplayState.DRAFTING);
        mMediator.beginInput(input);
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // While in SEARCH and DRAFTING mode, not reparented to suggestions container.
        assertFalse(mMediator.isParentedToSuggestionsContainer());
        verify(mLocationBarLayout, never()).setDeleteButtonVisibility(/* shouldShow= */ true);

        // Transition to AI_MODE and SUGGESTIONS mode triggers reparenting to suggestions container.
        input.setRequestType(AutocompleteRequestType.AI_MODE);
        input.setDisplayState(DisplayState.SUGGESTIONS);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertTrue(mMediator.isParentedToSuggestionsContainer());
        verify(mLocationBarLayout, atLeastOnce()).setDeleteButtonVisibility(/* shouldShow= */ true);

        // Transition back to DRAFTING mode reparents back to toolbar and hides delete button.
        input.setDisplayState(DisplayState.DRAFTING);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertFalse(mMediator.isParentedToSuggestionsContainer());
        verify(mLocationBarLayout, atLeastOnce())
                .setDeleteButtonVisibility(/* shouldShow= */ false);
    }

    @Test
    public void testIsKeyboardSuppressed() {
        SettableNonNullObservableSupplier<Integer> popupStateSupplier =
                ObservableSuppliers.createNonNull(PopupState.HIDDEN);
        doReturn(popupStateSupplier).when(mFuseboxCoordinator).getPopupStateSupplier();

        // 1. Popup state is HIDDEN -> not suppressed
        popupStateSupplier.set(PopupState.HIDDEN);
        assertFalse(mMediator.isKeyboardSuppressed());

        // 2. Popup state is FLOATING -> not suppressed
        popupStateSupplier.set(PopupState.FLOATING);
        assertFalse(mMediator.isKeyboardSuppressed());

        // 3. Popup state is BOTTOM -> suppressed
        popupStateSupplier.set(PopupState.BOTTOM);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM)
    public void testUrlBarAccessibilityWarning_notSecure_flagOn() {
        mMediator.onFinishNativeInitialization();
        doReturn(ConnectionSecurityLevel.WARNING).when(mLocationBarDataProvider).getSecurityLevel();
        clearInvocations(mUrlCoordinator);

        mMediator.onSecurityStateChanged();

        verify(mUrlCoordinator)
                .setAccessibilityWarning(
                        eq(mContext.getString(R.string.page_info_not_secure_description)));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM)
    public void testUrlBarAccessibilityWarning_secure_flagOn() {
        mMediator.onFinishNativeInitialization();
        doReturn(ConnectionSecurityLevel.SECURE).when(mLocationBarDataProvider).getSecurityLevel();
        clearInvocations(mUrlCoordinator);

        mMediator.onSecurityStateChanged();

        verify(mUrlCoordinator).setAccessibilityWarning(eq(null));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM)
    public void testUrlBarAccessibilityWarning_notSecure_flagOff() {
        mMediator.onFinishNativeInitialization();
        clearInvocations(mUrlCoordinator);

        mMediator.onSecurityStateChanged();

        verify(mUrlCoordinator).setAccessibilityWarning(eq(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM)
    public void testUrlBarAccessibilityWarning_notSecure_flagOn_focused() {
        mMediator.onFinishNativeInitialization();
        doReturn(ConnectionSecurityLevel.WARNING).when(mLocationBarDataProvider).getSecurityLevel();

        // Focus the URL bar.
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        clearInvocations(mUrlCoordinator);

        mMediator.onSecurityStateChanged();

        // The warning should be null because the URL bar is focused.
        verify(mUrlCoordinator).setAccessibilityWarning(eq(null));

        // Unfocus the URL bar.
        clearInvocations(mUrlCoordinator);
        mMediator.onUrlFocusChange(/* hasFocus= */ false);

        // Now that it's unfocused, it should set the non-secure warning.
        verify(mUrlCoordinator)
                .setAccessibilityWarning(
                        eq(mContext.getString(R.string.page_info_not_secure_description)));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM)
    public void testUrlBarAccessibilityWarning_ntp_flagOn() {
        mMediator.onFinishNativeInitialization();
        doReturn(new GURL("chrome-native://newtab/")).when(mTab).getUrl();
        doReturn(ConnectionSecurityLevel.NONE).when(mLocationBarDataProvider).getSecurityLevel();
        clearInvocations(mUrlCoordinator);

        mMediator.onSecurityStateChanged();

        verify(mUrlCoordinator).setAccessibilityWarning(eq(null));
    }

    @Test
    public void testReparenting_onSessionRestoration_whenUrlAlreadyFocused() {
        // Start in a focused but not reparented state.
        mMediator.onUrlFocusChange(/* hasFocus= */ true);
        assertFalse(mMediator.isParentedToSuggestionsContainer());

        // Complete initialization after focus has already happened.
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        doReturn(mLocationBarParent).when(mLocationBarLayout).getParent();
        doReturn(mSuggestionsContainer).when(mAutocompleteCoordinator).getSuggestionsContainer();
        doReturn(mDropdown).when(mSuggestionsContainer).takeDropdownView();
        MarginLayoutParams layoutParams = new MarginLayoutParams(-2, -2);
        doReturn(layoutParams).when(mLocationBarLayout).getLayoutParams();
        doReturn(mPlaceholder)
                .when(mLocationBarLayout)
                .findViewById(R.id.suggestions_container_placeholder);
        int placeholderIndex = 2;
        doReturn(placeholderIndex).when(mLocationBarLayout).indexOfChild(mPlaceholder);

        mSessionState.getAutocompleteInput().setDisplayState(DisplayState.SUGGESTIONS);
        mMediator.beginInput(mSessionState.getAutocompleteInput());

        assertTrue(mMediator.isParentedToSuggestionsContainer());
        verify(mSuggestionsContainer).addView(mLocationBarLayout, 0, layoutParams);
        verify(mUrlCoordinator).startReparenting();
        verify(mUrlCoordinator).finishReparenting(true);
    }

    @Test
    public void testStandbyEndsWithRequestTypeChanged() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        mMediator.beginInput(input);
        verify(mLocationBarLayout, atLeastOnce()).setShowFocusRing(/* showFocusRing= */ true);
        verify(mLocationBarLayout, never()).setShowFocusRing(/* showFocusRing= */ false);
        clearInvocations(mLocationBarLayout);

        input.setDisplayState(DisplayState.SUGGESTIONS);
        input.setRequestType(AutocompleteRequestType.AI_MODE);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mLocationBarLayout).setShowFocusRing(/* showFocusRing= */ false);
    }

    @Test
    public void onUrlFocusChange_keyboardForward_entersStandbyNoPopover() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        mMediator.onUrlFocusChange(new UrlBarFocusChangeInfo(true, View.FOCUS_FORWARD));

        verify(mLocationBarLayout, atLeastOnce()).setShowFocusRing(/* showFocusRing= */ true);
        verify(mUrlCoordinator, never()).startReparenting();
    }

    @Test
    public void onUrlFocusChange_programmaticFocus_keepsExistingPath() {
        mMediator.onUrlFocusChange(new UrlBarFocusChangeInfo(true, View.FOCUS_DOWN));

        verify(mLocationBarLayout, atLeastOnce()).setShowFocusRing(/* showFocusRing= */ true);
    }

    @Test
    public void testTranslateDisplaySelectionToEditing() {
        // display: "youtube.com/?app=desktop", selection [3,7] = "tube"
        // editing: "www.youtube.com/?app=desktop", expect [7,11] = "tube"
        assertEquals(
                new TextSelection(7, 11),
                LocationBarMediator.translateDisplaySelectionToEditing(
                        new TextSelection(3, 7),
                        "youtube.com/?app=desktop",
                        "www.youtube.com/?app=desktop"));

        // Collapsed selection (just a cursor) -> unchanged, never select-all.
        assertEquals(
                new TextSelection(5, 5),
                LocationBarMediator.translateDisplaySelectionToEditing(
                        new TextSelection(5, 5), "youtube.com", "www.youtube.com"));
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testAlwaysShowAiModePrefTogglesAndSyncs() throws Exception {
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        verify(mUrlCoordinator).setShowAiMode(true);
        verify(mUrlCoordinator).setShowAiModeCallback(mCallbackCaptor.capture());
        assertNotNull(mCallbackCaptor.getValue());

        // Toggle the pref via callback (set to false) and verify it writes to PrefService
        // and updates the coordinator directly
        mCallbackCaptor.getValue().onResult(false);
        verify(mPrefService).setBoolean(Pref.SHOW_AI_MODE_OMNIBOX_BUTTON, false);
        verify(mUrlCoordinator).setShowAiMode(false);
    }

    @Test
    public void testAlwaysShowAiMode_disabledWhenNotAimEligible() {
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        ComposeplateUtils.setIsEnabledForTesting(false);
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        verify(mUrlCoordinator).setShowAiModeCallback(null);
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testAlwaysShowAiMode_aimEligible_notFuseboxEligible() {
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        ComposeplateUtils.setIsEnabledForTesting(true);
        doReturn(false).when(mComposeboxBridgeJni).isFuseboxEligibleForProfile(any());
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        verify(mUrlCoordinator).setShowAiMode(true);
        verify(mUrlCoordinator).setShowAiModeCallback(mCallbackCaptor.capture());
        assertNotNull(mCallbackCaptor.getValue());
    }

    @Test
    public void testHandleKeyNavigationEventIneligibleKey() {
        doReturn(KeyEvent.KEYCODE_A).when(mKeyEvent).getKeyCode();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        assertFalse(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_A, mKeyEvent));
    }

    @Test
    public void testHandleKeyNavigationEvent_activate() {
        doReturn(KeyEvent.KEYCODE_ENTER).when(mKeyEvent).getKeyCode();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(9999L).when(mKeyEvent).getEventTime();

        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_ENTER, mKeyEvent));
        verify(mAutocompleteCoordinator)
                .loadTypedOmniboxText(eq(9999L), eq(NavigationTarget.CURRENT_TAB));

        doReturn(true).when(mKeyEvent).isAltPressed();
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_ENTER, mKeyEvent));
        verify(mAutocompleteCoordinator)
                .loadTypedOmniboxText(eq(9999L), eq(NavigationTarget.NEW_TAB));

        doReturn(false).when(mKeyEvent).isAltPressed();
        doReturn(true).when(mKeyEvent).isShiftPressed();
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_ENTER, mKeyEvent));
        verify(mAutocompleteCoordinator)
                .loadTypedOmniboxText(eq(9999L), eq(NavigationTarget.NEW_WINDOW));
    }

    @Test
    public void testHandleKeyNavigationEvent_delegateToAutocomplete() {
        doReturn(KeyEvent.KEYCODE_TAB).when(mKeyEvent).getKeyCode();
        doReturn(true).when(mKeyEvent).hasNoModifiers();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(View.VISIBLE).when(mPlusButton).getVisibility();
        doReturn(View.VISIBLE).when(mDeleteButton).getVisibility();
        doReturn(View.GONE).when(mActivationChip).getVisibility();
        doReturn(true).when(mAutocompleteCoordinator).isServingSuggestions();

        var input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH).setUserText("user text");
        mMediator.beginInput(input);

        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        LocationBarSelectionController selectionController =
                mMediator.getSelectionControllerForTesting();
        assertEquals(1, selectionController.getPosition().intValue());
        doReturn(true).when(mAutocompleteCoordinator).selectFirstItem();
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertEquals(2, selectionController.getPosition().intValue());
        verify(mAutocompleteCoordinator).selectFirstItem();
        verify(mAutocompleteCoordinator).handleKeyEvent(KeyEvent.KEYCODE_TAB, mKeyEvent);
        assertTrue(selectionController.isAutocompleteListSelected());

        doReturn(true)
                .when(mAutocompleteCoordinator)
                .handleKeyEvent(KeyEvent.KEYCODE_TAB, mKeyEvent);
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertEquals(2, selectionController.getPosition().intValue());
        assertTrue(selectionController.isAutocompleteListSelected());

        doReturn(KeyEvent.KEYCODE_ENTER).when(mKeyEvent).getKeyCode();
        doReturn(true)
                .when(mAutocompleteCoordinator)
                .handleKeyEvent(KeyEvent.KEYCODE_ENTER, mKeyEvent);
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_ENTER, mKeyEvent));
        verify(mAutocompleteCoordinator).handleKeyEvent(KeyEvent.KEYCODE_ENTER, mKeyEvent);

        doReturn(KeyEvent.KEYCODE_TAB).when(mKeyEvent).getKeyCode();
        doReturn(false)
                .when(mAutocompleteCoordinator)
                .handleKeyEvent(KeyEvent.KEYCODE_TAB, mKeyEvent);
        when(mAutocompleteCoordinator.getSelectedIndex()).thenReturn(1, 2);
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertEquals(3, selectionController.getPosition().intValue());
        assertFalse(selectionController.isAutocompleteListSelected());
        verify(mAutocompleteCoordinator).resetSelection();

        doReturn(false).when(mKeyEvent).hasNoModifiers();
        doReturn(true).when(mKeyEvent).hasModifiers(KeyEvent.META_SHIFT_ON);
        doReturn(true).when(mAutocompleteCoordinator).selectLastItem();
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertEquals(2, selectionController.getPosition().intValue());
        verify(mAutocompleteCoordinator).selectFirstItem();
        assertTrue(selectionController.isAutocompleteListSelected());

        when(mAutocompleteCoordinator.getSelectedIndex()).thenReturn(2, 1);
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertFalse(selectionController.isAutocompleteListSelected());
        verify(mAutocompleteCoordinator, times(2)).resetSelection();
        assertEquals(1, selectionController.getPosition().intValue());
    }

    @Test
    public void testHandleKeyNavigationEventDelegateToFusebox() {
        doReturn(KeyEvent.KEYCODE_TAB).when(mKeyEvent).getKeyCode();
        doReturn(true).when(mKeyEvent).hasNoModifiers();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(View.VISIBLE).when(mPlusButton).getVisibility();
        doReturn(View.VISIBLE).when(mDeleteButton).getVisibility();
        doReturn(View.GONE).when(mActivationChip).getVisibility();
        doReturn(true).when(mAutocompleteCoordinator).isServingSuggestions();
        mHasAttachmentsSupplier.set(true);

        var input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH).setUserText("user text");
        mMediator.beginInput(input);

        LocationBarSelectionController selectionController =
                mMediator.getSelectionControllerForTesting();

        // Position 0: UrlBar
        // Position 1: DeleteButton (ActivationChip is GONE)
        // Position 2: FuseboxAttachments

        // Move to DeleteButton
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertEquals(1, selectionController.getPosition().intValue());

        // Move to FuseboxAttachments
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertEquals(2, selectionController.getPosition().intValue());
        verify(mFuseboxCoordinator).selectFirstAttachment();

        // Test delegation of key event to FuseboxCoordinator when FuseboxAttachments is selected
        doReturn(true).when(mFuseboxCoordinator).handleKeyEvent(KeyEvent.KEYCODE_TAB, mKeyEvent);
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        verify(mFuseboxCoordinator).handleKeyEvent(KeyEvent.KEYCODE_TAB, mKeyEvent);

        // Test moving backwards to FuseboxAttachments
        doReturn(false).when(mKeyEvent).hasNoModifiers();
        doReturn(true).when(mKeyEvent).hasModifiers(KeyEvent.META_SHIFT_ON);

        // Move from AutocompleteList (Position 3) back to FuseboxAttachments (Position 2)
        selectionController.selectAutocompleteList();
        assertEquals(3, selectionController.getPosition().intValue());
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertEquals(2, selectionController.getPosition().intValue());
        verify(mFuseboxCoordinator).selectLastAttachment();
    }

    @Test
    public void testOnPerformPasteAndGo() {
        mMediator.onFinishNativeInitialization();
        mMediator.onPerformPasteAndGo("pasted text");
        verify(mAutocompleteCoordinator)
                .loadPastedText(eq("pasted text"), anyLong(), eq(NavigationTarget.CURRENT_TAB));
    }

    @Test
    public void testHandleKeyNavigationEvent_ctrlTab_notHandled() {
        doReturn(KeyEvent.KEYCODE_TAB).when(mKeyEvent).getKeyCode();
        doReturn(false).when(mKeyEvent).hasNoModifiers();
        doReturn(false).when(mKeyEvent).hasModifiers(KeyEvent.META_SHIFT_ON);
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();

        assertFalse(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
    }

    @Test
    public void testHandleKeyNavigationEvent_dpadDown_standby_togglesEnabled() {
        doReturn(KeyEvent.KEYCODE_DPAD_DOWN).when(mKeyEvent).getKeyCode();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();

        var input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.STANDBY);
        mMediator.beginInput(input);

        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_DPAD_DOWN, mKeyEvent));
        assertAutocompleteState(AutocompleteState.ENABLED);
    }

    @Test
    public void testHandleKeyNavigationEvent_dpadDown_notStandby_doesNotToggle() {
        doReturn(KeyEvent.KEYCODE_DPAD_DOWN).when(mKeyEvent).getKeyCode();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();

        var input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.ENABLED);
        mMediator.beginInput(input);

        mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_DPAD_DOWN, mKeyEvent);
        assertAutocompleteState(AutocompleteState.ENABLED);
    }

    @Test
    public void testHandleKeyNavigationEvent_dpadUp_standby_togglesEnabled() {
        doReturn(KeyEvent.KEYCODE_DPAD_UP).when(mKeyEvent).getKeyCode();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();

        var input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.STANDBY);
        mMediator.beginInput(input);

        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_DPAD_UP, mKeyEvent));
        assertAutocompleteState(AutocompleteState.ENABLED);
    }

    @Test
    public void testHandleKeyNavigationEvent_dpadUp_notStandby_doesNotToggle() {
        doReturn(KeyEvent.KEYCODE_DPAD_UP).when(mKeyEvent).getKeyCode();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();

        var input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.ENABLED);
        mMediator.beginInput(input);

        mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_DPAD_UP, mKeyEvent);
        assertAutocompleteState(AutocompleteState.ENABLED);
    }

    @Test
    public void testHandleKeyNavigationEvent_urlBarAutocomplete() {
        doReturn(KeyEvent.KEYCODE_TAB).when(mKeyEvent).getKeyCode();
        doReturn(true).when(mKeyEvent).hasNoModifiers();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(View.VISIBLE).when(mPlusButton).getVisibility();
        doReturn(View.VISIBLE).when(mDeleteButton).getVisibility();
        doReturn(View.GONE).when(mActivationChip).getVisibility();
        doReturn(true).when(mAutocompleteCoordinator).isServingSuggestions();
        doReturn(true).when(mUrlCoordinator).hasAutocomplete();

        var input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH).setUserText("user text");
        mMediator.beginInput(input);

        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        verify(mUrlCoordinator).maybeAcceptInlineSuggestion(mKeyEvent);
    }

    @Test
    public void testHandleKeyNavigationEvent_tabToActivationChip() {
        doReturn(KeyEvent.KEYCODE_TAB).when(mKeyEvent).getKeyCode();
        doReturn(true).when(mKeyEvent).hasNoModifiers();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(View.VISIBLE).when(mActivationChip).getVisibility();

        var input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.beginInput(input);
        input.setInitialUserText("page.com");
        doReturn("page.com").when(mUrlCoordinator).getTextWithoutAutocomplete();
        clearInvocations(mUrlCoordinator);

        // Tab from UrlBar to ActivationChip.
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        verify(mActivationChip).setSelected(true);
        verify(mUrlCoordinator)
                .setUrlBarData(any(), eq(ScrollType.NO_SCROLL), eq(TextSelection.SELECT_END));
    }

    @Test
    public void testHandleKeyNavigationEvent_navigateToFirstItemInTypedState() {
        doReturn(KeyEvent.KEYCODE_TAB).when(mKeyEvent).getKeyCode();
        doReturn(true).when(mKeyEvent).hasNoModifiers();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(View.VISIBLE).when(mPlusButton).getVisibility();
        doReturn(View.GONE).when(mDeleteButton).getVisibility();
        doReturn(View.VISIBLE).when(mActivationChip).getVisibility();
        doReturn(true).when(mAutocompleteCoordinator).isServingSuggestions();
        doReturn(true).when(mAutocompleteCoordinator).selectFirstItem();

        var input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH).setUserText("user text");
        mMediator.beginInput(input);

        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        LocationBarSelectionController selectionController =
                mMediator.getSelectionControllerForTesting();
        verify(mAutocompleteCoordinator).selectFirstItem();
        verify(mAutocompleteCoordinator).handleKeyEvent(KeyEvent.KEYCODE_TAB, mKeyEvent);
        assertTrue(selectionController.isAutocompleteListSelected());

        doReturn(true)
                .when(mAutocompleteCoordinator)
                .handleKeyEvent(KeyEvent.KEYCODE_TAB, mKeyEvent);
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertEquals(2, selectionController.getPosition().intValue());
        assertTrue(selectionController.isAutocompleteListSelected());

        doReturn(false).when(mKeyEvent).hasNoModifiers();
        doReturn(true).when(mKeyEvent).hasModifiers(KeyEvent.META_SHIFT_ON);
        when(mAutocompleteCoordinator.getSelectedIndex()).thenReturn(1, 0);
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_TAB, mKeyEvent));
        assertFalse(selectionController.isAutocompleteListSelected());
    }

    @Test
    public void testHandleKeyNavigationEvent_downKeySelectsAutocomplete() {
        doReturn(KeyEvent.KEYCODE_DPAD_DOWN).when(mKeyEvent).getKeyCode();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(View.VISIBLE).when(mPlusButton).getVisibility();
        doReturn(View.VISIBLE).when(mDeleteButton).getVisibility();
        doReturn(View.VISIBLE).when(mActivationChip).getVisibility();
        doReturn(true).when(mAutocompleteCoordinator).isServingSuggestions();

        var input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH).setUserText("user text");
        mMediator.beginInput(input);

        LocationBarSelectionController selectionController =
                mMediator.getSelectionControllerForTesting();
        assertEquals(0, selectionController.getPosition().intValue());
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_DPAD_DOWN, mKeyEvent));
        assertEquals(3, selectionController.getPosition().intValue());
        assertTrue(selectionController.isAutocompleteListSelected());
        verify(mAutocompleteCoordinator).handleKeyEvent(KeyEvent.KEYCODE_DPAD_DOWN, mKeyEvent);
    }

    @Test
    public void testHandleKeyNavigationEvent_upKeySelectsAutocomplete() {
        doReturn(KeyEvent.KEYCODE_DPAD_UP).when(mKeyEvent).getKeyCode();
        doReturn(KeyEvent.ACTION_DOWN).when(mKeyEvent).getAction();
        doReturn(View.VISIBLE).when(mPlusButton).getVisibility();
        doReturn(View.VISIBLE).when(mDeleteButton).getVisibility();
        doReturn(View.VISIBLE).when(mActivationChip).getVisibility();
        doReturn(true).when(mAutocompleteCoordinator).isServingSuggestions();

        var input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH).setUserText("user text");
        mMediator.beginInput(input);

        LocationBarSelectionController selectionController =
                mMediator.getSelectionControllerForTesting();
        assertEquals(0, selectionController.getPosition().intValue());
        assertTrue(mMediator.handleKeyNavigationEvent(KeyEvent.KEYCODE_DPAD_UP, mKeyEvent));
        assertEquals(3, selectionController.getPosition().intValue());
        assertTrue(selectionController.isAutocompleteListSelected());
        verify(mAutocompleteCoordinator).handleKeyEvent(KeyEvent.KEYCODE_DPAD_UP, mKeyEvent);
    }

    @Test
    public void testShowUrlBarCursorWithoutFocusAnimations_disabledState_earlyReturns() {
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(true);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.DISABLED);

        mMediator.showUrlBarCursorWithoutFocusAnimations();

        assertFalse(mSessionState.isSessionActive());
    }

    @Test
    public void testShowUrlBarCursorWithoutFocusAnimations_enabledState_startsSession() {
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(true);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.ENABLED);

        mMediator.showUrlBarCursorWithoutFocusAnimations();

        assertTrue(mSessionState.isSessionActive());
        assertAutocompleteState(AutocompleteState.STANDBY);
    }

    @Test
    public void testShowUrlBarCursorWithoutFocusAnimations_activeSession_preservesExistingInput() {
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(true);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        mSessionState.getAutocompleteInput().setUserText("active text", TextSelection.SELECT_END);
        mSessionState.activate(mContext, mWebContents, mProfileSupplier, null);
        assertTrue(mSessionState.isSessionActive());

        mMediator.showUrlBarCursorWithoutFocusAnimations();

        assertEquals("active text", mSessionState.getAutocompleteInput().getUserText());
    }

    @Test
    public void testBeginInput_fromUnanimatedFocus_transitionsToEnabledAndDoesNotShowScrim() {
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(true);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.ENABLED);

        mMediator.showUrlBarCursorWithoutFocusAnimations();
        assertTrue(mSessionState.isSessionActive());
        assertAutocompleteState(AutocompleteState.STANDBY);
        assertTrue(mMediator.isUrlBarFocusedWithoutAnimation());
        verify(mScrimHandler, never()).setVisibility(true);

        mMediator.beginInput(
                new AutocompleteInput(OmniboxFocusReason.FAKE_BOX_TAP)
                        .setAutocompleteState(AutocompleteState.ENABLED));

        assertAutocompleteState(AutocompleteState.ENABLED);
        assertFalse(mMediator.isUrlBarFocusedWithoutAnimation());
        // We don't show the scrim on desktop
        verify(mScrimHandler, never()).setVisibility(true);
    }

    @Test
    public void testOnUrlFocusChange_regularFocus_transitionsToEnabledState() {
        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.DISABLED);

        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        assertAutocompleteState(AutocompleteState.ENABLED);
    }

    @Test
    public void testOnTabChanged_enabledState_transitionsToStandby() {
        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.ENABLED);
        mSessionState.activate(mContext, mWebContents, mProfileSupplier, null);
        mMediator.beginInput(mSessionState.getAutocompleteInput());
        assertTrue(mSessionState.isSessionActive());

        mMediator.onTabChanged(null);

        assertAutocompleteState(AutocompleteState.STANDBY);
        assertTrue(mMediator.isUrlBarFocusedWithoutAnimation());
    }

    @Test
    public void testOnTabChanged_activeInput_focusesCurrentTab() {
        mMediator.beginInput(mSessionState.getAutocompleteInput());
        mSessionState.deactivate();
        assertFalse(mSessionState.isSessionActive());
        mMediator.onTabChanged(null);

        verify(mTabView).requestFocus();
    }

    @Test
    public void testOnTabChanged_inactiveInput_doesNotFocusCurrentTab() {
        assertFalse(mSessionState.isSessionActive());
        mMediator.onTabChanged(null);

        verify(mTabView, never()).requestFocus();
    }

    @Test
    public void testTabSwitch_previouslyDeactivated_remainsDisabled() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);

        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.ENABLED);
        mSessionState.activate(mContext, mWebContents, mProfileSupplier, null);
        mMediator.beginInput(mSessionState.getAutocompleteInput());
        assertTrue(mSessionState.isSessionActive());

        mMediator.onUrlFocusChange(/* hasFocus= */ false);
        assertFalse(mSessionState.isSessionActive());
        assertAutocompleteState(AutocompleteState.DISABLED);

        // Simulate switching to another tab and back. We always return the same input anyway.
        mMediator.onTabChanged(null);
        mMediator.onTabChanged(null);
        assertFalse(mSessionState.isSessionActive());

        mMediator.onUrlChanged(/* isTabChanging= */ true);
        assertFalse(mSessionState.isSessionActive());
        assertAutocompleteState(AutocompleteState.DISABLED);
    }

    @Test
    public void testEscPress_transitionsStates() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);

        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.ENABLED);
        mSessionState.getAutocompleteInput().setDisplayState(DisplayState.SUGGESTIONS);
        mSessionState.activate(mContext, mWebContents, mProfileSupplier, null);
        mMediator.beginInput(mSessionState.getAutocompleteInput());
        assertTrue(mSessionState.isSessionActive());

        mSessionState.getAutocompleteInput().setUserText("query");
        mSessionState.getAutocompleteInput().setInitialUserText("example.com");

        assertTrue(mMediator.handleEscPress());
        assertAutocompleteState(AutocompleteState.STANDBY);
        assertEquals(DisplayState.DRAFTING, mSessionState.getAutocompleteInput().getDisplayState());
        assertEquals("query", mSessionState.getAutocompleteInput().getUserText());

        clearInvocations(mUrlCoordinator);
        assertTrue(mMediator.handleEscPress());
        assertAutocompleteState(AutocompleteState.STANDBY);
        assertEquals(DisplayState.DRAFTING, mSessionState.getAutocompleteInput().getDisplayState());
        assertEquals("example.com", mSessionState.getAutocompleteInput().getUserText());
        verify(mUrlCoordinator)
                .setUrlBarData(any(), eq(ScrollType.NO_SCROLL), eq(TextSelection.SELECT_ALL));

        assertTrue(mMediator.handleEscPress());
        assertFalse(mSessionState.isSessionActive());
        assertAutocompleteState(AutocompleteState.DISABLED);
        assertEquals(DisplayState.WEBSITE, mSessionState.getAutocompleteInput().getDisplayState());
    }

    @Test
    public void testEscPress_transitionsStates_withRealTextChange() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(/* hasDesktopExperience= */ true);
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        doAnswer(
                        invocation -> {
                            UrlBarData data = invocation.getArgument(0);
                            mMediator.onUrlTextChanged(data.displayText.toString());
                            return true;
                        })
                .when(mUrlCoordinator)
                .setUrlBarData(any(), anyInt(), any());

        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.ENABLED);
        mSessionState.getAutocompleteInput().setDisplayState(DisplayState.SUGGESTIONS);
        mSessionState.activate(mContext, mWebContents, mProfileSupplier, null);
        mMediator.beginInput(mSessionState.getAutocompleteInput());
        assertTrue(mSessionState.isSessionActive());

        mSessionState.getAutocompleteInput().setUserText("query");
        mSessionState.getAutocompleteInput().setInitialUserText("example.com");

        assertTrue(mMediator.handleEscPress());
        assertAutocompleteState(AutocompleteState.STANDBY);
        assertEquals(DisplayState.DRAFTING, mSessionState.getAutocompleteInput().getDisplayState());
        assertEquals("query", mSessionState.getAutocompleteInput().getUserText());
        assertTrue(mMediator.handleEscPress());
        assertAutocompleteState(AutocompleteState.STANDBY);
        assertEquals(DisplayState.DRAFTING, mSessionState.getAutocompleteInput().getDisplayState());
        assertEquals("example.com", mSessionState.getAutocompleteInput().getUserText());
        assertTrue(mMediator.handleEscPress());
        assertFalse(mSessionState.isSessionActive());
        assertAutocompleteState(AutocompleteState.DISABLED);
        assertEquals(DisplayState.WEBSITE, mSessionState.getAutocompleteInput().getDisplayState());
    }

    @Test
    public void testTabSwitch_withPreviewText_commitsPreviewText() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.ENABLED);
        mSessionState.activate(mContext, mWebContents, mProfileSupplier, null);
        mMediator.beginInput(mSessionState.getAutocompleteInput());
        assertTrue(mSessionState.isSessionActive());

        mSessionState.getAutocompleteInput().setUserText("www.");
        mSessionState.getAutocompleteInput().setInitialUserText("example.com");
        mSessionState.getAutocompleteInput().setPreviewText("www.example.com");

        mMediator.onTabChanged(null);

        assertAutocompleteState(AutocompleteState.STANDBY);
        assertEquals("www.example.com", mSessionState.getAutocompleteInput().getUserText());
        assertFalse(mSessionState.getAutocompleteInput().hasPreviewText());
        assertEquals(new TextSelection(4, 15), mSessionState.getAutocompleteInput().getSelection());
    }

    @Test
    public void testTabSwitch_maintainsPreviewMatchUrl() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        AutocompleteInput input = new AutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.beginInput(input);

        mSessionState.getAutocompleteInput().setPreviewMatchUrl(JUnitTestGURLs.RED_1);

        mMediator.suspendInput();

        // Switch to a different tab with its own session state.
        FuseboxSessionState nextTabSessionState = new FuseboxSessionState();
        nextTabSessionState.getAutocompleteInput().setPreviewMatchUrl(JUnitTestGURLs.BLUE_1);
        doReturn(nextTabSessionState).when(mLocationBarDataProvider).getFuseboxSessionState();
        mMediator.onTabChanged(null);

        assertEquals(
                JUnitTestGURLs.BLUE_1,
                nextTabSessionState.getAutocompleteInput().getPreviewMatchUrl());

        // Switch back to the original tab.
        mMediator.suspendInput();
        doReturn(mSessionState).when(mLocationBarDataProvider).getFuseboxSessionState();
        mMediator.onTabChanged(null);

        assertEquals(
                JUnitTestGURLs.RED_1, mSessionState.getAutocompleteInput().getPreviewMatchUrl());
    }

    @Test
    public void testEscPress_withPreviewText_upgradesToUserTextAndGoesToStandby() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.ENABLED);
        mSessionState.activate(mContext, mWebContents, mProfileSupplier, null);
        mMediator.beginInput(mSessionState.getAutocompleteInput());
        assertTrue(mSessionState.isSessionActive());

        mSessionState.getAutocompleteInput().setUserText("goo");
        mSessionState.getAutocompleteInput().setInitialUserText("example.com");
        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();
        doReturn("gle.com").when(mAutocompleteMatch).getInlineAutocompletion();
        mSessionState.getAutocompleteInput().setPreviewText("google.com");
        mMediator.onSuggestionsChanged(mAutocompleteMatch, /* hasSuggestions= */ true);

        assertEquals("google.com", mSessionState.getAutocompleteInput().getPreviewText());
        assertTrue(mSessionState.getAutocompleteInput().hasPreviewText());
        assertTrue(mMediator.handleEscPress());
        assertAutocompleteState(AutocompleteState.STANDBY);
        assertEquals("google.com", mSessionState.getAutocompleteInput().getUserText());
        assertEquals(new TextSelection(3, 10), mSessionState.getAutocompleteInput().getSelection());
        assertFalse(mSessionState.getAutocompleteInput().hasPreviewText());
    }

    @Test
    public void testTypeCharacter_replacingOriginalUrl_doesNotAutoCommit() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.ENABLED);
        mSessionState.activate(mContext, mWebContents, mProfileSupplier, null);
        mSessionState.getAutocompleteInput().setUserText("google.com");
        mSessionState.getAutocompleteInput().setSelection(TextSelection.SELECT_ALL);
        mMediator.beginInput(mSessionState.getAutocompleteInput());
        assertTrue(mSessionState.isSessionActive());

        mMediator.onUrlTextChanged("w");

        assertEquals("w", mSessionState.getAutocompleteInput().getUserText());

        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();
        doReturn("ikipedia.org").when(mAutocompleteMatch).getInlineAutocompletion();
        mSessionState.getAutocompleteInput().setPreviewText("wikipedia.org");
        mMediator.onSuggestionsChanged(mAutocompleteMatch, /* hasSuggestions= */ true);

        assertEquals("w", mSessionState.getAutocompleteInput().getUserText());
        assertEquals("wikipedia.org", mSessionState.getAutocompleteInput().getPreviewText());
        assertTrue(mSessionState.getAutocompleteInput().hasPreviewText());
    }

    @Test
    public void testOnSuggestionsChanged_withSiteSearchData_preservesSiteSearchLabel() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mSessionState.getAutocompleteInput().setAutocompleteState(AutocompleteState.ENABLED);
        mSessionState.activate(mContext, mWebContents, mProfileSupplier, null);
        mSessionState.getAutocompleteInput().setUserText("t");
        mSessionState
                .getAutocompleteInput()
                .setSiteSearchData(new SiteSearchData("bing.com", "Search Microsoft Bing"));
        mMediator.beginInput(mSessionState.getAutocompleteInput());
        doReturn(true).when(mUrlCoordinator).shouldAutocomplete();
        doReturn("est").when(mAutocompleteMatch).getInlineAutocompletion();
        mMediator.onSuggestionsChanged(mAutocompleteMatch, /* hasSuggestions= */ true);

        verify(mUrlCoordinator).setAutocompleteText("t", "est", null, "Search Microsoft Bing");
    }

    @Test
    public void testUrlBarAccessibilityOrder() {
        setUpMediatorAndCoordinator();
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.beginInput(input);
        mMediator.updateActivationChip();
        verify(mUrlBar, atLeastOnce())
                .setAccessibilityTraversalBefore(R.id.fusebox_activation_chip);

        input.setRequestType(AutocompleteRequestType.AI_MODE);
        doReturn(View.VISIBLE).when(mDeleteButton).getVisibility();
        mMediator.updateButtonVisibility();
        verify(mUrlBar, atLeastOnce()).setAccessibilityTraversalBefore(R.id.delete_button);

        doReturn(View.GONE).when(mDeleteButton).getVisibility();
        mMediator.updateButtonVisibility();
        verify(mUrlBar, atLeastOnce())
                .setAccessibilityTraversalBefore(R.id.omnibox_suggestions_dropdown);
    }

    @Test
    public void testOnWindowFocusChanged_updatesFocusRing() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);
        mSessionState.activate(mContext, mWebContents, mProfileSupplier, null);

        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.STANDBY);
        mMediator.beginInput(input);

        // By default, window is focused (mocked in setUp) and we are in STANDBY.
        // So focus ring should be shown.
        verify(mLocationBarLayout).setShowFocusRing(/* showFocusRing= */ true);

        // Lose window focus -> focus ring should be hidden.
        mWindowHasFocusSupplier.set(false);
        verify(mLocationBarLayout).setShowFocusRing(/* showFocusRing= */ false);

        // Regain window focus -> focus ring should be shown again.
        mWindowHasFocusSupplier.set(true);
        verify(mLocationBarLayout, times(2)).setShowFocusRing(/* showFocusRing= */ true);
    }

    @Test
    public void testBeginInput_ReentrancyGuard() {
        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        // Stub requestFocus to trigger focus change synchronously.
        doAnswer(
                        invocation -> {
                            mMediator.onUrlFocusChange(/* hasFocus= */ true);
                            return null;
                        })
                .when(mUrlCoordinator)
                .requestFocus();

        // Clear invocations to start fresh.
        clearInvocations(mUrlCoordinator);

        AutocompleteInput input = new AutocompleteInput();
        mMediator.beginInput(input);

        // Verify beginInput is called only once.
        verify(mUrlCoordinator).beginInput(any());
    }

    @Test
    public void testOnScrimClicked_draftingTextMatches_clearsSession() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        setupSession(DisplayState.DRAFTING, /* textDiffers= */ false);

        mMediator.onScrimClicked();

        assertFalse(mSessionState.isSessionActive());
        assertAutocompleteState(AutocompleteState.DISABLED);
        assertFalse(mMediator.isUrlBarFocused());
    }

    @Test
    public void testOnScrimClicked_draftingTextDiffers_enterDraftingNoFocus() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        setupSession(DisplayState.DRAFTING, /* textDiffers= */ true);

        mMediator.onScrimClicked();

        assertDraftingNoFocusProperties();
    }

    @Test
    public void testOnScrimClicked_suggestionsTextDiffers_enterDraftingNoFocus() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        beginInput(
                new AutocompleteInput()
                        .setDisplayState(DisplayState.SUGGESTIONS)
                        .setUserText(TEST_USER_TEXT)
                        .setInitialUserText(TEST_INITIAL_USER_TEXT));

        mMediator.onScrimClicked();

        assertDraftingNoFocusProperties();
    }

    @Test
    public void testEnterDraftingNoFocus_withPreviewText_commitsPreviewText() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        beginInput(
                new AutocompleteInput()
                        .setDisplayState(DisplayState.DRAFTING)
                        .setUserText("goo")
                        .setInitialUserText(TEST_INITIAL_USER_TEXT)
                        .setPreviewText("google.com"));

        mMediator.onScrimClicked();

        assertDraftingNoFocusProperties();
        assertUserText("google.com");
    }

    @Test
    public void testOnScrimClicked_nonDesktop_endsInput() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ false);
        beginInput(
                new AutocompleteInput()
                        .setDisplayState(DisplayState.SUGGESTIONS)
                        .setAutocompleteState(AutocompleteState.ENABLED));

        mMediator.onScrimClicked();

        assertFalse(mSessionState.isSessionActive());
        assertAutocompleteState(AutocompleteState.DISABLED);
        assertFalse(mMediator.isUrlBarFocused());
    }

    @Test
    public void testOnUrlFocusChange_losingFocus_draftingTextDiffers_enterDraftingNoFocus() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        setupSession(DisplayState.DRAFTING, /* textDiffers= */ true);

        mMediator.onUrlFocusChange(/* hasFocus= */ false);

        assertDraftingNoFocusProperties();
    }

    @Test
    public void testOnUrlFocusChange_losingFocus_draftingTextMatches_endsSession() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        setupSession(DisplayState.DRAFTING, /* textDiffers= */ false);

        mMediator.onUrlFocusChange(/* hasFocus= */ false);

        assertFalse(mSessionState.isSessionActive());
        assertFalse(mMediator.isUrlBarFocused());
    }

    @Test
    public void testOnUrlFocusChange_gainingFocus_fromDraftingNoFocus_resumeSession() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        beginInput(
                new AutocompleteInput()
                        .setDisplayState(DisplayState.DRAFTING_NO_FOCUS)
                        .setUserText(TEST_USER_TEXT)
                        .setPageUrl(JUnitTestGURLs.BLUE_1));

        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        assertTrue(mSessionState.isSessionActive());
        assertUserText(TEST_USER_TEXT);
        assertTrue(mMediator.isUrlBarFocused());
    }

    @Test
    public void testOnUrlFocusChange_gainingFocus_fromDraftingNoFocus_selectAllText() {
        beginInput(new AutocompleteInput().setDisplayState(DisplayState.DRAFTING_NO_FOCUS));

        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        assertEquals(TextSelection.SELECT_ALL, mSessionState.getAutocompleteInput().getSelection());
        verify(mUrlCoordinator)
                .setUrlBarData(
                        any(), eq(UrlBar.ScrollType.NO_SCROLL), eq(TextSelection.SELECT_ALL));
    }

    @Test
    public void testOnTabChanged_restoresDraftingNoFocusTabState() {
        AutocompleteInput input =
                new AutocompleteInput().setDisplayState(DisplayState.DRAFTING_NO_FOCUS);
        beginInput(input);

        FuseboxSessionState newTabState = new FuseboxSessionState();
        doReturn(newTabState).when(mLocationBarDataProvider).getFuseboxSessionState();
        mMediator.onTabChanged(null);

        doReturn(mSessionState).when(mLocationBarDataProvider).getFuseboxSessionState();
        mMediator.onTabChanged(null);

        assertDraftingNoFocusProperties();
    }

    @Test
    public void testOnUrlChanged_desktop_endsDraftingSession() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        AutocompleteInput input =
                new AutocompleteInput()
                        .setDisplayState(DisplayState.DRAFTING_NO_FOCUS)
                        .setPageUrl(JUnitTestGURLs.BLUE_1);
        beginInput(input);

        UrlBarData newUrlData = UrlBarData.forUrl(JUnitTestGURLs.RED_1);
        doReturn(newUrlData).when(mLocationBarDataProvider).getUrlBarData();
        clearInvocations(mUrlCoordinator);

        mMediator.onUrlChanged(/* isTabChanging= */ false);

        assertFalse(mSessionState.isSessionActive());
        verify(mUrlCoordinator, atLeastOnce())
                .setUrlBarData(
                        eq(newUrlData),
                        eq(UrlBar.ScrollType.SCROLL_TO_TLD),
                        eq(TextSelection.SELECT_ALL));
    }

    @Test
    public void testOnUrlChanged_tabChanging_preservesDraftingNoFocusSession() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        beginInput(
                new AutocompleteInput()
                        .setDisplayState(DisplayState.DRAFTING_NO_FOCUS)
                        .setAutocompleteState(AutocompleteState.STANDBY)
                        .setPageUrl(JUnitTestGURLs.BLUE_1));

        mMediator.onUrlChanged(/* isTabChanging= */ true);

        assertDraftingNoFocusProperties();
    }

    @Test
    public void testUpdateActivationChip_visibility() {
        setUpMediatorAndCoordinator();
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);

        // Initial zero-prefix focus on a webpage.
        input.setInitialUserText("page.com");
        input.setUserText("page.com");
        input.setPreviewMatchUrl(new GURL("https://page.com"));
        clearInvocations(mLocationBarLayout);
        mMediator.beginInput(input);
        verify(mLocationBarLayout, never()).setActivationChipVisibility(/* shouldShow= */ false);
        clearInvocations(mLocationBarLayout);

        input.setSiteSearchData(new SiteSearchData("test", "Test"));
        verify(mLocationBarLayout).setActivationChipVisibility(/* shouldShow= */ false);

        input.setSiteSearchData(null);
        verify(mLocationBarLayout).setActivationChipVisibility(/* shouldShow= */ true);

        // When user types a new URL, it hides the chip.
        input.setUserText("https://example.com");
        input.setPreviewMatchUrl(new GURL("https://example.com"));
        verify(mLocationBarLayout, times(2)).setActivationChipVisibility(/* shouldShow= */ false);

        input.setPreviewMatchUrl(null);
        verify(mLocationBarLayout, times(2)).setActivationChipVisibility(/* shouldShow= */ true);

        input.setRequestType(AutocompleteRequestType.AI_MODE);
        verify(mLocationBarLayout, times(3)).setActivationChipVisibility(/* shouldShow= */ false);

        mMediator.endInput();
    }

    @Test
    public void testUpdateActivationChip_windowFocusChanged() {
        setUpMediatorAndCoordinator();
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        input.setPreviewMatchUrl(null);
        clearInvocations(mLocationBarLayout);
        mMediator.beginInput(input);

        verify(mLocationBarLayout, never()).setActivationChipVisibility(/* shouldShow= */ false);
        clearInvocations(mLocationBarLayout);

        mWindowHasFocusSupplier.set(false);
        verify(mLocationBarLayout).setActivationChipVisibility(/* shouldShow= */ false);

        mWindowHasFocusSupplier.set(true);
        verify(mLocationBarLayout).setActivationChipVisibility(/* shouldShow= */ true);
    }

    @Test
    public void testUpdateActivationChip_prefChanged() {
        setUpMediatorAndCoordinator();
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        input.setPreviewMatchUrl(null);
        clearInvocations(mLocationBarLayout);
        mMediator.beginInput(input);

        verify(mLocationBarLayout, never()).setActivationChipVisibility(/* shouldShow= */ false);
        clearInvocations(mLocationBarLayout);

        doReturn(123L).when(mProfile).getNativeBrowserContextPointer();
        doReturn(false).when(mPrefService).getBoolean(Pref.SHOW_AI_MODE_OMNIBOX_BUTTON);
        mMediator.updateActivationChip();

        verify(mLocationBarLayout).setActivationChipVisibility(/* shouldShow= */ false);
    }

    @Test
    public void testUpdateActivationChip_draftingNoFocus() {
        setUpMediatorAndCoordinator();
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        input.setDisplayState(DisplayState.DRAFTING_NO_FOCUS);

        mMediator.beginInput(input);
        mMediator.updateActivationChip();

        verify(mLocationBarLayout, never()).setActivationChipVisibility(/* shouldShow= */ true);
    }

    @Test
    public void testOnUrlFocusChange_focusFromDraftingNoFocus_showsActivationChip() {
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        setupSession(DisplayState.DRAFTING_NO_FOCUS, /* textDiffers= */ true);
        clearInvocations(mLocationBarLayout);

        mMediator.onUrlFocusChange(/* hasFocus= */ true);

        assertEquals(DisplayState.DRAFTING, mSessionState.getAutocompleteInput().getDisplayState());
        verify(mLocationBarLayout).setActivationChipVisibility(/* shouldShow= */ true);
    }

    @Test
    public void testActivationChipClicked_transitionsStandbyToEnabled() {
        setUpMediatorAndCoordinator();
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setAutocompleteState(AutocompleteState.STANDBY);
        input.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.beginInput(input);

        mMediator.onActivationChipClicked();

        assertEquals(AutocompleteState.ENABLED, input.getAutocompleteState());
        assertEquals(AutocompleteRequestType.AI_MODE, input.getRequestType());
    }

    @Test
    public void testActivationChipClicked_otherText() {
        setUpMediatorAndCoordinator();
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        input.setInitialUserText("google.com");
        doReturn("suggestion text").when(mUrlCoordinator).getTextWithoutAutocomplete();
        mMediator.beginInput(input);
        clearInvocations(mAutocompleteCoordinator, mUrlCoordinator);

        mMediator.onActivationChipClicked();

        assertEquals(AutocompleteRequestType.AI_MODE, input.getRequestType());
        verify(mAutocompleteCoordinator)
                .loadTypedOmniboxText(anyLong(), eq(NavigationTarget.CURRENT_TAB));
    }

    @Test
    public void testActivationChipClicked_emptyText() {
        setUpMediatorAndCoordinator();
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        input.setInitialUserText("google.com");
        doReturn("").when(mUrlCoordinator).getTextWithoutAutocomplete();
        mMediator.beginInput(input);
        clearInvocations(mAutocompleteCoordinator, mUrlCoordinator);

        mMediator.onActivationChipClicked();

        assertEquals(AutocompleteRequestType.AI_MODE, input.getRequestType());
        verify(mAutocompleteCoordinator, never()).loadTypedOmniboxText(anyLong(), anyInt());
    }

    @Test
    public void testActivationChipClicked_currentUrl() {
        setUpMediatorAndCoordinator();
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.beginInput(input);
        input.setInitialUserText("google.com");
        doReturn("google.com").when(mUrlCoordinator).getTextWithoutAutocomplete();
        clearInvocations(mAutocompleteCoordinator, mUrlCoordinator);

        mMediator.onActivationChipClicked();

        assertEquals(AutocompleteRequestType.AI_MODE, input.getRequestType());
        verify(mAutocompleteCoordinator, never()).loadTypedOmniboxText(anyLong(), anyInt());
        verify(mUrlCoordinator)
                .setUrlBarData(any(), eq(ScrollType.NO_SCROLL), eq(TextSelection.SELECT_END));
    }

    @Test
    public void testActivationChipClicked_aimEligible_notFuseboxEligible_loadsComposeplateUrl() {
        GURL composeplateUrl = new GURL("https://google.com/aim");
        doReturn(composeplateUrl).when(mTemplateUrlService).getComposeplateUrl();
        doReturn(false).when(mComposeboxBridgeJni).isFuseboxEligibleForProfile(any());
        ComposeplateUtils.setIsEnabledForTesting(true);
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        setUpMediatorAndCoordinator();

        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        input.setInitialUserText("");
        doReturn("").when(mUrlCoordinator).getTextWithoutAutocomplete();
        mMediator.beginInput(input);

        mMediator.onActivationChipClicked();

        assertEquals(AutocompleteRequestType.AI_MODE, input.getRequestType());
        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(composeplateUrl.getSpec(), mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
    }

    @Test
    public void testActivationChip_aimEligible_notFuseboxEligible_visible() {
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        ComposeplateUtils.setIsEnabledForTesting(true);
        doReturn(false).when(mComposeboxBridgeJni).isFuseboxEligibleForProfile(any());

        mMediator.onFinishNativeInitialization();
        mProfileSupplier.set(mProfile);

        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        input.setInitialUserText("page.com");
        input.setUserText("page.com");
        clearInvocations(mLocationBarLayout);
        mMediator.beginInput(input);

        verify(mLocationBarLayout, never()).setActivationChipVisibility(/* shouldShow= */ false);
    }

    @Test
    public void testActivationChipSelectionChanged_clearsUrl() {
        setUpMediatorAndCoordinator();
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.beginInput(input);
        input.setInitialUserText("google.com");
        doReturn("google.com").when(mUrlCoordinator).getTextWithoutAutocomplete();
        clearInvocations(mUrlCoordinator);

        mMediator.onActivationChipSelectionChanged(true);

        verify(mUrlCoordinator)
                .setUrlBarData(any(), eq(ScrollType.NO_SCROLL), eq(TextSelection.SELECT_END));
    }

    @Test
    public void testActivationChipSelectionChanged_doesNotClearIfDifferent() {
        setUpMediatorAndCoordinator();
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.beginInput(input);
        input.setInitialUserText("google.com");
        doReturn("different text").when(mUrlCoordinator).getTextWithoutAutocomplete();
        clearInvocations(mUrlCoordinator);

        mMediator.onActivationChipSelectionChanged(true);

        verify(mUrlCoordinator, never()).setUrlBarData(any(), anyInt(), any());
    }

    @Test
    public void testActivationChipSelectionChanged_doesNotClearIfEmpty() {
        setUpMediatorAndCoordinator();
        AutocompleteInput input = mSessionState.getAutocompleteInput();
        input.setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.beginInput(input);
        input.setInitialUserText("google.com");
        doReturn("").when(mUrlCoordinator).getTextWithoutAutocomplete();
        clearInvocations(mUrlCoordinator);

        mMediator.onActivationChipSelectionChanged(true);

        verify(mUrlCoordinator, never()).setUrlBarData(any(), anyInt(), any());
    }

    @Test
    @Config(qualifiers = "w300dp")
    public void updatesActivationChipCompact_screenWidthTriggersCompact() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        mMediator.updateActivationChipCompact();
        verify(mLocationBarLayout).setActivationChipCompact(true);
    }

    @Test
    public void updateActivationChipCompact_textOverflowTriggersCompact() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        when(mLocationBarLayout.getUrlBarWidth()).thenReturn(100);
        when(mLocationBarLayout.getActivationChipCompactWidthDelta()).thenReturn(50);
        when(mLocationBarLayout.isActivationChipCompact()).thenReturn(false);
        when(mLocationBarLayout.getUrlBarTextWidth()).thenReturn(150);

        mMediator.updateActivationChipCompact();

        verify(mLocationBarLayout).setActivationChipCompact(true);
    }

    @Test
    public void updateActivationChipCompact_safeAgainstOscillation() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        when(mLocationBarLayout.getUrlBarTextWidth()).thenReturn(120);
        when(mLocationBarLayout.getActivationChipCompactWidthDelta()).thenReturn(50);

        // Initial state: expanded, url bar width is 100.
        when(mLocationBarLayout.getUrlBarWidth()).thenReturn(100);
        when(mLocationBarLayout.isActivationChipCompact()).thenReturn(false);
        mMediator.updateActivationChipCompact();
        verify(mLocationBarLayout).setActivationChipCompact(true);

        // Transitioned state: compact, url bar width grew to 150 because chip shrank by 50.
        when(mLocationBarLayout.getUrlBarWidth()).thenReturn(150);
        when(mLocationBarLayout.isActivationChipCompact()).thenReturn(true);
        mMediator.updateActivationChipCompact();

        // Should remain compact (never set to false).
        verify(mLocationBarLayout, never()).setActivationChipCompact(false);
    }

    @Test
    public void updateActivationChipCompact_isTextWrappingTriggersCompact() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        when(mLocationBarLayout.getUrlBarWidth()).thenReturn(100);
        when(mLocationBarLayout.getUrlBarTextWidth()).thenReturn(50);
        when(mLocationBarLayout.getActivationChipCompactWidthDelta()).thenReturn(50);
        when(mLocationBarLayout.isActivationChipCompact()).thenReturn(false);

        mMediator.setIsTextWrapping(true);

        verify(mLocationBarLayout).setActivationChipCompact(true);
    }

    @Test
    public void updateActivationChipCompact_urlBarWidthIncrease() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        verify(mLocationBarLayout)
                .addOnLayoutChangeListener(mOnLayoutChangeListenerCaptor.capture());
        when(mLocationBarLayout.getUrlBarTextWidth()).thenReturn(120);
        when(mLocationBarLayout.getActivationChipCompactWidthDelta()).thenReturn(50);

        // Initial state: overflowed with url bar width 150 (expanded baseline 100), chip is
        // compact.
        when(mLocationBarLayout.getUrlBarWidth()).thenReturn(150);
        when(mLocationBarLayout.isActivationChipCompact()).thenReturn(true);
        mMediator.updateActivationChipCompact();

        // Url bar width increases to 200 (expanded baseline is now 200 - 50 = 150 > 120 text
        // width).
        when(mLocationBarLayout.getUrlBarWidth()).thenReturn(200);

        // Simulate layout change where location bar width expands.
        mOnLayoutChangeListenerCaptor
                .getValue()
                .onLayoutChange(mLocationBarLayout, 0, 0, 400, 50, 0, 0, 300, 50);

        verify(mLocationBarLayout).setActivationChipCompact(false);
    }

    @Test
    public void updateActivationChipCompact_urlBarWidthDecrease() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(/* isDesktopPlatform= */ true);
        verify(mLocationBarLayout)
                .addOnLayoutChangeListener(mOnLayoutChangeListenerCaptor.capture());
        when(mLocationBarLayout.getUrlBarTextWidth()).thenReturn(120);
        when(mLocationBarLayout.getActivationChipCompactWidthDelta()).thenReturn(50);

        // Initial state: not overflowed with url bar width 200, chip is expanded.
        when(mLocationBarLayout.getUrlBarWidth()).thenReturn(200);
        when(mLocationBarLayout.isActivationChipCompact()).thenReturn(false);
        mMediator.updateActivationChipCompact();

        // Url bar width decreases to 100 (expanded baseline is 100 < 120 text width).
        when(mLocationBarLayout.getUrlBarWidth()).thenReturn(100);

        // Simulate layout change where location bar width shrinks.
        mOnLayoutChangeListenerCaptor
                .getValue()
                .onLayoutChange(mLocationBarLayout, 0, 0, 200, 50, 0, 0, 300, 50);

        verify(mLocationBarLayout).setActivationChipCompact(true);
    }

    @Test
    public void testHandleEscPress_suggestionsDisplayState_transitionsToDrafting() {
        mSessionState.getAutocompleteInput().setDisplayState(DisplayState.SUGGESTIONS);
        mSessionState.getAutocompleteInput().setRequestType(AutocompleteRequestType.SEARCH);
        mMediator.beginInput(mSessionState.getAutocompleteInput());

        assertTrue(mMediator.handleEscPress());
        assertEquals(DisplayState.DRAFTING, mSessionState.getAutocompleteInput().getDisplayState());
        assertAutocompleteState(AutocompleteState.STANDBY);
    }
}
