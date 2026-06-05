// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.animation.Animator;
import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.Window;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowPausedSystemClock;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.DeferredIMEWindowInsetApplicationCallback;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.RoundSides;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionInSuggest;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.preloading.PreloadingFeatureMap;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.SiteSearchData;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.AutocompleteStopReason;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.StarterPackId;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.function.Consumer;

/** Tests for {@link AutocompleteMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ShadowLooper.class)
public class AutocompleteMediatorUnitTest {
    private static final int SUGGESTION_MIN_HEIGHT = 20;
    private static final int HEADER_MIN_HEIGHT = 15;
    private static final GURL PAGE_URL = new GURL("https://www.site.com/page.html");
    private static final String PAGE_TITLE = "Page Title";
    private static final String TABS_STARTER_PACK_KEYWORD = "@tabs";

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock AutocompleteDelegate mAutocompleteDelegate;
    private @Mock UrlBarEditingTextStateProvider mTextStateProvider;
    private @Mock SuggestionProcessor mMockProcessor;
    private @Mock HeaderProcessor mMockHeaderProcessor;
    private @Mock AutocompleteController mAutocompleteController;
    private @Mock AutocompleteMatch mAutocompleteMatch;
    private @Mock AutocompleteController.Natives mControllerJniMock;
    private @Mock LocationBarDataProvider mLocationBarDataProvider;
    private @Mock ModalDialogManager mModalDialogManager;
    private @Mock OmniboxActionDelegateImpl mOmniboxActionDelegate;
    private @Mock LargeIconBridge.Natives mLargeIconBridgeJniMock;
    private @Mock NavigationHandle mNavigationHandle;
    private @Mock ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private @Mock WindowAndroid mWindowAndroid;
    private @Mock Activity mActivity;
    private @Mock Window mWindow;
    private @Mock View mDecorView;
    private @Mock OmniboxSuggestionsDropdownEmbedder mEmbedder;
    private @Mock InsetObserver mInsetObserver;
    private @Mock AutocompleteCoordinator.OmniboxSuggestionsVisualStateObserver
            mVisualStateObserver;
    private @Mock DeferredIMEWindowInsetApplicationCallback mDeferredImeCallback;
    private @Mock FuseboxCoordinator mFuseboxCoordinator;
    private @Mock PreloadingFeatureMap mPreloadingFeatureMap;
    private @Mock ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;
    private @Mock Callback<GURL> mGurlCallback;
    private @Captor ArgumentCaptor<OmniboxLoadUrlParams> mOmniboxLoadUrlParamsCaptor;
    private @Captor ArgumentCaptor<Consumer<SiteSearchData>> mKeywordModeEnteredCaptor;
    private @Captor ArgumentCaptor<Callback<GURL>> mUrlCallbackCaptor;
    private @Mock CachedZeroSuggestionsManager.OverridesForTesting
            mMockCachedZeroSuggestionsManager;
    private @Mock TemplateUrlService mTemplateUrlService;
    private @Mock Profile mProfile;
    private @Mock PrefService mPrefService;
    private @Mock TemplateUrl mTemplateUrl;
    private PropertyModel mListModel;
    private AutocompleteMediator mMediator;
    private List<AutocompleteMatch> mSuggestionsList;
    private AutocompleteResult mAutocompleteResult;
    private ModelList mSuggestionModels;
    private SettableNonNullObservableSupplier<@ControlsPosition Integer> mToolbarPositionSupplier;
    private SettableNonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier;
    private SettableNonNullObservableSupplier<@FuseboxCoordinator.FuseboxLayoutMode Integer>
            mFuseboxLayoutModeSupplier;
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        CachedZeroSuggestionsManager.setOverridesForTesting(mMockCachedZeroSuggestionsManager);
        UserPrefs.setPrefServiceForTesting(mPrefService);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        PreloadingFeatureMap.setInstanceForTesting(mPreloadingFeatureMap);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJniMock);
        AutocompleteControllerJni.setInstanceForTesting(mControllerJniMock);
        mToolbarPositionSupplier = ObservableSuppliers.createNonNull(ControlsPosition.TOP);
        mFuseboxStateSupplier = ObservableSuppliers.createNonNull(FuseboxState.DISABLED);
        mFuseboxLayoutModeSupplier = ObservableSuppliers.createNonNull(FuseboxLayoutMode.TOOLBAR);

        lenient().doReturn(mAutocompleteController).when(mControllerJniMock).getForProfile(any());

        mSuggestionModels = new ModelList();
        mListModel =
                new PropertyModel.Builder(SuggestionListProperties.ALL_KEYS)
                        .with(SuggestionListProperties.SUGGESTION_MODELS, mSuggestionModels)
                        .build();

        lenient().doReturn(mInsetObserver).when(mWindowAndroid).getInsetObserver();
        lenient().doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();
        lenient().doReturn(true).when(mActivity).hasWindowFocus();
        lenient().doReturn(mWindow).when(mWindowAndroid).getWindow();
        lenient().doReturn(mDecorView).when(mWindow).getDecorView();
        lenient()
                .doReturn(mToolbarPositionSupplier)
                .when(mLocationBarDataProvider)
                .getToolbarPositionSupplier();

        lenient()
                .doReturn(mFuseboxStateSupplier)
                .when(mFuseboxCoordinator)
                .getFuseboxStateSupplier();
        lenient()
                .doReturn(mFuseboxLayoutModeSupplier)
                .when(mFuseboxCoordinator)
                .getFuseboxLayoutModeSupplier();

        mMediator =
                new AutocompleteMediator(
                        mContext,
                        mAutocompleteDelegate,
                        mTextStateProvider,
                        mListModel,
                        new Handler(),
                        () -> mModalDialogManager,
                        null,
                        null,
                        mLocationBarDataProvider,
                        tabGroupId -> {},
                        url -> false,
                        mOmniboxActionDelegate,
                        mActivityLifecycleDispatcher,
                        mEmbedder,
                        mWindowAndroid,
                        mDeferredImeCallback,
                        mFuseboxCoordinator,
                        false);
        mMediator
                .getDropdownItemViewInfoListBuilderForTest()
                .registerSuggestionProcessor(mMockProcessor);
        mMediator
                .getDropdownItemViewInfoListBuilderForTest()
                .setHeaderProcessorForTest(mMockHeaderProcessor);

        lenient().doReturn(SUGGESTION_MIN_HEIGHT).when(mMockProcessor).getMinimumViewHeight();
        lenient().doReturn(true).when(mMockProcessor).doesProcessSuggestion(any(), anyInt());
        lenient()
                .doAnswer((invocation) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS))
                .when(mMockProcessor)
                .createModel();
        lenient().doReturn(OmniboxSuggestionUiType.DEFAULT).when(mMockProcessor).getViewTypeId();

        lenient()
                .doAnswer((invocation) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS))
                .when(mMockHeaderProcessor)
                .createModel();
        lenient().doReturn(HEADER_MIN_HEIGHT).when(mMockHeaderProcessor).getMinimumViewHeight();
        lenient()
                .doReturn(OmniboxSuggestionUiType.HEADER)
                .when(mMockHeaderProcessor)
                .getViewTypeId();

        mSuggestionsList = buildSampleSuggestionsList(10, "Suggestion");
        mAutocompleteResult = spy(AutocompleteResult.fromCache(mSuggestionsList, null));
        lenient()
                .doReturn(mAutocompleteResult)
                .when(mMockCachedZeroSuggestionsManager)
                .readFromCache(anyInt());
        lenient().doReturn(true).when(mAutocompleteDelegate).isKeyboardActive();
        lenient()
                .when(mAutocompleteController.onSuggestionTouchDown(any(), any(), anyInt()))
                .thenReturn(true);
        setUpLocationBarDataProvider(
                JUnitTestGURLs.NTP_URL,
                "New Tab Page",
                PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE);

        mMediator.setOmniboxSuggestionsVisualStateObserver(mVisualStateObserver);
        mMediator.onTopResumedActivityChanged(true);
    }

    /**
     * Build a fake suggestions list with elements named 'Suggestion #', where '#' is the suggestion
     * index (1-based).
     *
     * @return List of suggestions.
     */
    private List<AutocompleteMatch> buildSampleSuggestionsList(int count, String prefix) {
        List<AutocompleteMatch> list = new ArrayList<>();
        for (int index = 0; index < count; ++index) {
            AutocompleteMatchBuilder builder =
                    AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                            .setDisplayText(prefix + (index + 1));
            if (index == 0) {
                builder.setInlineAutocompletion("inline_autocomplete")
                        .setAllowedToBeDefaultMatch(true);
            }
            list.add(builder.build());
        }

        return list;
    }

    /**
     * Build AutocompleteInput for the supplied input parameters.
     *
     * @param url The URL to report as a current URL.
     * @param title The Page Title to report.
     * @param pageClassification The Page classification to report.
     */
    private FuseboxSessionState createSession(GURL url, String title, int pageClassification) {
        var autocompleteInput = new AutocompleteInput();
        autocompleteInput.setPageUrl(url);
        autocompleteInput.setPageTitle(title);
        autocompleteInput.setPageClassification(pageClassification);

        var session = mock(FuseboxSessionState.class);
        lenient().doReturn(mProfile).when(session).getProfile();
        lenient().doReturn(mAutocompleteController).when(session).getAutocompleteController();
        lenient().doReturn(autocompleteInput).when(session).getAutocompleteInput();
        lenient()
                .doReturn(mComposeboxQueryControllerBridge)
                .when(session)
                .getComposeboxQueryControllerBridge();
        return session;
    }

    private FuseboxSessionState createEmptySession() {
        return createSession(PAGE_URL, PAGE_TITLE, PageClassification.BLANK_VALUE);
    }

    private FuseboxSessionState createSession(@AutocompleteRequestType int requestType) {
        var session = createSession(PAGE_URL, PAGE_TITLE, PageClassification.OTHER_VALUE);
        session.getAutocompleteInput().setRequestType(requestType);
        return session;
    }

    private void loadUrlForOmniboxMatch(GURL url) {
        mMediator.loadUrlForOmniboxMatch(
                /* matchIndex= */ 0,
                mAutocompleteMatch,
                url,
                /* inputStart= */ 0,
                /* openInNewTab= */ false,
                /* openInNewWindow= */ false);
    }

    private void setUpSessionAndMatch(
            @AutocompleteRequestType int requestType, @OmniboxSuggestionType int matchType) {
        var session = createSession(requestType);
        mMediator.beginInput(session);
        doReturn(matchType).when(mAutocompleteMatch).getType();
        doReturn(AutocompleteMatch.isWhatYouTyped(matchType))
                .when(mAutocompleteMatch)
                .isWhatYouTyped();
    }

    private void verifyLoadUrl(GURL expectedUrl) {
        verify(mAutocompleteDelegate).loadUrl(mOmniboxLoadUrlParamsCaptor.capture());
        assertEquals(expectedUrl.getSpec(), mOmniboxLoadUrlParamsCaptor.getValue().url);
    }

    private void verifySuggestionModelsRoundSides(@RoundSides int roundSides) {
        assertTrue(mSuggestionModels.size() > 0);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            PropertyModel model = mSuggestionModels.get(i).model;
            if (model.containsKey(SuggestionCommonProperties.BG_ROUND_SIDES)) {
                assertEquals(
                        "Unexpected round sides for suggestion at position " + i,
                        roundSides,
                        model.get(SuggestionCommonProperties.BG_ROUND_SIDES));
            }
        }
    }

    /**
     * Set up LocationBarDataProvider to report supplied values.
     *
     * @param url The URL to report as a current URL.
     * @param title The Page Title to report.
     * @param pageClassification The Page classification to report.
     */
    void setUpLocationBarDataProvider(GURL url, String title, int pageClassification) {
        lenient().when(mLocationBarDataProvider.hasTab()).thenReturn(true);
        lenient().when(mLocationBarDataProvider.getCurrentGurl()).thenReturn(url);
        lenient().when(mLocationBarDataProvider.getTitle()).thenReturn(title);
        lenient()
                .when(mLocationBarDataProvider.getPageClassification(/* prefetch= */ false))
                .thenReturn(pageClassification);
    }

    /** Sets the native object reference for all suggestions in mSuggestionList. */
    void setSuggestionNativeObjectRef() {
        for (int index = 0; index < mSuggestionsList.size(); index++) {
            mSuggestionsList.get(index).updateNativeObjectRef(index + 1);
        }
    }

    @Test
    @SmallTest
    public void beginEndInput_propagatesSessionStateToProcessors() {
        // beginInput must notify processors that the session is active so per-session
        // state (e.g. cached suggestion images) can be set up.
        mMediator.beginInput(createEmptySession());
        verify(mMockProcessor, atLeastOnce()).onOmniboxSessionStateChange(true);
        verify(mMockHeaderProcessor, atLeastOnce()).onOmniboxSessionStateChange(true);

        clearInvocations(mMockProcessor, mMockHeaderProcessor);

        // endInput must notify processors that the session has ended so per-session
        // state can be released. Regression test: this used to be propagated as `true`,
        // preventing DropdownItemViewInfoListBuilder from clearing its image cache.
        mMediator.endInput();
        verify(mMockProcessor, atLeastOnce()).onOmniboxSessionStateChange(false);
        verify(mMockHeaderProcessor, atLeastOnce()).onOmniboxSessionStateChange(false);
        verify(mMockProcessor, never()).onOmniboxSessionStateChange(true);
        verify(mMockHeaderProcessor, never()).onOmniboxSessionStateChange(true);
    }

    @Test
    @SmallTest
    public void endInput_clearsSiteSearchChip() {
        var session = createEmptySession();
        mMediator.beginInput(session);
        session.getAutocompleteInput()
                .setSiteSearchData(new SiteSearchData("history", "Search history"));
        verify(mTextStateProvider).setSiteSearchChip("Search history");

        mMediator.endInput();
        verify(mTextStateProvider).setSiteSearchChip(null);
    }

    /**
     * Verifies that triggerSiteSearch successfully executes a SITE_SEARCH action when a matching
     * suggestion with an action is available.
     */
    @Test
    @SmallTest
    public void triggerSiteSearchSpaceSuccess() {
        // Setup: Start session and mock text state.
        var session = createEmptySession();
        mMediator.beginInput(session);

        doReturn(true)
                .when(mPrefService)
                .getBoolean(AutocompleteMediator.KEYWORD_SPACE_TRIGGERING_ENABLED_PREF);
        doReturn("bing").when(mTextStateProvider).getTextWithoutAutocomplete();
        doReturn(true).when(mTemplateUrlService).isLoaded();
        var mockTemplateUrl = mock(TemplateUrl.class);
        doReturn("bing").when(mockTemplateUrl).getKeyword();
        doReturn("Bing").when(mockTemplateUrl).getShortName();
        doReturn(mockTemplateUrl).when(mAutocompleteController).getTemplateUrlForText("bing");

        assertTrue(mMediator.triggerSiteSearch(SiteSearchActivationSource.SPACE));
        verify(mAutocompleteDelegate).setOmniboxEditingText("");
    }

    /** Verifies that triggerSiteSearch fails (returns false) if the Omnibox text is empty. */
    @Test
    @SmallTest
    public void triggerSiteSearch_Failure_NoText() {
        mMediator.beginInput(createEmptySession());
        doReturn("").when(mTextStateProvider).getTextWithoutAutocomplete();

        assertFalse(mMediator.triggerSiteSearch(SiteSearchActivationSource.SPACE));
    }

    @Test
    @SmallTest
    public void triggerSiteSearch_Failure_AlreadyInSiteSearchMode() {
        var session = createEmptySession();
        mMediator.beginInput(session);
        session.getAutocompleteInput()
                .setSiteSearchData(new SiteSearchData("existing", "Existing site search"));

        doReturn(true)
                .when(mPrefService)
                .getBoolean(AutocompleteMediator.KEYWORD_SPACE_TRIGGERING_ENABLED_PREF);
        doReturn("bing").when(mTextStateProvider).getTextWithoutAutocomplete();
        doReturn(true).when(mTemplateUrlService).isLoaded();
        var mockTemplateUrl = mock(TemplateUrl.class);
        doReturn("bing").when(mockTemplateUrl).getKeyword();
        doReturn("Bing").when(mockTemplateUrl).getShortName();
        doReturn(mockTemplateUrl).when(mAutocompleteController).getTemplateUrlForText("bing");

        assertFalse(mMediator.triggerSiteSearch(SiteSearchActivationSource.SPACE));
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_worksWithNullList() {
        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(null, null), /* isFinal= */ true);

        assertEquals(0, mSuggestionModels.size());
        assertFalse(mListModel.get(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE));
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_worksWithEmptyList() {
        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(null, null), /* isFinal= */ true);

        assertEquals(0, mSuggestionModels.size());
        assertFalse(mListModel.get(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE));
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_scrolEventsWithConcealedItemsTogglesKeyboardVisibility() {
        mMediator.beginInput(createEmptySession());

        final int heightWithOneConcealedItem =
                (mSuggestionsList.size() - 2) * SUGGESTION_MIN_HEIGHT;

        mMediator.onSuggestionDropdownHeightChanged(heightWithOneConcealedItem);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), /* isFinal= */ true);

        // With fully concealed elements, scroll should trigger keyboard hide.
        reset(mAutocompleteDelegate);
        mMediator.onSuggestionDropdownScroll();
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(false), anyBoolean());
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(true), anyBoolean());

        // Pretend that the user scrolled back to top with an overscroll.
        // This should bring back the soft keyboard.
        reset(mAutocompleteDelegate);
        mMediator.onSuggestionDropdownOverscrolledToTop();
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(true), anyBoolean());
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(false), anyBoolean());
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_updateHeightWhenHardwareKeyboardIsConnected() {
        // Simulates behavior of physical keyboard being attached to the device.
        // In this scenario, requesting keyboard to come up will not result with an actual
        // keyboard showing on the screen. As a result, the updated height should be used
        // when estimating presence of fully concealed items on the suggestions list.
        //
        // Attaching and detaching physical keyboard will affect the space on the screen, but since
        // the list of suggestions does not change, we are keeping them in exactly the same order
        // (and keep the grouping prior to the change).
        // The grouping is only affected, when the new list is provided (as a result of user's
        // input).
        final int heightOfOAllSuggestions = mSuggestionsList.size() * SUGGESTION_MIN_HEIGHT;
        final int heightWithOneConcealedItem =
                (mSuggestionsList.size() - 1) * SUGGESTION_MIN_HEIGHT;

        // This will request keyboard to show up upon receiving next suggestions list.
        when(mAutocompleteDelegate.isKeyboardActive()).thenReturn(true);
        // Report the height of the suggestions list, indicating that the keyboard is not visible.
        // In both cases, the updated suggestions list height should be used to estimate presence of
        // fully concealed items on the suggestions list.
        mMediator.onSuggestionDropdownHeightChanged(heightOfOAllSuggestions);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), /* isFinal= */ true);

        // Build separate list of suggestions so that these are accepted as a new set.
        // We want to follow the same restrictions as the original list (specifically: have a
        // resulting list of suggestions taller than the space in dropdown view), so make sure
        // the list sizes are same.
        List<AutocompleteMatch> newList =
                buildSampleSuggestionsList(mSuggestionsList.size(), "SuggestionB");
        mMediator.onSuggestionDropdownHeightChanged(heightWithOneConcealedItem);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(newList, null), /* isFinal= */ true);
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_rejectsHeightUpdatesWhenKeyboardIsHidden() {
        // Simulates scenario where we receive dropdown height update after software keyboard is
        // explicitly hidden. In this scenario the updates should be rejected when estimating
        // presence of fully concealed items on the suggestions list.
        final int heightOfOAllSuggestions = mSuggestionsList.size() * SUGGESTION_MIN_HEIGHT;
        final int heightWithOneConcealedItem =
                (mSuggestionsList.size() - 1) * SUGGESTION_MIN_HEIGHT;

        // Report height change with keyboard visible
        mMediator.onSuggestionDropdownHeightChanged(heightWithOneConcealedItem);
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(mSuggestionsList, null), true);

        // "Hide keyboard", report larger area and re-evaluate the results. We should see no
        // difference, as the logic should only evaluate presence of items concealed when keyboard
        // is active.
        when(mAutocompleteDelegate.isKeyboardActive()).thenReturn(false);
        mMediator.onSuggestionDropdownHeightChanged(heightOfOAllSuggestions);
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(mSuggestionsList, null), true);
    }

    @Test
    public void setSessionState_mobileMode_emptyOmnibox() {
        // In Mobile mode, if LocationBar clears the Page URL on focus, Autocomplete requests
        // Zero-Prefix suggestions.
        OmniboxCapabilities.setHasDesktopExperienceForTesting(false);

        GURL url = new GURL("https://www.google.com");
        String title = "title";
        int pageClassification = PageClassification.BLANK_VALUE;

        mMediator.beginInput(createSession(url, title, pageClassification));
        RobolectricUtil.runAllBackgroundAndUi();
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
    }

    @Test
    public void setSessionState_mobileMode_populatedOmnibox() {
        // In Mobile mode, if LocationBar does not clear the Page URL on focus, Autocomplete
        // requests Prefixed suggestions.
        OmniboxCapabilities.setHasDesktopExperienceForTesting(false);

        GURL url = new GURL("https://www.google.com");
        String title = "title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);
        session.getAutocompleteInput().setUserText("test");

        mMediator.beginInput(session);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verifyAutocompleteStart(url, pageClassification, "test", 0, true);
    }

    @Test
    public void testIsDesktopPlatform() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        assertTrue(OmniboxCapabilities.isDesktopPlatform());

        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        assertFalse(OmniboxCapabilities.isDesktopPlatform());

        OmniboxCapabilities.setIsDesktopPlatformForTesting(null);
        // Verify it doesn't crash and returns the default value.
        OmniboxCapabilities.isDesktopPlatform();
    }

    public void verifyAutocompleteStart(
            GURL url, int pageClass, String userText, int cursorPos, boolean preventAutocomplete) {
        var captor = ArgumentCaptor.forClass(AutocompleteInput.class);
        verify(mAutocompleteController)
                .start(any(), captor.capture(), eq(cursorPos), eq(preventAutocomplete));
        verify(mAutocompleteController, times(1)).start(any(), any(), anyInt(), anyBoolean());

        AutocompleteInput input = captor.getValue();
        assertEquals(pageClass, input.getPageClassification());
        assertEquals(userText, input.getUserText());
        assertEquals(url.getSpec(), input.getPageUrl().getSpec());

        clearInvocations(mAutocompleteController);
    }

    public void verifyAutocompleteStartZeroSuggest(
            String userText, GURL url, int pageClass, String pageTitle) {
        var captor = ArgumentCaptor.forClass(AutocompleteInput.class);
        verify(mAutocompleteController).startZeroSuggest(any(), captor.capture());
        verify(mAutocompleteController, times(1)).startZeroSuggest(any(), any());

        AutocompleteInput input = captor.getValue();
        assertEquals(pageClass, input.getPageClassification());
        assertEquals(userText, input.getUserText());
        assertEquals(url.getSpec(), input.getPageUrl().getSpec());
        assertEquals(pageTitle, input.getPageTitle());

        clearInvocations(mAutocompleteController);
    }

    @Test
    public void setSessionState_desktopMode() {
        // In Desktop mode, Omnibox always retains the Page URL on focus.
        // Autocomplete should continue to request the Zero-Prefix suggestions.
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);

        GURL url = new GURL("https://www.google.com");
        String title = "title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);
        session.getAutocompleteInput().setUserText("Text").setInitialUserText("Text");

        mMediator.beginInput(session);
        RobolectricUtil.runAllBackgroundAndUi();
        // Strictly expect the call to `startZeroSuggest()` here, as Desktop mode retains the
        // Omnibox content on focus.
        verifyAutocompleteStartZeroSuggest("Text", url, pageClassification, title);
    }

    @Test
    @SmallTest
    public void onTextChanged_emptyTextTriggersZeroSuggest() {

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        mMediator.beginInput(createSession(url, title, pageClassification));

        RobolectricUtil.runAllBackgroundAndUi();
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
    }

    @Test
    @SmallTest
    public void onInputChanged_initialTextTriggersZeroSuggest() {
        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);
        session.getAutocompleteInput()
                .setUserText("initial text")
                .setInitialUserText("initial text");
        mMediator.beginInput(session);

        RobolectricUtil.runAllBackgroundAndUi();
        verifyAutocompleteStartZeroSuggest("initial text", url, pageClassification, title);
    }

    @Test
    @SmallTest
    public void onTextChanged_noZeroSuggestInKeywordMode() {
        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);
        SiteSearchData data = new SiteSearchData("keyword", "Full Name");
        session.getAutocompleteInput().setSiteSearchData(data);

        mMediator.beginInput(session);

        RobolectricUtil.runAllBackgroundAndUi();
        var captor = ArgumentCaptor.forClass(AutocompleteInput.class);
        verify(mAutocompleteController, never()).startZeroSuggest(any(), captor.capture());
        clearInvocations(mAutocompleteController);
    }

    @Test
    @SmallTest
    public void onInputChanged_userTextDiffersFromInitialText_triggersPrefixedSuggest() {
        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);
        session.getAutocompleteInput().setUserText("user text").setInitialUserText("initial text");

        when(mTextStateProvider.getSelectionStart()).thenReturn(9);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(9);
        when(mTextStateProvider.shouldAutocomplete()).thenReturn(false);

        mMediator.beginInput(session);

        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verifyAutocompleteStart(url, pageClassification, "user text", 9, true);
    }

    @Test
    @SmallTest
    public void onTextChanged_nonEmptyTextTriggersSuggestions() {

        GURL url = JUnitTestGURLs.BLUE_1;
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, url.getSpec(), pageClassification);
        mMediator.beginInput(session);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        session.getAutocompleteInput().setUserText("test");
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verifyAutocompleteStart(url, pageClassification, "test", 4, false);
    }

    @Test
    @SmallTest
    public void onTextChanged_cancelsPendingRequests() {

        GURL url = JUnitTestGURLs.BLUE_1;
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, url.getSpec(), pageClassification);
        mMediator.beginInput(session);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        session.getAutocompleteInput().setUserText("test");
        session.getAutocompleteInput().setUserText("nottest");
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verifyAutocompleteStart(url, pageClassification, "nottest", 4, false);
    }

    @Test
    @SmallTest
    public void setSessionState_preventsTypedSuggestRequestOnDeactivation() {
        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);
        session.getAutocompleteInput().setUserText("text");

        // Simulate URL being focus changes.
        mMediator.beginInput(session);
        mMediator.endInput();
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mAutocompleteController, never()).startZeroSuggest(any(), any());

        // Simulate native being inititalized. Make sure no suggest requests are sent.
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mAutocompleteController, never()).start(any(), any(), anyInt(), anyBoolean());
    }

    @Test
    @SmallTest
    public void onSuggestionsReceived_triggersPrewarm() {
        mMediator.beginInput(createEmptySession());

        when(mPreloadingFeatureMap.shouldPrewarmOnAutocomplete()).thenReturn(true);
        Tab tab = mock(Tab.class);
        WebContents webContents = mock(WebContents.class);
        when(mLocationBarDataProvider.getTab()).thenReturn(tab);
        when(tab.getWebContents()).thenReturn(webContents);

        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ false);
        verify(mAutocompleteController).startPrewarm(eq(webContents));
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    @EnableFeatures(OmniboxFeatureList.AIM_SUPPRESS_VERBATIM_MATCH)
    public void onSuggestionsReceived_sendsOnSuggestionsChanged() {
        FuseboxSessionState session = createEmptySession();
        mMediator.beginInput(session);
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(mSuggestionsList, null), true);
        verify(mAutocompleteDelegate).onSuggestionsChanged(any(), anyBoolean());

        // Ensure duplicate requests are not suppressed, to preserve the
        // relationship between Native and Java AutocompleteResult objects.
        AutocompleteMatch defaultMatch =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("Suggestion1")
                        .setInlineAutocompletion("inline_autocomplete2")
                        .setAllowedToBeDefaultMatch(true)
                        .build();
        mSuggestionsList.remove(0);
        mSuggestionsList.add(0, defaultMatch);
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(mSuggestionsList, null), true);
        verify(mAutocompleteDelegate).onSuggestionsChanged(defaultMatch, true);

        // Clear the suggestions list so that we do not detect "unchanged suggestions" in the next
        // step. When suggestions are unchanged, we won't rebuild the list, and the events below
        // will not trigger.
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(null, null), true);
        clearInvocations(mAutocompleteDelegate);

        defaultMatch =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED)
                        .setDisplayText("Suggestion1")
                        .setInlineAutocompletion("inline_autocomplete2")
                        .setAllowedToBeDefaultMatch(true)
                        .build();
        mSuggestionsList.clear();
        mSuggestionsList.add(0, defaultMatch);
        var autocompleteInput = session.getAutocompleteInput();
        autocompleteInput.setRequestType(AutocompleteRequestType.AI_MODE);
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(mSuggestionsList, null), true);
        verify(mAutocompleteDelegate).onSuggestionsChanged(defaultMatch, false);
    }

    @Test
    @SmallTest
    public void onSuggestionClicked_TabsStarterPack() {
        mMediator.onNativeInitialized();
        mMediator.beginInput(createEmptySession());
        mMediator
                .getAutocompleteInputForTesting()
                .setSiteSearchData(
                        new SiteSearchData(
                                TABS_STARTER_PACK_KEYWORD,
                                "Tabs",
                                /* enteredViaSpace= */ false,
                                StarterPackId.TABS));
        doReturn(true).when(mOmniboxActionDelegate).switchToTab(anyInt(), any());
        GURL url = new GURL("https://example.com");
        AutocompleteMatch match =
                new AutocompleteMatchBuilder()
                        .setHasTabMatch(true)
                        .setType(OmniboxSuggestionType.OPEN_TAB)
                        .setAndroidTabId(123)
                        .setActions(
                                List.of(
                                        new OmniboxActionInSuggest(
                                                /* nativeInstance= */ 0,
                                                "hint",
                                                "acc",
                                                /* actionType= */ 1002,
                                                "",
                                                /* tabId= */ 123,
                                                /* presentationMode= */ 1)))
                        .build();

        mMediator.onSuggestionClicked(match, 0, url);

        verify(mOmniboxActionDelegate).switchToTab(eq(123), eq(url));
        verify(mAutocompleteDelegate, never()).loadUrl(any());
    }

    @Test
    @SmallTest
    public void onSuggestionClicked_TabsStarterPack_NonOpenTabMatch() {
        mMediator.onNativeInitialized();
        mMediator.beginInput(createEmptySession());
        mMediator
                .getAutocompleteInputForTesting()
                .setSiteSearchData(
                        new SiteSearchData(
                                TABS_STARTER_PACK_KEYWORD,
                                "Tabs",
                                /* enteredViaSpace= */ false,
                                StarterPackId.TABS));
        doReturn(true).when(mOmniboxActionDelegate).switchToTab(anyInt(), any());
        GURL url = new GURL("https://example.com");
        AutocompleteMatch match =
                new AutocompleteMatchBuilder()
                        .setHasTabMatch(true)
                        .setType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setAndroidTabId(123)
                        .setActions(
                                List.of(
                                        new OmniboxActionInSuggest(
                                                /* nativeInstance= */ 0,
                                                "hint",
                                                "acc",
                                                /* actionType= */ 1002,
                                                "",
                                                /* tabId= */ 123,
                                                /* presentationMode= */ 1)))
                        .build();

        mMediator.onSuggestionClicked(match, 0, url);

        verify(mOmniboxActionDelegate).switchToTab(eq(123), eq(url));
        verify(mAutocompleteDelegate, never()).loadUrl(any());
    }

    @Test
    @SmallTest
    public void onSuggestionClicked_doesNotOpenInNewTab() {
        mMediator.beginInput(createEmptySession());
        GURL url = JUnitTestGURLs.BLUE_1;

        mMediator.onSuggestionClicked(mSuggestionsList.get(0), 0, url);
        // Verify that the URL is not loaded in a new tab.
        verify(mAutocompleteDelegate).loadUrl(mOmniboxLoadUrlParamsCaptor.capture());
        assertEquals(mOmniboxLoadUrlParamsCaptor.getValue().url, url.getSpec());
        assertFalse(mOmniboxLoadUrlParamsCaptor.getValue().openInNewTab);

        // Verify the callback.
        mOmniboxLoadUrlParamsCaptor
                .getValue()
                .callback
                .onLoadUrl(
                        null,
                        new LoadUrlResult(Tab.TabLoadStatus.DEFAULT_PAGE_LOAD, mNavigationHandle));
        verify(mAutocompleteController)
                .createNavigationObserver(mNavigationHandle, mSuggestionsList.get(0));
    }

    @Test
    public void onSuggestionClicked_ClipboardImageSuggestion() {
        mMediator.beginInput(createEmptySession());
        var url = new GURL("http://test");
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.CLIPBOARD_IMAGE)
                        .build();

        // Verify that loadUrlWithPostData is called for the clipboard image suggestion.
        mMediator.onSuggestionClicked(match, 0, url);
        verify(mAutocompleteDelegate).loadUrl(mOmniboxLoadUrlParamsCaptor.capture());
        assertEquals(mOmniboxLoadUrlParamsCaptor.getValue().url, url.getSpec());

        // Verify the callback.
        mOmniboxLoadUrlParamsCaptor
                .getValue()
                .callback
                .onLoadUrl(
                        null,
                        new LoadUrlResult(Tab.TabLoadStatus.DEFAULT_PAGE_LOAD, mNavigationHandle));
        verify(mAutocompleteController).createNavigationObserver(mNavigationHandle, match);
    }

    @Test
    @SmallTest
    public void onSuggestionClicked_aimIsSentSuggestionText() {
        String suggestionText = "test suggestion";
        AutocompleteMatch match =
                new AutocompleteMatchBuilder().setDisplayText(suggestionText).build();
        var session = createSession(PAGE_URL, PAGE_TITLE, PageClassification.OTHER_VALUE);
        session.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);
        mMediator.beginInput(session);

        mMediator.onSuggestionClicked(match, /* matchIndex= */ 0, JUnitTestGURLs.RED_1);

        verify(mComposeboxQueryControllerBridge).getAimUrl(any(), eq(suggestionText), any());
        verifyNoMoreInteractions(mAutocompleteDelegate);
    }

    @Test
    @SmallTest
    public void setLayoutDirection_beforeInitialization() {
        mMediator.beginInput(createEmptySession());
        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), /* isFinal= */ true);
        assertEquals(mSuggestionsList.size(), mSuggestionModels.size());
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            assertEquals(
                    i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels
                            .get(i)
                            .model
                            .get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @Test
    @SmallTest
    public void setLayoutDirection_afterInitialization() {
        mMediator.beginInput(createEmptySession());
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), /* isFinal= */ true);
        assertEquals(mSuggestionsList.size(), mSuggestionModels.size());

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            assertEquals(
                    i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels
                            .get(i)
                            .model
                            .get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        }

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            assertEquals(
                    i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_LTR,
                    mSuggestionModels
                            .get(i)
                            .model
                            .get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @Test
    public void onSuggestionDropdownHeightChanged_noCallsUntilSessionStarted() {
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        verifyNoMoreInteractions(mAutocompleteController);
    }

    @Test
    public void onSuggestionDropdownHeightChanged_updatedHeightPassedToNative() {
        mMediator.beginInput(createEmptySession());

        var res = ContextUtils.getApplicationContext().getResources();
        int suggestionHeight = res.getDimensionPixelSize(R.dimen.omnibox_suggestion_content_height);
        float displayDensity = res.getDisplayMetrics().density;

        mMediator.onSuggestionDropdownHeightChanged(100);

        verify(mAutocompleteController)
                .onSuggestionDropdownHeightChanged((int) (100 * displayDensity), suggestionHeight);
    }

    @Test
    @SmallTest
    public void setSessionState_triggersZeroSuggest() {
        // This scenario is true for the LFF devices with precision pointer
        // device attached.
        // Here we don't clear the URL in the omnibox, but still require the
        // Autocomplete to issue the zero prefix suggest request.

        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);
        session.getAutocompleteInput().setUserText(url.getSpec()).setInitialUserText(url.getSpec());

        mMediator.beginInput(session);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyAutocompleteStartZeroSuggest(url.getSpec(), url, pageClassification, title);
    }

    @Test
    @SmallTest
    public void setSessionState_trackSessionState() {

        assertFalse(mMediator.isOmniboxSessionActiveForTesting());

        mMediator.beginInput(createEmptySession());
        assertTrue(mMediator.isOmniboxSessionActiveForTesting());

        mMediator.endInput();
        assertFalse(mMediator.isOmniboxSessionActiveForTesting());
    }

    /**
     * Verify the values recorded by SuggestionList.RequestToUiModel.* histograms.
     *
     * @param firstHistogramTotalCount total number of recorded values for the
     *     RequestToUiModel.First histogram
     * @param firstHistogramTime the value to expect to be recorded as RequestToUiModel.First, or
     *     null if this histogram should not be recorded
     * @param lastHistogramTotalCount total number of recorded values for the RequestToUiModel.Last
     *     histogram
     * @param lastHistogramTime the value to expect to be recorded as RequestToUiModel.Last, or null
     *     if this histogram should not be recorded
     */
    private void verifySuggestionRequestToUiModelHistograms(
            int firstHistogramTotalCount,
            @Nullable Integer firstHistogramTime,
            int lastHistogramTotalCount,
            @Nullable Integer lastHistogramTime) {
        assertEquals(
                firstHistogramTotalCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_FIRST));
        assertEquals(
                lastHistogramTotalCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_LAST));

        if (firstHistogramTime != null) {
            assertEquals(
                    1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_FIRST,
                            firstHistogramTime));
        }

        if (lastHistogramTime != null) {
            assertEquals(
                    1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_LAST,
                            lastHistogramTime));
        }
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_recordedForZps() {

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);
        mMediator.beginInput(session);

        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report first results. Observe first results histogram reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(100));
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ false);
        verifySuggestionRequestToUiModelHistograms(1, 100, 0, null);

        // Report next results. Observe first results histogram not reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(300));
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ false);
        verifySuggestionRequestToUiModelHistograms(1, 100, 0, null);

        // Report last results. Observe two histograms reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(100));
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);
        verifySuggestionRequestToUiModelHistograms(1, 100, 1, 500);
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_notRecordedWhenCanceled_LastResult() {

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);
        mMediator.beginInput(session);

        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report first results. Observe first results histogram reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(10));
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ false);
        verifySuggestionRequestToUiModelHistograms(1, 10, 0, null);

        // Cancel the interaction.
        mMediator.endInput();

        // Report last results. Observe no final report.
        verifySuggestionRequestToUiModelHistograms(1, 10, 0, null);
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_notRecordedWhenCanceled_FirstResult() {

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);

        mMediator.beginInput(session);

        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Cancel the interaction.
        mMediator.endInput();

        // Report first results. Observe no report (no focus).
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ false);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report last results. Observe no final report (no focus).
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_recordsBothHistogramsWhenFirstResponseIsFinal() {

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);
        mMediator.beginInput(session);

        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report first result as final. Observe both metrics reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(150));
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);
        verifySuggestionRequestToUiModelHistograms(1, 150, 1, 150);
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_subsequentKeyStrokesReportTimeSinceLastKeystroke() {

        UnsyncedSuggestionsListAnimationDriver.setAnimationsDisabledForTesting(true);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);

        mMediator.beginInput(session);
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report first result as final. Observe both metrics reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(150));
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ false);
        verifySuggestionRequestToUiModelHistograms(1, 150, 0, null);

        // No change on key press. No unexpected recordings.
        // Need to run looper here to flush the pending operation.
        session.getAutocompleteInput().setUserText("a");
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verifySuggestionRequestToUiModelHistograms(1, 150, 0, null);

        // No change on key press. No unexpected recordings.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(100));
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);
        verifySuggestionRequestToUiModelHistograms(2, 100, 1, 100);
        UnsyncedSuggestionsListAnimationDriver.setAnimationsDisabledForTesting(false);
    }

    @Test
    @SmallTest
    public void touchDownForPrefetch_PrefetchHit() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                OmniboxMetrics.HISTOGRAM_SEARCH_PREFETCH_TOUCH_DOWN_PREFETCH_RESULT,
                                OmniboxMetrics.PrefetchResult.HIT)
                        .expectIntRecord(
                                OmniboxMetrics
                                        .HISTOGRAM_SEARCH_PREFETCH_NUM_PREFETCHES_STARTED_IN_OMNIBOX_SESSION,
                                1)
                        .build();
        setSuggestionNativeObjectRef();
        // Simulate omnibox session start, and offer suggestions.
        mMediator.beginInput(createEmptySession());
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);

        // Simulate a suggestion being touched down.
        mMediator.onSuggestionTouchDown(mSuggestionsList.get(0), /* matchIndex= */ 0);

        // Ensure that no extra signals are sent to native.
        verify(mAutocompleteController, times(1))
                .onSuggestionTouchDown(null, mSuggestionsList.get(0), 0);

        // Simulate a navigation to the suggestion that was prefetched. This causes metrics about
        // prefetch to be recorded.
        mMediator.onSuggestionClicked(
                mSuggestionsList.get(0), /* matchIndex= */ 0, JUnitTestGURLs.URL_1);

        // Ends the omnibox session to reset state of touch down prefetch, and record metrics.
        mMediator.endInput();

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void touchDownForPrefetch_PrefetchMiss() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                OmniboxMetrics.HISTOGRAM_SEARCH_PREFETCH_TOUCH_DOWN_PREFETCH_RESULT,
                                OmniboxMetrics.PrefetchResult.MISS)
                        .expectIntRecord(
                                OmniboxMetrics
                                        .HISTOGRAM_SEARCH_PREFETCH_NUM_PREFETCHES_STARTED_IN_OMNIBOX_SESSION,
                                1)
                        .build();
        setSuggestionNativeObjectRef();
        // Simulate omnibox session start, and offer suggestions.
        mMediator.beginInput(createEmptySession());
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);

        // Simulate a suggestion being touched down.
        mMediator.onSuggestionTouchDown(mSuggestionsList.get(0), /* matchIndex= */ 0);

        // Ensure that no extra signals are sent to native.
        verify(mAutocompleteController, times(1))
                .onSuggestionTouchDown(null, mSuggestionsList.get(0), 0);

        // Simulate a navigation to a suggestion that was not prefetched. This causes metrics about
        // prefetch to be recorded.
        mMediator.onSuggestionClicked(
                mSuggestionsList.get(1), /* matchIndex= */ 1, JUnitTestGURLs.URL_1);

        // Ends the omnibox session to reset state of touch down prefetch, and record metrics.
        mMediator.endInput();

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void touchDownForPrefetch_NoPrefetch() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                OmniboxMetrics.HISTOGRAM_SEARCH_PREFETCH_TOUCH_DOWN_PREFETCH_RESULT,
                                OmniboxMetrics.PrefetchResult.NO_PREFETCH)
                        .expectIntRecord(
                                OmniboxMetrics
                                        .HISTOGRAM_SEARCH_PREFETCH_NUM_PREFETCHES_STARTED_IN_OMNIBOX_SESSION,
                                0)
                        .build();
        setSuggestionNativeObjectRef();
        // Simulate omnibox session start, and offer suggestions.
        mMediator.beginInput(createEmptySession());
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);

        // This will simulate the touch down trigger not starting a prefetch.
        when(mAutocompleteController.onSuggestionTouchDown(any(), any(), anyInt()))
                .thenReturn(false);

        // Simulate a suggestion being touched down.
        mMediator.onSuggestionTouchDown(mSuggestionsList.get(0), /* matchIndex= */ 0);

        // Ensure that no extra signals are sent to native.
        verify(mAutocompleteController, times(1))
                .onSuggestionTouchDown(null, mSuggestionsList.get(0), 0);

        // Simulate a navigation to the suggestion that was not prefetched. This causes metrics
        // about prefetch to be recorded.
        mMediator.onSuggestionClicked(
                mSuggestionsList.get(0), /* matchIndex= */ 0, JUnitTestGURLs.URL_1);

        // Ends the omnibox session to reset state of touch down prefetch, and record metrics.
        mMediator.endInput();

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void touchDownForPrefetch_LimitNumPrefetches() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                OmniboxMetrics
                                        .HISTOGRAM_SEARCH_PREFETCH_NUM_PREFETCHES_STARTED_IN_OMNIBOX_SESSION,
                                OmniboxFeatures.DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION)
                        .expectIntRecord(
                                OmniboxMetrics
                                        .HISTOGRAM_SEARCH_PREFETCH_NUM_PREFETCHES_STARTED_IN_OMNIBOX_SESSION,
                                1)
                        .build();
        setSuggestionNativeObjectRef();
        // Simulate omnibox session start.
        mMediator.beginInput(createEmptySession());

        // Triggeer one touch down event the maximum allowed. The extra event should not be sent to
        // native.
        int numTouchDownEvents = OmniboxFeatures.DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION + 1;
        assertTrue(numTouchDownEvents < mSuggestionsList.size());
        for (int i = 0; i < numTouchDownEvents; i++) {
            mMediator.onSuggestionTouchDown(mSuggestionsList.get(i), i);
        }

        // Ensure that no extra signals are sent to native.
        verify(
                        mAutocompleteController,
                        times(OmniboxFeatures.DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION))
                .onSuggestionTouchDown(any(), any(), anyInt());

        // Ends the omnibox session to reset state of touch down prefetch, and record metrics.
        mMediator.endInput();

        // Since the state is reset, new prefetches are allowed.
        // Simulate a new omnibox session start.
        mMediator.beginInput(createEmptySession());
        mMediator.onSuggestionTouchDown(mSuggestionsList.get(0), 0);
        verify(
                        mAutocompleteController,
                        times(OmniboxFeatures.DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION + 1))
                .onSuggestionTouchDown(any(), any(), anyInt());
        mMediator.endInput();

        histogramWatcher.assertExpected();
    }

    @Test
    public void onTopResumedActivityChanged_nonZeroSuggest() {

        GURL url = JUnitTestGURLs.BLUE_1;
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, url.getSpec(), pageClassification);
        mMediator.beginInput(session);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        session.getAutocompleteInput().setUserText("test");
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verifyAutocompleteStart(url, pageClassification, "test", 4, false);

        mMediator.onTopResumedActivityChanged(false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mAutocompleteController, never()).start(any(), any(), anyInt(), anyBoolean());

        session.getAutocompleteInput().setUserText("test");

        mMediator.onTopResumedActivityChanged(true);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verifyAutocompleteStart(url, pageClassification, "test", 4, false);
    }

    @Test
    public void onTopResumedActivityChanged_zeroSuggest() {

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(url, title, pageClassification);
        mMediator.beginInput(session);

        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);

        mMediator.onTopResumedActivityChanged(false);
        verify(mAutocompleteController, never()).startZeroSuggest(any(), any());

        mMediator.onTopResumedActivityChanged(true);
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
    }

    @Test
    public void onTextChanged_cachedZpsEligibleOnSelectPageClasses() {
        Set<Integer> eligibleClasses =
                Set.of(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE);

        var session = createSession(PAGE_URL, PAGE_TITLE, 0);
        mMediator.beginInput(session);

        for (var pageClass : PageClassification.values()) {
            session.getAutocompleteInput().setPageClassification(pageClass.getNumber());
            mMediator.serveCachedZeroSuggest(session.getAutocompleteInput());

            // Should only be invoked if page class is eligible.
            int numTimesInvoked = eligibleClasses.contains(pageClass.getNumber()) ? 1 : 0;
            verify(mMockCachedZeroSuggestionsManager, times(numTimesInvoked))
                    .readFromCache(anyInt());
            verify(mMockCachedZeroSuggestionsManager, never()).saveToCache(anyInt(), any());

            clearInvocations(mMockCachedZeroSuggestionsManager);
        }
    }

    @Test
    public void onTextChanged_cachedZpsNotInvokedInTypedContext() {

        var session = createSession(PAGE_URL, PAGE_TITLE, 0);
        mMediator.beginInput(session);

        for (var pageClass : PageClassification.values()) {
            session.getAutocompleteInput().setPageClassification(pageClass.getNumber());

            session.getAutocompleteInput().setUserText("text");

            // Should only be invoked if page class is eligible.
            verify(mMockCachedZeroSuggestionsManager, never()).readFromCache(anyInt());
            verify(mMockCachedZeroSuggestionsManager, never()).saveToCache(anyInt(), any());

            clearInvocations(mMockCachedZeroSuggestionsManager);
        }
    }

    @Test
    public void onTextChanged_cacheZpsFromEligiblePageClasses() {
        Set<Integer> eligibleClasses =
                Set.of(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE);

        mMediator.beginInput(createEmptySession());
        doReturn(false).when(mAutocompleteResult).isFromCachedResult();

        for (var pageClass : PageClassification.values()) {
            mMediator.getAutocompleteInputForTesting().setPageClassification(pageClass.getNumber());

            mMediator.onSuggestionsReceived(mAutocompleteResult, true);

            // Should only be invoked if page class is eligible.
            int numTimesInvoked = eligibleClasses.contains(pageClass.getNumber()) ? 1 : 0;
            verify(mMockCachedZeroSuggestionsManager, times(numTimesInvoked))
                    .saveToCache(anyInt(), any());

            clearInvocations(mMockCachedZeroSuggestionsManager);
        }
    }

    @Test
    public void onTextChanged_dontCacheTypedSuggestions() {

        for (var pageClass : PageClassification.values()) {
            var session = createSession(PAGE_URL, PAGE_TITLE, pageClass.getNumber());
            mMediator.beginInput(session);
            session.getAutocompleteInput().setUserText("x");
            verify(mMockCachedZeroSuggestionsManager, never()).saveToCache(anyInt(), any());
            clearInvocations(mMockCachedZeroSuggestionsManager);
        }
    }

    @Test
    public void onTextChanged_dontCacheCachedSuggestions() {

        for (var pageClass : PageClassification.values()) {
            var session = createSession(PAGE_URL, PAGE_TITLE, pageClass.getNumber());
            mMediator.beginInput(session);
            // Force an update as "" -> "" is not an observable change.
            mMediator.onInputChanged();
            verify(mMockCachedZeroSuggestionsManager, never()).saveToCache(anyInt(), any());
            clearInvocations(mMockCachedZeroSuggestionsManager);
        }
    }

    @Test
    public void updateVisualsForState_informsVisualStateObserver() {
        mMediator.updateVisualsForState(BrandedColorScheme.LIGHT_BRANDED_THEME);
        verify(mVisualStateObserver)
                .onOmniboxSuggestionsBackgroundColorChanged(
                        eq(
                                OmniboxResourceProvider.getSuggestionsDropdownBackgroundColor(
                                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME)));

        mMediator.updateVisualsForState(BrandedColorScheme.INCOGNITO);
        verify(mVisualStateObserver)
                .onOmniboxSuggestionsBackgroundColorChanged(
                        eq(
                                OmniboxResourceProvider.getSuggestionsDropdownBackgroundColor(
                                        mContext, BrandedColorScheme.INCOGNITO)));
    }

    @Test
    public void propagateOmniboxSessionStateChange_informsVisualStateObserver() {
        setUpLocationBarDataProvider(
                new GURL("https://abc.xyz"), "title", PageClassification.ANDROID_HUB_VALUE);
        mMediator.beginInput(createEmptySession());

        mMediator.propagateOmniboxSessionStateChange(true);
        verify(mVisualStateObserver, atLeastOnce()).onOmniboxSessionStateChange(true);

        mMediator.endInput();

        mMediator.propagateOmniboxSessionStateChange(false);
        verify(mVisualStateObserver, atLeastOnce()).onOmniboxSessionStateChange(false);
    }

    @Test
    public void propagateOmniboxSessionStateChange_hubSearchContainerVisible() {
        var session =
                createSession(
                        new GURL("https://abc.xyz"), "title", PageClassification.ANDROID_HUB_VALUE);

        mMediator.beginInput(session);
        assertTrue(mListModel.get(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE));

        mMediator.endInput();

        var session2 =
                createSession(new GURL("https://abc.xyz"), "title", PageClassification.BLANK_VALUE);
        mMediator.beginInput(session2);
        assertFalse(mListModel.get(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE));
    }

    @Test
    public void onTopResumedActivityChanged_hubSearchContainerVisible() {
        var session =
                createSession(
                        new GURL("https://abc.xyz"), "title", PageClassification.ANDROID_HUB_VALUE);

        mMediator.beginInput(session);
        mMediator.onTopResumedActivityChanged(true);
        assertTrue(mListModel.get(SuggestionListProperties.ACTIVITY_WINDOW_FOCUSED));

        mMediator.onTopResumedActivityChanged(false);
        assertTrue(mListModel.get(SuggestionListProperties.ACTIVITY_WINDOW_FOCUSED));
    }

    @Test
    @SmallTest
    public void setSessionState_attachesImeCallback() {
        mMediator.beginInput(createEmptySession());
        verify(mDeferredImeCallback).attach(mWindowAndroid);

        mMediator.endInput();
        verify(mDeferredImeCallback).detach();
    }

    @Test
    @SmallTest
    public void testDefaultBrowserPromo_notShownWithIncorrectSuggestionType() {
        var url = new GURL("http://test");

        for (@OmniboxSuggestionType int type = 0; type < OmniboxSuggestionType.NUM_TYPES; type++) {
            if (type != OmniboxSuggestionType.CLIPBOARD_URL) {
                var match = AutocompleteMatchBuilder.searchWithType(type).build();
                mMediator.onSuggestionClicked(match, 0, url);
                verify(
                                mAutocompleteDelegate,
                                never().description(
                                                "Default browser prompt should not show on"
                                                        + " OmniboxSuggestionType#"
                                                        + type))
                        .maybeShowDefaultBrowserPromo();
            }
        }
    }

    @Test
    @SmallTest
    public void testDefaultBrowserPromo_clipboardUrl() {
        mMediator.beginInput(createEmptySession());

        var url = new GURL("http://test");
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.CLIPBOARD_URL)
                        .build();
        mMediator.onSuggestionClicked(match, 0, url);

        verify(mAutocompleteDelegate).maybeShowDefaultBrowserPromo();
    }

    @Test
    @SmallTest
    public void testDefaultBrowserPromo_pastedUrl() {
        mMediator.beginInput(createEmptySession());
        var url = new GURL("http://test");
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.URL_WHAT_YOU_TYPED)
                        .build();

        when(mTextStateProvider.wasLastEditPaste()).thenReturn(false);
        mMediator.onSuggestionClicked(match, 0, url);
        verify(mAutocompleteDelegate, never()).maybeShowDefaultBrowserPromo();

        when(mTextStateProvider.wasLastEditPaste()).thenReturn(true);
        mMediator.onSuggestionClicked(match, 0, url);
        verify(mAutocompleteDelegate).maybeShowDefaultBrowserPromo();
    }

    @Test
    @SmallTest
    public void loadTypedOmniboxText_aimUrl() {
        var session = createEmptySession();
        var autocompleteInput = session.getAutocompleteInput();
        autocompleteInput
                .setUserText("test")
                .setPageClassification(
                        PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE)
                .setRequestType(AutocompleteRequestType.AI_MODE);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("test");
        mMediator.beginInput(session);
        GURL url = JUnitTestGURLs.BLUE_2;
        doAnswer(
                        invocation -> {
                            Callback<GURL> cb = invocation.getArgument(2);
                            cb.onResult(url);
                            return null;
                        })
                .when(mComposeboxQueryControllerBridge)
                .getAimUrl(any(), any(), any());

        AutocompleteMatch defaultMatch =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("test suggestion")
                        .setInlineAutocompletion("")
                        .setAllowedToBeDefaultMatch(true)
                        .setUrl(JUnitTestGURLs.GOOGLE_URL)
                        .build();
        mSuggestionsList.add(0, defaultMatch);
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(mSuggestionsList, null), true);

        mMediator.loadTypedOmniboxText(
                123L, /* openInNewTab= */ false, /* openInNewWindow= */ false);

        verify(mAutocompleteDelegate).loadUrl(mOmniboxLoadUrlParamsCaptor.capture());
        assertEquals(mOmniboxLoadUrlParamsCaptor.getValue().url, url.getSpec());
    }

    @Test
    @SmallTest
    public void loadTypedOmniboxText_imageGenerationUrl() {
        var session = createEmptySession();
        var autocompleteInput = session.getAutocompleteInput();
        autocompleteInput
                .setUserText("test")
                .setPageClassification(
                        PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE)
                .setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        setUpLocationBarDataProvider(
                JUnitTestGURLs.NTP_URL,
                "New Tab Page",
                PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("test");
        mMediator.beginInput(session);
        GURL url2 = JUnitTestGURLs.BLUE_2;
        doAnswer(
                        invocation -> {
                            Callback<GURL> cb = invocation.getArgument(2);
                            cb.onResult(url2);
                            return null;
                        })
                .when(mComposeboxQueryControllerBridge)
                .getImageGenerationUrl(any(), any(), any());

        AutocompleteMatch defaultMatch =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("test suggestion")
                        .setInlineAutocompletion("")
                        .setAllowedToBeDefaultMatch(true)
                        .setUrl(JUnitTestGURLs.GOOGLE_URL)
                        .build();
        mSuggestionsList.add(0, defaultMatch);
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(mSuggestionsList, null), true);

        mMediator.loadTypedOmniboxText(
                123L, /* openInNewTab= */ false, /* openInNewWindow= */ false);

        verify(mAutocompleteDelegate).loadUrl(mOmniboxLoadUrlParamsCaptor.capture());
        assertEquals(mOmniboxLoadUrlParamsCaptor.getValue().url, url2.getSpec());
    }

    @Test
    @SmallTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":disable_zero_suggest/true")
    public void
            onTextChanged_cachedZpsNotInvoked_whenOmniboxAutofocusOnIncognitoNtpAllowed_withoutZeroSuggest() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                OmniboxMetrics.HISTOGRAM_ZERO_SUGGEST_SUPPRESSED_ON_INCOGNITO_NTP,
                                true,
                                1)
                        .build();

        NewTabPageDelegate ntpDelegate = mock(NewTabPageDelegate.class);
        doReturn(ntpDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        var session =
                createSession(
                        new GURL("https://abc.xyz"),
                        "title",
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE);

        // Cached suggestions should be suppressed when on an Incognito NTP with autofocus enabled
        // and zero suggest disabled.
        doReturn(true).when(ntpDelegate).isIncognitoNewTabPageCurrentlyVisible();

        mMediator.beginInput(session);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mMockCachedZeroSuggestionsManager, never()).readFromCache(anyInt());

        // Histogram should be recorded once.
        histogramWatcher.assertExpected();

        // When not on an Incognito NTP, cached suggestions should be shown.
        doReturn(false).when(ntpDelegate).isIncognitoNewTabPageCurrentlyVisible();
        // Force an update as "" -> "" is not an observable change.
        mMediator.onInputChanged();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mMockCachedZeroSuggestionsManager, times(1)).readFromCache(anyInt());

        // Histogram record count should not be increased.
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void fuseboxStateChanges() {
        doReturn(true).when(mEmbedder).isTablet();
        mMediator.beginInput(createEmptySession());
        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(mListModel.get(SuggestionListProperties.ROUND_TOP_CORNERS));
        assertFalse(mListModel.get(SuggestionListProperties.DRAW_OVER_ANCHOR));

        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        mFuseboxStateSupplier.set(FuseboxState.DISABLED);
        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(mListModel.get(SuggestionListProperties.ROUND_TOP_CORNERS));
        assertTrue(mListModel.get(SuggestionListProperties.DRAW_OVER_ANCHOR));
    }

    @Test
    @SmallTest
    public void roundSidesPropagatedToModels_popoverLayoutModeTransitions() {
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        mMediator.beginInput(createEmptySession());
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(mSuggestionsList, null), true);

        verifySuggestionModelsRoundSides(RoundSides.BOTTOM_ONLY);

        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        verifySuggestionModelsRoundSides(RoundSides.NONE);

        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        verifySuggestionModelsRoundSides(RoundSides.BOTTOM_ONLY);
    }

    @Test
    @SmallTest
    public void roundSidesPropagatedToModels_toolbarLayoutModeTransitions() {
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.TOOLBAR);
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        mMediator.beginInput(createEmptySession());
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(mSuggestionsList, null), true);

        verifySuggestionModelsRoundSides(RoundSides.TOP_AND_BOTTOM);

        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        verifySuggestionModelsRoundSides(RoundSides.TOP_AND_BOTTOM);
    }

    @Test
    @SmallTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":disable_zero_suggest/false")
    public void
            onTextChanged_cachedZpsShown_whenOmniboxAutofocusOnIncognitoNtpAllowed_withZeroSuggest() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                OmniboxMetrics.HISTOGRAM_ZERO_SUGGEST_SUPPRESSED_ON_INCOGNITO_NTP)
                        .build();

        NewTabPageDelegate ntpDelegate = mock(NewTabPageDelegate.class);
        doReturn(ntpDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();

        var session =
                createSession(
                        new GURL("https://abc.xyz"),
                        "title",
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE);

        mMediator.beginInput(session);
        verify(mMockCachedZeroSuggestionsManager).readFromCache(anyInt());

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void onKeywordModeEntered_setsSiteSearchData() {
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(JUnitTestGURLs.BLUE_1, "Title", pageClassification);
        mMediator.beginInput(session);

        verify(mOmniboxActionDelegate)
                .setOnKeywordModeEnteredCb(mKeywordModeEnteredCaptor.capture());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        clearInvocations(mAutocompleteController);

        SiteSearchData data = new SiteSearchData("keyword", "Full Name");
        mKeywordModeEnteredCaptor.getValue().accept(data);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTextStateProvider).setSiteSearchChip("Full Name");
        assertEquals("", session.getAutocompleteInput().getUserText());
        assertEquals(data, session.getAutocompleteInput().getSiteSearchData());

        verify(mAutocompleteController).start(any(), any(), anyInt(), anyBoolean());
    }

    @Test
    @SmallTest
    public void onKeywordModeEntered_previewDoesNotTriggerAutocomplete() {
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(JUnitTestGURLs.BLUE_1, "Title", pageClassification);
        session.getAutocompleteInput().setUserText("original text");
        mMediator.beginInput(session);

        verify(mOmniboxActionDelegate)
                .setOnKeywordModeEnteredCb(mKeywordModeEnteredCaptor.capture());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        clearInvocations(mAutocompleteController);
        clearInvocations(mTextStateProvider);
        clearInvocations(mAutocompleteDelegate);

        // Enter preview
        SiteSearchData data =
                new SiteSearchData("keyword", "Full Name", /* enteredViaSpace= */ false);
        mMediator.allowPendingItemSelection();
        mKeywordModeEnteredCaptor.getValue().accept(data);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTextStateProvider).setSiteSearchChip("Full Name");
        assertEquals("original text", session.getAutocompleteInput().getUserText());
        assertEquals("", session.getAutocompleteInput().getPreviewText());
        assertTrue(session.getAutocompleteInput().hasPreviewText());
        verify(mAutocompleteDelegate).setOmniboxEditingText("");

        verify(mAutocompleteController, never()).start(any(), any(), anyInt(), anyBoolean());

        // Exit preview
        clearInvocations(mAutocompleteController);
        clearInvocations(mTextStateProvider);
        clearInvocations(mAutocompleteDelegate);

        mKeywordModeEnteredCaptor.getValue().accept(null);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertEquals("original text", session.getAutocompleteInput().getUserText());
        assertEquals("original text", session.getAutocompleteInput().getPreviewText());
        assertFalse(session.getAutocompleteInput().hasPreviewText());
        verify(mAutocompleteDelegate).setOmniboxEditingText("original text");

        verify(mAutocompleteController, never()).start(any(), any(), anyInt(), anyBoolean());
    }

    @Test
    @SmallTest
    public void onKeywordModeEntered_nullDoesNotClearText() {
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(JUnitTestGURLs.BLUE_1, "Title", pageClassification);
        session.getAutocompleteInput().setUserText("b");
        session.getAutocompleteInput().setSiteSearchData(new SiteSearchData("keyword", "label"));
        mMediator.beginInput(session);

        verify(mOmniboxActionDelegate)
                .setOnKeywordModeEnteredCb(mKeywordModeEnteredCaptor.capture());

        mMediator.allowPendingItemSelection();
        mKeywordModeEnteredCaptor.getValue().accept(null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTextStateProvider).setSiteSearchChip(null);
        assertEquals("b", session.getAutocompleteInput().getUserText());
    }

    @Test
    @SmallTest
    public void onRefineSuggestion_stripsKeyword() {
        int pageClassification = PageClassification.BLANK_VALUE;
        var session = createSession(JUnitTestGURLs.BLUE_1, "Title", pageClassification);
        SiteSearchData data = new SiteSearchData("keyword", "Full Name");
        session.getAutocompleteInput().setSiteSearchData(data);
        mMediator.beginInput(session);

        AutocompleteMatch match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("keyword query")
                        .setFillIntoEdit("keyword query")
                        .build();

        mMediator.onRefineSuggestion(match);

        verify(mAutocompleteDelegate).setOmniboxEditingText("query ");
    }

    @Test
    public void setOmniboxEditingText_doesNotCommitUserText() {
        mMediator.onNativeInitialized();
        var session = createEmptySession();
        session.getAutocompleteInput().setUserText("user text");
        mMediator.beginInput(session);

        mMediator.allowPendingItemSelection();
        mMediator.setOmniboxEditingText("suggestion text");

        // Verify the source of truth (AutocompleteInput) is NOT updated.
        assertEquals("user text", session.getAutocompleteInput().getUserText());
        // Verify UI is updated with the suggestion text.
        verify(mAutocompleteDelegate).setOmniboxEditingText("suggestion text");
    }

    @Test
    public void setOmniboxEditingText_stripsKeywordWithoutCommittingUserText() {
        mMediator.onNativeInitialized();
        var session = createEmptySession();
        session.getAutocompleteInput().setUserText("u");
        session.getAutocompleteInput().setSiteSearchData(new SiteSearchData("keyword", "label"));
        mMediator.beginInput(session);

        mMediator.allowPendingItemSelection();
        mMediator.setOmniboxEditingText("keyword query");

        // Verify user text is NOT updated.
        assertEquals("u", session.getAutocompleteInput().getUserText());
        // Verify SiteSearchData is NOT cleared.
        assertNotNull(session.getAutocompleteInput().getSiteSearchData());
        // Verify UI is updated with stripped text.
        verify(mAutocompleteDelegate).setOmniboxEditingText("query");
    }

    @Test
    public void setOmniboxEditingText_preservesKeywordMode() {
        mMediator.onNativeInitialized();
        var session = createEmptySession();
        session.getAutocompleteInput().setUserText("user text");
        session.getAutocompleteInput().setSiteSearchData(new SiteSearchData("keyword", "label"));
        mMediator.beginInput(session);

        mMediator.allowPendingItemSelection();
        mMediator.setOmniboxEditingText("new text");

        assertEquals("user text", session.getAutocompleteInput().getUserText());
        assertNotNull(session.getAutocompleteInput().getSiteSearchData());
        verify(mAutocompleteDelegate).setOmniboxEditingText("new text");
    }

    @Test
    @SmallTest
    public void testExternallyDrivenFadeAnimation() {
        doReturn(true).when(mEmbedder).isTablet();
        var session = createEmptySession();
        mMediator.beginInput(session);
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        @Nullable Animator result = mMediator.setupSuggestionsListShowAnimation();
        UnsyncedSuggestionsListAnimationDriver animationDriver =
                (UnsyncedSuggestionsListAnimationDriver) mMediator.getAnimationDriverForTesting();
        assertFalse(animationDriver.isRunning());
    }

    @Test
    @SmallTest
    public void testUnsyncedAnimation_doesNotShowKeyboardInStandbyNoFocus() {
        doReturn(false).when(mEmbedder).isTablet();
        var session = createSession(AutocompleteRequestType.SEARCH);
        session.getAutocompleteInput()
                .setAutocompleteState(AutocompleteInput.AutocompleteState.STANDBY_NO_FOCUS);
        mMediator.beginInput(session);

        reset(mAutocompleteDelegate);
        mMediator.setupSuggestionsListShowAnimation();

        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(true), anyBoolean());
    }

    @Test
    @SmallTest
    public void testUnsyncedAnimation_showsKeyboardInEnabledState() {
        doReturn(false).when(mEmbedder).isTablet();
        var session = createSession(AutocompleteRequestType.SEARCH);
        session.getAutocompleteInput()
                .setAutocompleteState(AutocompleteInput.AutocompleteState.ENABLED);
        mMediator.beginInput(session);

        reset(mAutocompleteDelegate);
        mMediator.setupSuggestionsListShowAnimation();

        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(true), anyBoolean());
    }

    @Test
    public void onTopResumedActivityChanged_managesObservers() {
        var session = createEmptySession();
        mMediator.beginInput(session);

        // Initially installed by beginInput -> setAutocompleteController.
        verify(mAutocompleteController).addOnSuggestionsReceivedListener(mMediator);

        // Deactivate: should remove observers and stop autocomplete.
        clearInvocations(mAutocompleteController);
        mMediator.onTopResumedActivityChanged(false);
        verify(mAutocompleteController).stop(AutocompleteStopReason.CLOBBERED);
        verify(mAutocompleteController).removeOnSuggestionsReceivedListener(mMediator);

        // Re-activate: should install observers and trigger suggestions.
        clearInvocations(mAutocompleteController);
        mMediator.onTopResumedActivityChanged(true);
        verify(mAutocompleteController).addOnSuggestionsReceivedListener(mMediator);
        // This will trigger startZeroSuggest because it's a new tab page in setup.
        verify(mAutocompleteController).startZeroSuggest(any(), any());
    }

    @Test
    public void isInInputSession_ignoresWindowFocus() {
        var session = createEmptySession();
        mMediator.beginInput(session);

        assertTrue(mMediator.isInInputSession());

        mMediator.onTopResumedActivityChanged(false);
        // Previously this would return false. Now it should still be true.
        assertTrue(mMediator.isInInputSession());

        mMediator.onTopResumedActivityChanged(true);
        assertTrue(mMediator.isInInputSession());

        mMediator.endInput();
        assertFalse(mMediator.isInInputSession());
    }

    private void setUpSiteSearchSpaceTrigger(
            String keyword,
            String shortName,
            @Nullable String fullName,
            @Nullable String userQuery) {
        doReturn(true)
                .when(mPrefService)
                .getBoolean(AutocompleteMediator.KEYWORD_SPACE_TRIGGERING_ENABLED_PREF);
        doReturn(keyword + " " + userQuery).when(mTextStateProvider).getTextWithoutAutocomplete();
        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(keyword).when(mTemplateUrl).getKeyword();
        doReturn(shortName).when(mTemplateUrl).getShortName();
        doReturn(fullName).when(mTemplateUrlService).getFullNameFromTemplateUrl(keyword);
        doReturn(mTemplateUrl)
                .when(mAutocompleteController)
                .getTemplateUrlForText(keyword + " " + userQuery);
    }

    @Test
    @SmallTest
    public void triggerSiteSearchSpaceWithQuerySuccess() {
        // Setup: Start session and mock text state with valid keyword and query.
        var session = createEmptySession();
        mMediator.beginInput(session);
        String query = "abc";
        setUpSiteSearchSpaceTrigger(
                /* keyword= */ "test",
                /* shortName= */ "Test",
                /* fullName= */ "Ask Test",
                /* userQuery= */ query);

        assertTrue(mMediator.triggerSiteSearch(SiteSearchActivationSource.SPACE));

        // Verify that the query "abc" was set in the omnibox.
        verify(mAutocompleteDelegate).setOmniboxEditingText(query);

        var siteSearchData = session.getAutocompleteInput().getSiteSearchData();
        assertNotNull(siteSearchData);
        assertEquals("test", siteSearchData.keyword);
        assertEquals("Ask Test", siteSearchData.fullName);
    }

    @Test
    @SmallTest
    public void triggerSiteSearchSpaceWithQuerySuccess_fallbackToShortName() {
        // Setup: Start session and mock text state with valid keyword and query.
        var session = createEmptySession();
        mMediator.beginInput(session);
        String query = "abc";
        setUpSiteSearchSpaceTrigger(
                /* keyword= */ "test",
                /* shortName= */ "Test",
                /* fullName= */ null,
                /* userQuery= */ query);

        assertTrue(mMediator.triggerSiteSearch(SiteSearchActivationSource.SPACE));

        // Verify that the query "abc" was set in the omnibox.
        verify(mAutocompleteDelegate).setOmniboxEditingText(query);

        var siteSearchData = session.getAutocompleteInput().getSiteSearchData();
        assertNotNull(siteSearchData);
        assertEquals("test", siteSearchData.keyword);
        assertEquals("Test", siteSearchData.fullName); // Should fall back to ShortName "Test"
    }

    @Test
    @SmallTest
    public void triggerSiteSearch_ReTriggerSuccess() {
        var session = createEmptySession();
        mMediator.beginInput(session);

        // 1. First trigger: user types "@gemini "
        setUpSiteSearchSpaceTrigger("@gemini", "Gemini", "Gemini AI", "");
        assertTrue(mMediator.triggerSiteSearch(SiteSearchActivationSource.SPACE));
        assertNotNull(session.getAutocompleteInput().getSiteSearchData());

        // 2. Backspace: simulate clearing site search data (exit keyword mode)
        session.getAutocompleteInput().setSiteSearchData(null);
        doReturn("@gemini").when(mTextStateProvider).getTextWithoutAutocomplete();

        // 3. Second trigger: user deletes space then types space again -> "@gemini "
        doReturn("@gemini ").when(mTextStateProvider).getTextWithoutAutocomplete();
        doReturn(mTemplateUrl).when(mAutocompleteController).getTemplateUrlForText("@gemini ");

        assertTrue(mMediator.triggerSiteSearch(SiteSearchActivationSource.SPACE));
        assertNotNull(session.getAutocompleteInput().getSiteSearchData());
    }

    @Test
    @SmallTest
    public void triggerSiteSearch_NoOpsInAiMode() {
        FuseboxSessionState session = createEmptySession();
        mMediator.beginInput(session);
        session.getAutocompleteInput().setRequestType(AutocompleteRequestType.AI_MODE);

        setUpSiteSearchSpaceTrigger(
                /* keyword= */ "test",
                /* shortName= */ "Test",
                /* fullName= */ "Test Site",
                /* userQuery= */ "abc");

        assertFalse(mMediator.triggerSiteSearch(SiteSearchActivationSource.SPACE));
    }

    @Test
    @SmallTest
    public void onInputChanged_setsAllowParkingAtSentinelProperty_mobile() {
        var session = createEmptySession();
        mMediator.beginInput(session);

        var input = session.getAutocompleteInput();
        OmniboxCapabilities.setHasDesktopExperienceForTesting(false);

        // ZPS -- allow parking.
        input.setUserText("");
        mMediator.onInputChanged();
        assertTrue(mListModel.get(SuggestionListProperties.ALLOW_PARKING_AT_SENTINEL));

        // Prefixed -- allow parking.
        input.setUserText("test");
        mMediator.onInputChanged();
        assertTrue(mListModel.get(SuggestionListProperties.ALLOW_PARKING_AT_SENTINEL));
    }

    @Test
    @SmallTest
    public void onInputChanged_setsAllowParkingAtSentinelProperty_desktop() {
        var session = createEmptySession();
        mMediator.beginInput(session);

        var input = session.getAutocompleteInput();
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);

        // ZPS -- allow parking.
        input.setUserText("");
        mMediator.onInputChanged();
        assertTrue(mListModel.get(SuggestionListProperties.ALLOW_PARKING_AT_SENTINEL));

        // Prefixed -- Don't allow parking.
        input.setUserText("test");
        mMediator.onInputChanged();
        assertFalse(mListModel.get(SuggestionListProperties.ALLOW_PARKING_AT_SENTINEL));
    }

    @Test
    @SmallTest
    public void loadUrlForOmniboxMatch_modelPickerShown_conventional_loadsUrl() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        setUpSessionAndMatch(AutocompleteRequestType.SEARCH, OmniboxSuggestionType.SEARCH_SUGGEST);

        loadUrlForOmniboxMatch(JUnitTestGURLs.RED_1);

        verifyLoadUrl(JUnitTestGURLs.RED_1);
    }

    @Test
    @SmallTest
    public void
            loadUrlForOmniboxMatch_modelPickerShown_aimSearchWhatYouTyped_getAimUrlFromInputState() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        setUpSessionAndMatch(
                AutocompleteRequestType.AI_MODE, OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED);

        loadUrlForOmniboxMatch(JUnitTestGURLs.RED_1);

        verify(mComposeboxQueryControllerBridge)
                .getAimUrlFromInputState(any(), any(), mUrlCallbackCaptor.capture());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);

        mUrlCallbackCaptor.getValue().onResult(JUnitTestGURLs.BLUE_1);
        verifyLoadUrl(JUnitTestGURLs.BLUE_1);
    }

    @Test
    @SmallTest
    public void
            loadUrlForOmniboxMatch_modelPickerShown_aimUrlWhatYouTyped_getAimUrlFromInputState() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        setUpSessionAndMatch(
                AutocompleteRequestType.AI_MODE, OmniboxSuggestionType.URL_WHAT_YOU_TYPED);

        loadUrlForOmniboxMatch(JUnitTestGURLs.RED_1);

        verify(mComposeboxQueryControllerBridge)
                .getAimUrlFromInputState(any(), any(), mUrlCallbackCaptor.capture());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);

        mUrlCallbackCaptor.getValue().onResult(JUnitTestGURLs.BLUE_1);
        verifyLoadUrl(JUnitTestGURLs.BLUE_1);
    }

    @Test
    @SmallTest
    public void loadUrlForOmniboxMatch_modelPickerNotShown_aim_getAimUrl() {
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        setUpSessionAndMatch(AutocompleteRequestType.AI_MODE, OmniboxSuggestionType.SEARCH_SUGGEST);

        loadUrlForOmniboxMatch(JUnitTestGURLs.RED_1);

        verify(mComposeboxQueryControllerBridge)
                .getAimUrl(any(), any(), mUrlCallbackCaptor.capture());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);

        mUrlCallbackCaptor.getValue().onResult(JUnitTestGURLs.BLUE_1);
        verifyLoadUrl(JUnitTestGURLs.BLUE_1);
    }

    @Test
    @SmallTest
    public void loadUrlForOmniboxMatch_modelPickerNotShown_imageGeneration_getImageGenerationUrl() {
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        setUpSessionAndMatch(
                AutocompleteRequestType.IMAGE_GENERATION, OmniboxSuggestionType.SEARCH_SUGGEST);

        loadUrlForOmniboxMatch(JUnitTestGURLs.RED_1);

        verify(mComposeboxQueryControllerBridge)
                .getImageGenerationUrl(any(), any(), mUrlCallbackCaptor.capture());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);

        mUrlCallbackCaptor.getValue().onResult(JUnitTestGURLs.BLUE_1);
        verifyLoadUrl(JUnitTestGURLs.BLUE_1);
    }

    @Test
    @SmallTest
    public void loadUrlForOmniboxMatch_modelPickerNotShown_conventional_loadsUrl() {
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        setUpSessionAndMatch(AutocompleteRequestType.SEARCH, OmniboxSuggestionType.SEARCH_SUGGEST);

        loadUrlForOmniboxMatch(JUnitTestGURLs.RED_1);

        verifyLoadUrl(JUnitTestGURLs.RED_1);
    }

    @Test
    public void onNavigation_parkedAtSentinelInZeroPrefixState_clearsText() {
        var session = createEmptySession();
        session.getAutocompleteInput().setUserText("");
        mMediator.beginInput(session);
        RobolectricUtil.runAllBackgroundAndUi();

        mMediator.onSuggestionDropdownNavigation(true);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mAutocompleteDelegate).setOmniboxEditingText("");
    }

    @Test
    public void installAutocompleteObservers_failsWhenActivityNotFocused() {
        // Create a new mediator with activity focus set to false.
        doReturn(false).when(mActivity).hasWindowFocus();
        AutocompleteMediator mediator =
                new AutocompleteMediator(
                        mContext,
                        mAutocompleteDelegate,
                        mTextStateProvider,
                        mListModel,
                        new Handler(),
                        () -> mModalDialogManager,
                        null,
                        null,
                        mLocationBarDataProvider,
                        tabGroupId -> {},
                        url -> false,
                        mOmniboxActionDelegate,
                        mActivityLifecycleDispatcher,
                        mEmbedder,
                        mWindowAndroid,
                        mDeferredImeCallback,
                        mFuseboxCoordinator,
                        false);
        mediator.getDropdownItemViewInfoListBuilderForTest()
                .registerSuggestionProcessor(mMockProcessor);
        mediator.getDropdownItemViewInfoListBuilderForTest()
                .setHeaderProcessorForTest(mMockHeaderProcessor);

        var session = createEmptySession();
        mediator.beginInput(session);

        // Verify that observers are NOT installed because activity is not focused.
        verify(mAutocompleteController, never()).addOnSuggestionsReceivedListener(any());
    }

    @Test
    @SmallTest
    public void adjustGurlForRequestType_noInputSession_noUrlAdjustment() {
        mMediator.adjustGurlForRequestType(JUnitTestGURLs.BLUE_1, "query", mGurlCallback);

        verify(mGurlCallback).onResult(JUnitTestGURLs.BLUE_1);
        verifyNoInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    @SmallTest
    public void adjustGurlForRequestType_conventionalRequest_noUrlAdjustment() {
        mMediator.beginInput(createSession(AutocompleteRequestType.SEARCH));

        mMediator.adjustGurlForRequestType(JUnitTestGURLs.BLUE_1, "query", mGurlCallback);

        verify(mGurlCallback).onResult(JUnitTestGURLs.BLUE_1);
        verifyNoInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    @SmallTest
    public void adjustGurlForRequestType_modelPickerAIM_getAimUrlFromInputState() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        mMediator.beginInput(createSession(AutocompleteRequestType.AI_MODE));

        mMediator.adjustGurlForRequestType(JUnitTestGURLs.BLUE_1, "query", mGurlCallback);

        verify(mComposeboxQueryControllerBridge)
                .getAimUrlFromInputState(eq(JUnitTestGURLs.BLUE_1), eq("query"), eq(mGurlCallback));
    }

    @Test
    @SmallTest
    public void adjustGurlForRequestType_modelPickerImageGen_getAimUrlFromInputState() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        mMediator.beginInput(createSession(AutocompleteRequestType.IMAGE_GENERATION));

        mMediator.adjustGurlForRequestType(JUnitTestGURLs.BLUE_1, "query", mGurlCallback);

        verify(mComposeboxQueryControllerBridge)
                .getAimUrlFromInputState(eq(JUnitTestGURLs.BLUE_1), eq("query"), eq(mGurlCallback));
    }

    @Test
    @SmallTest
    public void adjustGurlForRequestType_modelPickerCanvas_getAimUrlFromInputState() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        mMediator.beginInput(createSession(AutocompleteRequestType.CANVAS));

        mMediator.adjustGurlForRequestType(JUnitTestGURLs.BLUE_1, "query", mGurlCallback);

        verify(mComposeboxQueryControllerBridge)
                .getAimUrlFromInputState(eq(JUnitTestGURLs.BLUE_1), eq("query"), eq(mGurlCallback));
    }

    @Test
    @SmallTest
    public void adjustGurlForRequestType_modelPickerDeepSearch_getAimUrlFromInputState() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        mMediator.beginInput(createSession(AutocompleteRequestType.DEEP_SEARCH));

        mMediator.adjustGurlForRequestType(JUnitTestGURLs.BLUE_1, "query", mGurlCallback);

        verify(mComposeboxQueryControllerBridge)
                .getAimUrlFromInputState(eq(JUnitTestGURLs.BLUE_1), eq("query"), eq(mGurlCallback));
    }

    @Test
    @SmallTest
    public void adjustGurlForRequestType_noModelPicker_getAimUrl() {
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        mMediator.beginInput(createSession(AutocompleteRequestType.AI_MODE));

        mMediator.adjustGurlForRequestType(JUnitTestGURLs.BLUE_1, "query", mGurlCallback);

        verify(mComposeboxQueryControllerBridge)
                .getAimUrl(eq(JUnitTestGURLs.BLUE_1), eq("query"), eq(mGurlCallback));
    }

    @Test
    @SmallTest
    public void adjustGurlForRequestType_noModelPicker_getImageGenerationUrl() {
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        mMediator.beginInput(createSession(AutocompleteRequestType.IMAGE_GENERATION));

        mMediator.adjustGurlForRequestType(JUnitTestGURLs.BLUE_1, "query", mGurlCallback);

        verify(mComposeboxQueryControllerBridge)
                .getImageGenerationUrl(eq(JUnitTestGURLs.BLUE_1), eq("query"), eq(mGurlCallback));
    }

    @Test
    @SmallTest
    public void adjustGurlForRequestType_noModelPickerDeepSearch_noUrlAdjustment() {
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        mMediator.beginInput(createSession(AutocompleteRequestType.DEEP_SEARCH));

        mMediator.adjustGurlForRequestType(JUnitTestGURLs.BLUE_1, "query", mGurlCallback);

        verify(mGurlCallback).onResult(JUnitTestGURLs.BLUE_1);
        verifyNoInteractions(mComposeboxQueryControllerBridge);
    }
}
