// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Handler;
import android.util.SparseArray;
import android.view.View;
import android.view.Window;

import androidx.annotation.Nullable;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowPausedSystemClock;

import org.chromium.base.ActivityState;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.DeferredIMEWindowInsetApplicationCallback;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteMediator.EditSessionState;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionFactoryImpl;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxAnswerAction;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionFactoryJni;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.Set;

/** Tests for {@link AutocompleteMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            AutocompleteMediatorUnitTest.ShadowCachedSuggestionsManager.class,
            ShadowLooper.class
        })
public class AutocompleteMediatorUnitTest {
    private static final int SUGGESTION_MIN_HEIGHT = 20;
    private static final int HEADER_MIN_HEIGHT = 15;
    private static final GURL PAGE_URL = new GURL("https://www.site.com/page.html");
    private static final String PAGE_TITLE = "Page Title";

    public @Rule JniMocker mJniMocker = new JniMocker();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock AutocompleteDelegate mAutocompleteDelegate;
    private @Mock UrlBarEditingTextStateProvider mTextStateProvider;
    private @Mock SuggestionProcessor mMockProcessor;
    private @Mock HeaderProcessor mMockHeaderProcessor;
    private @Mock AutocompleteController mAutocompleteController;
    private @Mock AutocompleteController.Natives mControllerJniMock;
    private @Mock LocationBarDataProvider mLocationBarDataProvider;
    private @Mock ModalDialogManager mModalDialogManager;
    private @Mock Profile mProfile;
    private @Mock Tab mTab;
    private @Mock TabModel mTabModel;
    private @Mock TabWindowManager mTabManager;
    private @Mock WindowAndroid mMockWindowAndroid;
    private @Mock OmniboxActionDelegate mOmniboxActionDelegate;
    private @Mock LargeIconBridge.Natives mLargeIconBridgeJniMock;
    private @Mock OmniboxActionFactoryJni mActionFactoryJni;
    private @Mock NavigationHandle mNavigationHandle;
    private @Mock ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private @Mock WindowAndroid mWindowAndroid;
    private @Mock Window mWindow;
    private @Mock View mDecorView;
    private @Mock OmniboxSuggestionsDropdownEmbedder mEmbedder;
    private @Mock InsetObserver mInsetObserver;
    private @Mock AutocompleteCoordinator.OmniboxSuggestionsVisualStateObserver
            mVisualStateObserver;
    private @Mock DeferredIMEWindowInsetApplicationCallback mDeferredImeCallback;
    private @Captor ArgumentCaptor<OmniboxLoadUrlParams> mOmniboxLoadUrlParamsCaptor;
    private @Captor ArgumentCaptor<SuggestionsListAnimationDriver> mDriverCaptor;

    private PropertyModel mListModel;
    private AutocompleteMediator mMediator;
    private List<AutocompleteMatch> mSuggestionsList;
    private AutocompleteResult mAutocompleteResult;
    private ModelList mSuggestionModels;
    private ObservableSupplierImpl<TabWindowManager> mTabWindowManagerSupplier;
    private Activity mActivity = Robolectric.buildActivity(Activity.class).setup().get();

    // Interface abstracting calls to CachedZeroSuggestionsManager, making interactions with that
    // more idiomatic.
    interface CachedZeroSuggestionsManagerCalls {
        void saveToCache(AutocompleteResult r);

        AutocompleteResult readFromCache();
    }

    // CachedZeroSuggestionsManager shadow that helps us intercept interactions with manager's
    // static methods.
    @Implements(CachedZeroSuggestionsManager.class)
    public static class ShadowCachedSuggestionsManager {
        public static CachedZeroSuggestionsManagerCalls mock =
                mock(CachedZeroSuggestionsManagerCalls.class);

        @Implementation
        public static void saveToCache(AutocompleteResult r) {
            mock.saveToCache(r);
        }

        @Implementation
        public static AutocompleteResult readFromCache() {
            return mock.readFromCache();
        }
    }

    @Before
    public void setUp() {
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeJniMock);
        mJniMocker.mock(OmniboxActionFactoryJni.TEST_HOOKS, mActionFactoryJni);
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mControllerJniMock);

        doReturn(mAutocompleteController).when(mControllerJniMock).getForProfile(any());

        mSuggestionModels = new ModelList();
        mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
        mListModel.set(SuggestionListProperties.SUGGESTION_MODELS, mSuggestionModels);

        mTabWindowManagerSupplier = new ObservableSupplierImpl<>();
        doReturn(mInsetObserver).when(mWindowAndroid).getInsetObserver();
        doReturn(mWindow).when(mWindowAndroid).getWindow();
        doReturn(mDecorView).when(mWindow).getDecorView();

        mMediator =
                new AutocompleteMediator(
                        mActivity,
                        mAutocompleteDelegate,
                        mTextStateProvider,
                        mListModel,
                        new Handler(),
                        () -> mModalDialogManager,
                        null,
                        null,
                        mLocationBarDataProvider,
                        tab -> {},
                        mTabWindowManagerSupplier,
                        url -> false,
                        mOmniboxActionDelegate,
                        mActivityLifecycleDispatcher,
                        mEmbedder,
                        mWindowAndroid,
                        mDeferredImeCallback);
        mMediator
                .getDropdownItemViewInfoListBuilderForTest()
                .registerSuggestionProcessor(mMockProcessor);
        mMediator
                .getDropdownItemViewInfoListBuilderForTest()
                .setHeaderProcessorForTest(mMockHeaderProcessor);

        doReturn(SUGGESTION_MIN_HEIGHT).when(mMockProcessor).getMinimumViewHeight();
        doReturn(true).when(mMockProcessor).doesProcessSuggestion(any(), anyInt());
        doAnswer((invocation) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS))
                .when(mMockProcessor)
                .createModel();
        doReturn(OmniboxSuggestionUiType.DEFAULT).when(mMockProcessor).getViewTypeId();

        doAnswer((invocation) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS))
                .when(mMockHeaderProcessor)
                .createModel();
        doReturn(HEADER_MIN_HEIGHT).when(mMockHeaderProcessor).getMinimumViewHeight();
        doReturn(OmniboxSuggestionUiType.HEADER).when(mMockHeaderProcessor).getViewTypeId();

        mSuggestionsList = buildSampleSuggestionsList(10, "Suggestion");
        mAutocompleteResult = spy(AutocompleteResult.fromCache(mSuggestionsList, null));
        doReturn(true).when(mAutocompleteDelegate).isKeyboardActive();
        setUpLocationBarDataProvider(
                JUnitTestGURLs.NTP_URL, "New Tab Page", PageClassification.NTP_VALUE);

        mMediator.setOmniboxSuggestionsVisualStateObserver(Optional.of(mVisualStateObserver));
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
     * Build a fake group headers map with elements named 'Header #', where '#' is the group header
     * index (1-based) and 'Header' is the supplied prefix. Each header has a corresponding key
     * computed as baseKey + #.
     *
     * @param count Number of group headers to build.
     * @param baseKey Key of the first group header.
     * @param prefix Name prefix for each group.
     * @return Map of group headers (populated in random order).
     */
    private SparseArray<String> buildSampleGroupHeaders(int count, int baseKey, String prefix) {
        SparseArray<String> headers = new SparseArray<>(count);
        for (int index = 0; index < count; index++) {
            headers.put(baseKey + index, prefix + " " + (index + 1));
        }

        return headers;
    }

    /**
     * Set up LocationBarDataProvider to report supplied values.
     *
     * @param url The URL to report as a current URL.
     * @param title The Page Title to report.
     * @param pageClassification The Page classification to report.
     */
    void setUpLocationBarDataProvider(GURL url, String title, int pageClassification) {
        when(mLocationBarDataProvider.hasTab()).thenReturn(true);
        when(mLocationBarDataProvider.getCurrentGurl()).thenReturn(url);
        when(mLocationBarDataProvider.getTitle()).thenReturn(title);
        when(mLocationBarDataProvider.getPageClassification(false)).thenReturn(pageClassification);
    }

    /** Sets the native object reference for all suggestions in mSuggestionList. */
    void setSuggestionNativeObjectRef() {
        for (int index = 0; index < mSuggestionsList.size(); index++) {
            mSuggestionsList.get(index).updateNativeObjectRef(index + 1);
        }
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_worksWithNullList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(null, null), /* isFinal= */ true);

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE));
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_worksWithEmptyList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(null, null), /* isFinal= */ true);

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE));
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_scrolEventsWithConcealedItemsTogglesKeyboardVisibility() {
        mMediator.onNativeInitialized();

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
        mMediator.onNativeInitialized();

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
        mMediator.onNativeInitialized();

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
    @SmallTest
    @EnableFeatures(OmniboxFeatureList.ANIMATE_SUGGESTIONS_LIST_APPEARANCE)
    public void onOmniboxSessionStateChange_startsAnimationDriver() {
        mListModel.set(SuggestionListProperties.ALPHA, 1.0f);
        SuggestionsListAnimationDriver animationDriver =
                mMediator.initializeAnimationDriver(mWindow);
        mMediator.onNativeInitialized();

        // Animation shouldn't run if IME insets are not yet controllable.
        mMediator.onOmniboxSessionStateChange(true);
        verify(mInsetObserver, never()).addWindowInsetsAnimationListener(animationDriver);
        mMediator.onOmniboxSessionStateChange(false);

        animationDriver.onControllableInsetsChanged(null, WindowInsetsCompat.Type.ime());
        // Animation can run now that IME insets are controllable.
        mMediator.onOmniboxSessionStateChange(true);
        verify(mInsetObserver).addWindowInsetsAnimationListener(animationDriver);

        mMediator.onOmniboxSessionStateChange(false);
        verify(mInsetObserver).removeWindowInsetsAnimationListener(animationDriver);
    }

    @Test
    @SmallTest
    public void onTextChanged_emptyTextTriggersZeroSuggest() {
        onTextChanged_textTriggersZeroSuggest(
                /* text= */ "", /* isOnFocusContext= */ false, /* expectIsOnFocusContext= */ false);
    }

    @Test
    @SmallTest
    public void onTextChanged_emptyTextTriggersZeroSuggestWithOnFocusContext() {
        onTextChanged_textTriggersZeroSuggest(
                /* text= */ "", /* isOnFocusContext= */ true, /* expectIsOnFocusContext= */ false);
    }

    @Test
    @SmallTest
    public void onTextChanged_nonEmptyTextTriggersZeroSuggestWithOnFocusContext() {
        onTextChanged_textTriggersZeroSuggest(
                /* text= */ "test",
                /* isOnFocusContext= */ true,
                /* expectIsOnFocusContext= */ true);
    }

    private void onTextChanged_textTriggersZeroSuggest(
            String text, boolean isOnFocusContext, boolean expectIsOnFocusContext) {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        mMediator.onOmniboxSessionStateChange(true);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn(text);

        mMediator.onNativeInitialized();
        mMediator.onTextChanged(text, isOnFocusContext);
        verify(mAutocompleteController)
                .startZeroSuggest(text, url, pageClassification, title, expectIsOnFocusContext);
    }

    @Test
    @SmallTest
    public void onTextChanged_nonEmptyTextTriggersSuggestions() {
        mMediator.setAutocompleteProfile(mProfile);

        GURL url = JUnitTestGURLs.BLUE_1;
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, url.getSpec(), pageClassification);
        mMediator.onOmniboxSessionStateChange(true);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("test", /* isOnFocusContext= */ false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mAutocompleteController).start(url, pageClassification, "test", 4, false);
    }

    @Test
    @SmallTest
    public void onTextChanged_cancelsPendingRequests() {
        mMediator.setAutocompleteProfile(mProfile);

        GURL url = JUnitTestGURLs.BLUE_1;
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, url.getSpec(), pageClassification);
        mMediator.onOmniboxSessionStateChange(true);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("test", /* isOnFocusContext= */ false);
        mMediator.onTextChanged("nottest", /* isOnFocusContext= */ false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mAutocompleteController, times(1))
                .start(url, pageClassification, "nottest", 4, false);
        verify(mAutocompleteController, times(1))
                .start(any(), anyInt(), any(), anyInt(), anyBoolean());
    }

    @Test
    @SmallTest
    public void onOmniboxSessionStateChange_onlyOneZeroSuggestRequestIsInvoked() {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Simulate URL being focus changes.
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onOmniboxSessionStateChange(false);
        mMediator.onOmniboxSessionStateChange(true);
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), anyInt(), any(), /* isOnFocusContext= */ eq(false));

        // Simulate native being initialized. Make sure we only ever issue one request, even if
        // there are multiple requests to activate the autocomplete session.
        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        mMediator.onOmniboxSessionStateChange(true);
        verify(mAutocompleteController, times(1))
                .startZeroSuggest(
                        "", url, pageClassification, title, /* isOnFocusContext= */ false);
    }

    @Test
    @SmallTest
    public void onOmniboxSessionStateChange_preventsZeroSuggestRequestOnDeactivation() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Simulate URL being focus changes.
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onOmniboxSessionStateChange(false);
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), anyInt(), any(), /* isOnFocusContext= */ eq(false));

        // Simulate native being inititalized. Make sure no suggest requests are sent.
        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), anyInt(), any(), /* isOnFocusContext= */ eq(false));
    }

    @Test
    @SmallTest
    public void onOmniboxSessionStateChange_textChangeCancelsOutstandingZeroSuggestRequest() {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("A");

        // Simulate URL being focus changes, and that user typed text and deleted it.
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onTextChanged("A", /* isOnFocusContext= */ false);
        mMediator.onTextChanged("", /* isOnFocusContext= */ false);
        mMediator.onTextChanged("A", /* isOnFocusContext= */ false);

        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never())
                .start(any(), anyInt(), any(), anyInt(), anyBoolean());
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), anyInt(), any(), /* isOnFocusContext= */ eq(false));

        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, times(1)).start(url, pageClassification, "A", 0, true);
        verify(mAutocompleteController, times(1))
                .start(any(), anyInt(), any(), anyInt(), anyBoolean());
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), anyInt(), any(), /* isOnFocusContext= */ eq(false));
    }

    @Test
    @SmallTest
    public void onOmniboxSessionStateChange_textChangeCancelsIntermediateZeroSuggestRequests() {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        mMediator.onOmniboxSessionStateChange(true);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Simulate URL being focus changes, and that user typed text and deleted it.
        mMediator.onTextChanged("A", /* isOnFocusContext= */ false);
        mMediator.onTextChanged("", /* isOnFocusContext= */ false);

        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never())
                .start(any(), anyInt(), any(), anyInt(), anyBoolean());
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), anyInt(), any(), /* isOnFocusContext= */ eq(false));

        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never())
                .start(any(), anyInt(), any(), anyInt(), anyBoolean());
        verify(mAutocompleteController, times(1))
                .startZeroSuggest(any(), any(), anyInt(), any(), /* isOnFocusContext= */ eq(false));
    }

    @Test
    @SmallTest
    public void onSuggestionsReceived_sendsOnSuggestionsChanged() {
        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(mSuggestionsList, null), true);
        verify(mAutocompleteDelegate).onSuggestionsChanged(any());

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
        verify(mAutocompleteDelegate).onSuggestionsChanged(defaultMatch);
    }

    @Test
    @SmallTest
    public void onSuggestionClicked_doesNotOpenInNewTab() {
        mMediator.setAutocompleteProfile(mProfile);
        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
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
        mMediator.setAutocompleteProfile(mProfile);
        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
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
    public void onSuggestionClicked_deferLoadingUntilNativeLibrariesLoaded() {
        var url = new GURL("http://test");
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();

        // Simulate interaction with match before native initialization completed.
        mMediator.onSuggestionClicked(match, 0, url);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyNoMoreInteractions(mAutocompleteDelegate);

        // Simulate native initialization complete, but still no profile.
        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyNoMoreInteractions(mAutocompleteDelegate);

        // Simulate profile loaded.
        mMediator.setAutocompleteProfile(mProfile);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mAutocompleteDelegate).loadUrl(mOmniboxLoadUrlParamsCaptor.capture());
        assertEquals(mOmniboxLoadUrlParamsCaptor.getValue().url, url.getSpec());
        assertFalse(mOmniboxLoadUrlParamsCaptor.getValue().openInNewTab);
        verify(mAutocompleteDelegate).clearOmniboxFocus();
        verifyNoMoreInteractions(mAutocompleteDelegate);

        // Verify the callback.
        mOmniboxLoadUrlParamsCaptor
                .getValue()
                .callback
                .onLoadUrl(
                        null,
                        new LoadUrlResult(Tab.TabLoadStatus.DEFAULT_PAGE_LOAD, mNavigationHandle));
        verify(mAutocompleteController).createNavigationObserver(mNavigationHandle, match);

        // Verify no reload on profile change.
        Profile newProfile = mock(Profile.class);
        mMediator.setAutocompleteProfile(newProfile);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyNoMoreInteractions(mAutocompleteDelegate);
    }

    @Test
    @SmallTest
    public void setLayoutDirection_beforeInitialization() {
        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), /* isFinal= */ true);
        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(
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
        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), /* isFinal= */ true);
        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(
                    i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels
                            .get(i)
                            .model
                            .get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        }

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(
                    i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_LTR,
                    mSuggestionModels
                            .get(i)
                            .model
                            .get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @Test
    public void onSuggestionDropdownHeightChanged_noNativeCallsUntilNativeIsReady() {
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        verifyNoMoreInteractions(mAutocompleteController);
    }

    @Test
    public void onSuggestionDropdownHeightChanged_noNativeCallsUntilProfileIsReady() {
        mMediator.onNativeInitialized();
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        verifyNoMoreInteractions(mAutocompleteController);
    }

    @Test
    public void onSuggestionDropdownHeightChanged_updatedHeightPassedToNative() {
        mMediator.onNativeInitialized();
        mMediator.setAutocompleteProfile(mProfile);

        var res = ContextUtils.getApplicationContext().getResources();
        int suggestionHeight = res.getDimensionPixelSize(R.dimen.omnibox_suggestion_content_height);
        float displayDensity = res.getDisplayMetrics().density;

        mMediator.onSuggestionDropdownHeightChanged(100);

        verify(mAutocompleteController)
                .onSuggestionDropdownHeightChanged((int) (100 * displayDensity), suggestionHeight);
    }

    @Test
    @SmallTest
    public void onOmniboxSessionStateChange_triggersZeroSuggest_nativeInitialized() {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn(url.getSpec());

        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController)
                .startZeroSuggest(
                        url.getSpec(),
                        url,
                        pageClassification,
                        title,
                        /* isOnFocusContext= */ false);
    }

    @Test
    @SmallTest
    public void onOmniboxSessionStateChange_triggersZeroSuggest_nativeNotInitialized() {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Signal focus prior to initializing native; confirm that zero suggest is not triggered.
        mMediator.onOmniboxSessionStateChange(true);
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), anyInt(), any(), /* isOnFocusContext= */ eq(false));

        // Initialize native and ensure zero suggest is triggered.
        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController)
                .startZeroSuggest(
                        "", url, pageClassification, title, /* isOnFocusContext= */ false);
    }

    @Test
    @SmallTest
    public void onTextChanged_editSessionActivatedByUserInput() {
        mMediator.setAutocompleteProfile(mProfile);

        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
        Assert.assertEquals(mMediator.getEditSessionStateForTest(), EditSessionState.INACTIVE);
        mMediator.onTextChanged("n", /* isOnFocusContext= */ false);
        Assert.assertEquals(
                mMediator.getEditSessionStateForTest(), EditSessionState.ACTIVATED_BY_USER_INPUT);

        mMediator.onOmniboxSessionStateChange(false);
        Assert.assertEquals(mMediator.getEditSessionStateForTest(), EditSessionState.INACTIVE);
    }

    @Test
    @SmallTest
    public void switchToTab_noTargetTab() {
        mMediator.setAutocompleteProfile(mProfile);

        // There is no Tab to switch to.
        doReturn(null).when(mAutocompleteController).getMatchingTabForSuggestion(any());
        Assert.assertFalse(mMediator.maybeSwitchToTab(null));
    }

    @Test
    @SmallTest
    public void switchToTab_noTabManager() {
        mMediator.setAutocompleteProfile(mProfile);

        // We have a tab, but no tab manager.
        doReturn(mTab).when(mAutocompleteController).getMatchingTabForSuggestion(any());
        Assert.assertFalse(mMediator.maybeSwitchToTab(null));
    }

    @Test
    @SmallTest
    public void switchToTab_tabAttachedToStoppedActivity() {
        mMediator.setAutocompleteProfile(mProfile);

        // We have a tab, and tab manager. The tab is part of the stopped activity.
        doReturn(mTab).when(mAutocompleteController).getMatchingTabForSuggestion(any());
        mTabWindowManagerSupplier.set(mTabManager);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.STOPPED).when(mMockWindowAndroid).getActivityState();
        Assert.assertTrue(mMediator.maybeSwitchToTab(null));
    }

    @Test
    @SmallTest
    public void switchToTab_noTabModelForTab() {
        mMediator.setAutocompleteProfile(mProfile);

        // We have a tab, and tab manager. The tab is part of the running activity.
        // The tab is not a part of the model though (eg. it has just been closed).
        // https://crbug.com/1300447
        doReturn(mTab).when(mAutocompleteController).getMatchingTabForSuggestion(any());
        mTabWindowManagerSupplier.set(mTabManager);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.RESUMED).when(mMockWindowAndroid).getActivityState();
        doReturn(null).when(mTabManager).getTabModelForTab(any());
        Assert.assertFalse(mMediator.maybeSwitchToTab(null));
    }

    @Test
    @SmallTest
    public void switchToTab_invalidTabModelAssociation() {
        mMediator.setAutocompleteProfile(mProfile);

        // We have a tab, and tab manager. The tab is part of the running activity.
        // The tab reports association with an existing model, but the model thinks otherwise.
        // https://crbug.com/1300447
        doReturn(mTab).when(mAutocompleteController).getMatchingTabForSuggestion(any());
        mTabWindowManagerSupplier.set(mTabManager);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.RESUMED).when(mMockWindowAndroid).getActivityState();
        doReturn(mTabModel).when(mTabManager).getTabModelForTab(any());

        // Make sure that this indeed returns no association.
        Assert.assertEquals(
                TabModel.INVALID_TAB_INDEX, TabModelUtils.getTabIndexById(mTabModel, mTab.getId()));
        Assert.assertFalse(mMediator.maybeSwitchToTab(null));
    }

    @Test
    @SmallTest
    public void switchToTab_validTabModelAssociation() {
        mMediator.setAutocompleteProfile(mProfile);

        // We have a tab, and tab manager. The tab is part of the running activity.
        // The tab reports association with an existing model; the model confirms this.
        doReturn(mTab).when(mAutocompleteController).getMatchingTabForSuggestion(any());
        mTabWindowManagerSupplier.set(mTabManager);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.RESUMED).when(mMockWindowAndroid).getActivityState();
        doReturn(mTabModel).when(mTabManager).getTabModelForTab(any());
        doReturn(1).when(mTabModel).getCount();
        doReturn(mTab).when(mTabModel).getTabAt(anyInt());
        Assert.assertTrue(mMediator.maybeSwitchToTab(null));
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
        Assert.assertEquals(
                firstHistogramTotalCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_FIRST));
        Assert.assertEquals(
                lastHistogramTotalCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_LAST));

        if (firstHistogramTime != null) {
            Assert.assertEquals(
                    1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_FIRST,
                            firstHistogramTime));
        }

        if (lastHistogramTime != null) {
            Assert.assertEquals(
                    1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_LAST,
                            lastHistogramTime));
        }
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_recordedForZps() {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onOmniboxSessionStateChange(true);
        verify(mAutocompleteController)
                .startZeroSuggest(
                        "", url, pageClassification, title, /* isOnFocusContext= */ false);
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
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onOmniboxSessionStateChange(true);
        verify(mAutocompleteController)
                .startZeroSuggest(
                        "", url, pageClassification, title, /* isOnFocusContext= */ false);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report first results. Observe first results histogram reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(10));
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ false);
        verifySuggestionRequestToUiModelHistograms(1, 10, 0, null);

        // Cancel the interaction.
        mMediator.onOmniboxSessionStateChange(false);

        // Report last results. Observe no final report.
        verifySuggestionRequestToUiModelHistograms(1, 10, 0, null);
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_notRecordedWhenCanceled_FirstResult() {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onOmniboxSessionStateChange(true);
        verify(mAutocompleteController)
                .startZeroSuggest(
                        "", url, pageClassification, title, /* isOnFocusContext= */ false);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Cancel the interaction.
        mMediator.onOmniboxSessionStateChange(false);

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
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onOmniboxSessionStateChange(true);
        verify(mAutocompleteController)
                .startZeroSuggest(
                        "", url, pageClassification, title, /* isOnFocusContext= */ false);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report first result as final. Observe both metrics reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(150));
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);
        verifySuggestionRequestToUiModelHistograms(1, 150, 1, 150);
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_subsequentKeyStrokesReportTimeSinceLastKeystroke() {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onOmniboxSessionStateChange(true);
        verify(mAutocompleteController)
                .startZeroSuggest(
                        "", url, pageClassification, title, /* isOnFocusContext= */ false);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report first result as final. Observe both metrics reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(150));
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ false);
        verifySuggestionRequestToUiModelHistograms(1, 150, 0, null);

        // No change on key press. No unexpected recordings.
        // Need to run looper here to flush the pending operation.
        mMediator.onTextChanged("a", /* isOnFocusContext= */ false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifySuggestionRequestToUiModelHistograms(1, 150, 0, null);

        // No change on key press. No unexpected recordings.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(100));
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);
        verifySuggestionRequestToUiModelHistograms(2, 100, 1, 100);
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
        mMediator.setAutocompleteProfile(mProfile);
        when(mLocationBarDataProvider.hasTab()).thenReturn(false);
        when(mAutocompleteController.onSuggestionTouchDown(any(), anyInt(), any()))
                .thenReturn(true);
        setSuggestionNativeObjectRef();
        mMediator.onNativeInitialized();
        // Simulate omnibox session start, and offer suggestions.
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);

        // Simulate a suggestion being touched down.
        mMediator.onSuggestionTouchDown(mSuggestionsList.get(0), /* matchIndex= */ 0);

        // Ensure that no extra signals are sent to native.
        verify(mAutocompleteController, times(1))
                .onSuggestionTouchDown(mSuggestionsList.get(0), 0, null);

        // Simulate a navigation to the suggestion that was prefetched. This causes metrics about
        // prefetch to be recorded.
        mMediator.onSuggestionClicked(
                mSuggestionsList.get(0), /* matchIndex= */ 0, JUnitTestGURLs.URL_1);

        // Ends the omnibox session to reset state of touch down prefetch, and record metrics.
        mMediator.onOmniboxSessionStateChange(false);

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
        mMediator.setAutocompleteProfile(mProfile);
        when(mLocationBarDataProvider.hasTab()).thenReturn(false);
        when(mAutocompleteController.onSuggestionTouchDown(any(), anyInt(), any()))
                .thenReturn(true);
        setSuggestionNativeObjectRef();
        mMediator.onNativeInitialized();
        // Simulate omnibox session start, and offer suggestions.
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);

        // Simulate a suggestion being touched down.
        mMediator.onSuggestionTouchDown(mSuggestionsList.get(0), /* matchIndex= */ 0);

        // Ensure that no extra signals are sent to native.
        verify(mAutocompleteController, times(1))
                .onSuggestionTouchDown(mSuggestionsList.get(0), 0, null);

        // Simulate a navigation to a suggestion that was not prefetched. This causes metrics about
        // prefetch to be recorded.
        mMediator.onSuggestionClicked(
                mSuggestionsList.get(1), /* matchIndex= */ 1, JUnitTestGURLs.URL_1);

        // Ends the omnibox session to reset state of touch down prefetch, and record metrics.
        mMediator.onOmniboxSessionStateChange(false);

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
        mMediator.setAutocompleteProfile(mProfile);
        when(mLocationBarDataProvider.hasTab()).thenReturn(false);
        setSuggestionNativeObjectRef();
        mMediator.onNativeInitialized();
        // Simulate omnibox session start, and offer suggestions.
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onSuggestionsReceived(mAutocompleteResult, /* isFinal= */ true);

        // This will simulate the touch down trigger not starting a prefetch.
        when(mAutocompleteController.onSuggestionTouchDown(any(), anyInt(), any()))
                .thenReturn(false);

        // Simulate a suggestion being touched down.
        mMediator.onSuggestionTouchDown(mSuggestionsList.get(0), /* matchIndex= */ 0);

        // Ensure that no extra signals are sent to native.
        verify(mAutocompleteController, times(1))
                .onSuggestionTouchDown(mSuggestionsList.get(0), 0, null);

        // Simulate a navigation to the suggestion that was not prefetched. This causes metrics
        // about prefetch to be recorded.
        mMediator.onSuggestionClicked(
                mSuggestionsList.get(0), /* matchIndex= */ 0, JUnitTestGURLs.URL_1);

        // Ends the omnibox session to reset state of touch down prefetch, and record metrics.
        mMediator.onOmniboxSessionStateChange(false);

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
        mMediator.setAutocompleteProfile(mProfile);
        when(mLocationBarDataProvider.hasTab()).thenReturn(false);
        when(mAutocompleteController.onSuggestionTouchDown(any(), anyInt(), any()))
                .thenReturn(true);
        setSuggestionNativeObjectRef();
        mMediator.onNativeInitialized();
        // Simulate omnibox session start.
        mMediator.onOmniboxSessionStateChange(true);

        // Triggeer one touch down event the maximum allowed. The extra event should not be sent to
        // native.
        int numTouchDownEvents = OmniboxFeatures.DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION + 1;
        Assert.assertTrue(numTouchDownEvents < mSuggestionsList.size());
        for (int i = 0; i < numTouchDownEvents; i++) {
            mMediator.onSuggestionTouchDown(mSuggestionsList.get(i), i);
        }

        // Ensure that no extra signals are sent to native.
        verify(
                        mAutocompleteController,
                        times(OmniboxFeatures.DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION))
                .onSuggestionTouchDown(any(), anyInt(), any());

        // Ends the omnibox session to reset state of touch down prefetch, and record metrics.
        mMediator.onOmniboxSessionStateChange(false);

        // Since the state is reset, new prefetches are allowed.
        // Simulate a new omnibox session start.
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onSuggestionTouchDown(mSuggestionsList.get(0), 0);
        verify(
                        mAutocompleteController,
                        times(OmniboxFeatures.DEFAULT_MAX_PREFETCHES_PER_OMNIBOX_SESSION + 1))
                .onSuggestionTouchDown(any(), anyInt(), any());
        mMediator.onOmniboxSessionStateChange(false);

        histogramWatcher.assertExpected();
    }

    @Test
    public void onTopResumedActivityChanged_toActive() {
        mMediator.onTopResumedActivityChanged(true);
        verify(mAutocompleteDelegate, never()).clearOmniboxFocus();
    }

    @Test
    public void onTopResumedActivityChanged_toNonActive() {
        mMediator.onTopResumedActivityChanged(false);
        Assert.assertEquals(mMediator.getEditSessionStateForTest(), EditSessionState.INACTIVE);
        verify(mAutocompleteDelegate, times(1)).clearOmniboxFocus();
    }

    @Test
    public void onTextChanged_cachedZpsEligibleOnSelectPageClasses() {
        Set<Integer> eligibleClasses =
                Set.of(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE);

        doReturn(mAutocompleteResult).when(ShadowCachedSuggestionsManager.mock).readFromCache();

        for (var pageClass : PageClassification.values()) {
            setUpLocationBarDataProvider(PAGE_URL, PAGE_TITLE, pageClass.getNumber());

            mMediator.onTextChanged("", /* isOnFocusContext= */ false);

            // Should only be invoked if page class is eligible.
            int numTimesInvoked = eligibleClasses.contains(pageClass.getNumber()) ? 1 : 0;
            verify(ShadowCachedSuggestionsManager.mock, times(numTimesInvoked)).readFromCache();
            verify(ShadowCachedSuggestionsManager.mock, never()).saveToCache(any());

            clearInvocations(ShadowCachedSuggestionsManager.mock);
        }
    }

    @Test
    public void onTextChanged_cachedZpsNotInvokedInTypedContext() {
        doReturn(mAutocompleteResult).when(ShadowCachedSuggestionsManager.mock).readFromCache();

        for (var pageClass : PageClassification.values()) {
            setUpLocationBarDataProvider(PAGE_URL, PAGE_TITLE, pageClass.getNumber());

            mMediator.onTextChanged("text", /* isOnFocusContext= */ false);

            // Should only be invoked if page class is eligible.
            verify(ShadowCachedSuggestionsManager.mock, never()).readFromCache();
            verify(ShadowCachedSuggestionsManager.mock, never()).saveToCache(any());

            clearInvocations(ShadowCachedSuggestionsManager.mock);
        }
    }

    @Test
    public void onTextChanged_cachedZpsNotInvokedWithAutocompleteControllerReady() {
        doReturn(mAutocompleteResult).when(ShadowCachedSuggestionsManager.mock).readFromCache();
        mMediator.setAutocompleteProfile(mProfile);

        for (var pageClass : PageClassification.values()) {
            setUpLocationBarDataProvider(PAGE_URL, PAGE_TITLE, pageClass.getNumber());

            mMediator.onTextChanged("", /* isOnFocusContext= */ false);

            // Should only be invoked if page class is eligible.
            verify(ShadowCachedSuggestionsManager.mock, never()).readFromCache();
            verify(ShadowCachedSuggestionsManager.mock, never()).saveToCache(any());

            clearInvocations(ShadowCachedSuggestionsManager.mock);
        }
    }

    @Test
    public void onTextChanged_cacheZpsFromEligiblePageClasses() {
        Set<Integer> eligibleClasses = Set.of(PageClassification.ANDROID_SEARCH_WIDGET_VALUE);

        mMediator.onOmniboxSessionStateChange(true);
        doReturn(mAutocompleteResult).when(ShadowCachedSuggestionsManager.mock).readFromCache();
        doReturn(false).when(mAutocompleteResult).isFromCachedResult();

        for (var pageClass : PageClassification.values()) {
            setUpLocationBarDataProvider(PAGE_URL, PAGE_TITLE, pageClass.getNumber());

            mMediator.onTextChanged("", /* isOnFocusContext= */ false);

            // Should only be invoked if page class is eligible.
            int numTimesInvoked = eligibleClasses.contains(pageClass.getNumber()) ? 1 : 0;
            verify(ShadowCachedSuggestionsManager.mock, times(numTimesInvoked)).saveToCache(any());

            clearInvocations(ShadowCachedSuggestionsManager.mock);
        }
    }

    @Test
    public void onTextChanged_dontCacheTypedSuggestions() {
        doReturn(mAutocompleteResult).when(ShadowCachedSuggestionsManager.mock).readFromCache();
        doReturn(false).when(mAutocompleteResult).isFromCachedResult();

        for (var pageClass : PageClassification.values()) {
            setUpLocationBarDataProvider(PAGE_URL, PAGE_TITLE, pageClass.getNumber());
            mMediator.onTextChanged("x", /* isOnFocusContext= */ false);
            verify(ShadowCachedSuggestionsManager.mock, never()).saveToCache(any());
            clearInvocations(ShadowCachedSuggestionsManager.mock);
        }
    }

    @Test
    public void onTextChanged_dontCacheCachedSuggestions() {
        doReturn(mAutocompleteResult).when(ShadowCachedSuggestionsManager.mock).readFromCache();
        doReturn(true).when(mAutocompleteResult).isFromCachedResult();

        for (var pageClass : PageClassification.values()) {
            setUpLocationBarDataProvider(PAGE_URL, PAGE_TITLE, pageClass.getNumber());
            mMediator.onTextChanged("", /* isOnFocusContext= */ false);
            verify(ShadowCachedSuggestionsManager.mock, never()).saveToCache(any());
            clearInvocations(ShadowCachedSuggestionsManager.mock);
        }
    }

    @Test
    public void updateVisualsForState_informsVisualStateObserver() {
        mMediator.updateVisualsForState(BrandedColorScheme.LIGHT_BRANDED_THEME);
        verify(mVisualStateObserver)
                .onOmniboxSuggestionsBackgroundColorChanged(
                        eq(
                                OmniboxResourceProvider
                                        .getSuggestionsDropdownStandardBackgroundColor(mActivity)));

        mMediator.updateVisualsForState(BrandedColorScheme.INCOGNITO);
        verify(mVisualStateObserver)
                .onOmniboxSuggestionsBackgroundColorChanged(
                        eq(
                                OmniboxResourceProvider
                                        .getSuggestionsDropdownIncognitoBackgroundColor(
                                                mActivity)));
    }

    @Test
    public void clearSuggestions_informsVisualStateObserver() {
        mMediator.onNativeInitialized();
        mMediator.setAutocompleteProfile(mProfile);

        mMediator.clearSuggestions();
    }

    @Test
    public void propagateOmniboxSessionStateChange_informsVisualStateObserver() {
        mMediator.propagateOmniboxSessionStateChange(true);
        verify(mVisualStateObserver, atLeastOnce()).onOmniboxSessionStateChange(eq(true));

        mMediator.propagateOmniboxSessionStateChange(false);
        verify(mVisualStateObserver, atLeastOnce()).onOmniboxSessionStateChange(eq(false));
    }

    @Test
    public void onOmniboxAnswerActionClicked() {
        mMediator.setAutocompleteProfile(mProfile);
        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
        OmniboxAnswerAction answerAction =
                (OmniboxAnswerAction)
                        OmniboxActionFactoryImpl.get()
                                .buildOmniboxAnswerAction(123L, "7 day forecast", "7 day forecast");

        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), /* isFinal= */ true);
        mMediator.onOmniboxActionClicked(answerAction, 0);

        verify(mAutocompleteDelegate).loadUrl(mOmniboxLoadUrlParamsCaptor.capture());
    }

    @Test
    @SmallTest
    @EnableFeatures(OmniboxFeatureList.ANIMATE_SUGGESTIONS_LIST_APPEARANCE)
    public void onOmniboxSessionStateChange_attachesImeCallback() {
        mMediator.onNativeInitialized();

        mMediator.onOmniboxSessionStateChange(true);
        verify(mDeferredImeCallback).attach(mWindowAndroid);

        mMediator.onOmniboxSessionStateChange(false);
        verify(mDeferredImeCallback).detach();
    }

    @Test
    public void bailWhenPageClassGone() {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("", /* isOnFocusContext= */ false);
        verify(mAutocompleteController, never())
                .startZeroSuggest(
                        "", url, pageClassification, title, /* isOnFocusContext= */ false);

        mMediator.onTextChanged("t", /* isOnFocusContext= */ false);
        verify(mAutocompleteController, never())
                .start(any(), anyInt(), any(), anyInt(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testDefaultBrowserPromo_notShownWithIncorrectSuggestionType() {
        mMediator.setAutocompleteProfile(mProfile);
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
        mMediator.setAutocompleteProfile(mProfile);

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
        mMediator.setAutocompleteProfile(mProfile);
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
}
