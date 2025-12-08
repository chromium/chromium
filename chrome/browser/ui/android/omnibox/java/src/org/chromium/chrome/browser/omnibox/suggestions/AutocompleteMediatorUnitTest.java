// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

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

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.DeferredIMEWindowInsetApplicationCallback;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionFactoryJni;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Tests for {@link AutocompleteMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ShadowLooper.class)
public class AutocompleteMediatorUnitTest {
    private static final int SUGGESTION_MIN_HEIGHT = 20;
    private static final int HEADER_MIN_HEIGHT = 15;
    private static final GURL PAGE_URL = new GURL("https://www.site.com/page.html");
    private static final String PAGE_TITLE = "Page Title";

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
    private @Mock FuseboxCoordinator mFuseboxCoordinator;
    private @Captor ArgumentCaptor<OmniboxLoadUrlParams> mOmniboxLoadUrlParamsCaptor;
    private @Mock CachedZeroSuggestionsManager.OverridesForTesting
            mMockCachedZeroSuggestionsManager;

    private final ObservableSupplierImpl<Boolean> mAttachmentsPresentSupplier =
            new ObservableSupplierImpl<>(false);
    private PropertyModel mListModel;
    private AutocompleteMediator mMediator;
    private List<AutocompleteMatch> mSuggestionsList;
    private AutocompleteResult mAutocompleteResult;
    private ModelList mSuggestionModels;
    private ObservableSupplierImpl<@ControlsPosition Integer> mToolbarPositionSupplier;
    private ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier;
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        CachedZeroSuggestionsManager.setOverridesForTesting(mMockCachedZeroSuggestionsManager);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJniMock);
        OmniboxActionFactoryJni.setInstanceForTesting(mActionFactoryJni);
        AutocompleteControllerJni.setInstanceForTesting(mControllerJniMock);
        mToolbarPositionSupplier = new ObservableSupplierImpl<>(ControlsPosition.TOP);
        mAutocompleteRequestTypeSupplier =
                new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH);

        lenient().doReturn(mAutocompleteController).when(mControllerJniMock).getForProfile(any());

        mSuggestionModels = new ModelList();
        mListModel =
                new PropertyModel.Builder(SuggestionListProperties.ALL_KEYS)
                        .with(SuggestionListProperties.SUGGESTION_MODELS, mSuggestionModels)
                        .build();

        lenient().doReturn(mInsetObserver).when(mWindowAndroid).getInsetObserver();
        lenient().doReturn(mWindow).when(mWindowAndroid).getWindow();
        lenient().doReturn(mDecorView).when(mWindow).getDecorView();
        lenient()
                .doReturn(mToolbarPositionSupplier)
                .when(mLocationBarDataProvider)
                .getToolbarPositionSupplier();

        lenient()
                .doReturn(mAutocompleteRequestTypeSupplier)
                .when(mFuseboxCoordinator)
                .getAutocompleteRequestTypeSupplier();

        lenient()
                .doReturn(mAttachmentsPresentSupplier)
                .when(mFuseboxCoordinator)
                .getAttachmentsPresentSupplier();

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
        lenient().doReturn(true).when(mAutocompleteDelegate).isKeyboardActive();
        setUpLocationBarDataProvider(
                JUnitTestGURLs.NTP_URL, "New Tab Page", PageClassification.NTP_VALUE);

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
    public void updateSuggestionsList_worksWithNullList() {
        mMediator.onNativeInitialized();

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
        mMediator.onNativeInitialized();

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
    public void onOmniboxSessionStateChange_mobileMode_emptyOmnibox() {
        // In Mobile mode, if LocationBar clears the Page URL on focus, Autocomplete requests
        // Zero-Prefix suggestions.
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(false);

        GURL url = new GURL("https://www.google.com");
        String title = "title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        mMediator.onNativeInitialized();
        mMediator.setAutocompleteProfile(mProfile);
        mMediator.onOmniboxSessionStateChange(true);
        ShadowLooper.runUiThreadTasks();
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
    }

    @Test
    public void onOmniboxSessionStateChange_mobileMode_populatedOmnibox() {
        // In Mobile mode, if LocationBar does not clear the Page URL on focus, Autocomplete
        // requests Prefixed suggestions.
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(false);

        GURL url = new GURL("https://www.google.com");
        String title = "title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("test");
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        mMediator.onNativeInitialized();
        mMediator.setAutocompleteProfile(mProfile);
        mMediator.onOmniboxSessionStateChange(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyAutocompleteStart(url, pageClassification, "test", 0, true);
    }

    public void verifyAutocompleteStart(
            GURL url, int pageClass, String userText, int cursorPos, boolean preventAutocomplete) {
        var captor = ArgumentCaptor.forClass(AutocompleteInput.class);
        verify(mAutocompleteController)
                .start(captor.capture(), eq(cursorPos), eq(preventAutocomplete));
        verify(mAutocompleteController, times(1)).start(any(), anyInt(), anyBoolean());

        AutocompleteInput input = captor.getValue();
        assertEquals(pageClass, input.getPageClassification());
        assertEquals(userText, input.getUserText());
        assertEquals(url.getSpec(), input.getPageUrl().getSpec());

        clearInvocations(mAutocompleteController);
    }

    public void verifyAutocompleteStartZeroSuggest(
            String userText, GURL url, int pageClass, String pageTitle) {
        var captor = ArgumentCaptor.forClass(AutocompleteInput.class);
        verify(mAutocompleteController).startZeroSuggest(captor.capture());
        verify(mAutocompleteController, times(1)).startZeroSuggest(any());

        AutocompleteInput input = captor.getValue();
        assertEquals(pageClass, input.getPageClassification());
        assertEquals(userText, input.getUserText());
        assertEquals(url.getSpec(), input.getPageUrl().getSpec());
        assertEquals(pageTitle, input.getPageTitle());

        clearInvocations(mAutocompleteController);
    }

    @Test
    public void onOmniboxSessionStateChange_desktopMode() {
        // In Desktop mode, Omnibox always retains the Page URL on focus.
        // Autocomplete should continue to request the Zero-Prefix suggestions.
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(true);

        GURL url = new GURL("https://www.google.com");
        String title = "title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("Text");
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        mMediator.onNativeInitialized();
        mMediator.setAutocompleteProfile(mProfile);
        mMediator.onOmniboxSessionStateChange(true);
        ShadowLooper.runUiThreadTasks();
        // Strictly expect the call to `startZeroSuggest()` here, as Desktop mode retains the
        // Omnibox content on focus.
        verifyAutocompleteStartZeroSuggest("Text", url, pageClassification, title);
    }

    @Test
    @SmallTest
    public void onTextChanged_emptyTextTriggersZeroSuggest() {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        mMediator.onOmniboxSessionStateChange(true);

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("", /* isOnFocusContext= */ false);
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
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
        verifyAutocompleteStart(url, pageClassification, "test", 4, false);
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
        verifyAutocompleteStart(url, pageClassification, "nottest", 4, false);
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

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");

        // Simulate URL being focus changes.
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onOmniboxSessionStateChange(false);
        mMediator.onOmniboxSessionStateChange(true);
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never()).startZeroSuggest(any());

        // Simulate native being initialized. Make sure we only ever issue one request, even if
        // there are multiple requests to activate the autocomplete session.
        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        mMediator.onOmniboxSessionStateChange(true);
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
    }

    @Test
    @SmallTest
    public void onOmniboxSessionStateChange_preventsZeroSuggestRequestOnDeactivation() {
        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        // Simulate URL being focus changes.
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onOmniboxSessionStateChange(false);
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never()).startZeroSuggest(any());

        // Simulate native being inititalized. Make sure no suggest requests are sent.
        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never()).startZeroSuggest(any());
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

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("A");

        // Simulate URL being focus changes, and that user typed text and deleted it.
        mMediator.onOmniboxSessionStateChange(true);
        mMediator.onTextChanged("A", /* isOnFocusContext= */ false);
        mMediator.onTextChanged("", /* isOnFocusContext= */ false);
        mMediator.onTextChanged("A", /* isOnFocusContext= */ false);

        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never()).start(any(), anyInt(), anyBoolean());
        verify(mAutocompleteController, never()).startZeroSuggest(any());

        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verifyAutocompleteStart(url, pageClassification, "A", 0, true);
        verify(mAutocompleteController, never()).startZeroSuggest(any());
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

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");

        // Simulate URL being focus changes, and that user typed text and deleted it.
        // - Typing should cancel initial ZPS request, and request regular suggestions
        // - Deletion should cancel pending typed suggest request and ask for ZPS.
        mMediator.onTextChanged("A", /* isOnFocusContext= */ false);
        mMediator.onTextChanged("", /* isOnFocusContext= */ false);

        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never()).start(any(), anyInt(), anyBoolean());
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
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
        clearInvocations(mAutocompleteDelegate);
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
        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
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
        // This scenario is true for the LFF devices with precision pointer
        // device attached.
        // Here we don't clear the URL in the omnibox, but still require the
        // Autocomplete to issue the zero prefix suggest request.
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(true);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn(url.getSpec());

        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
        ShadowLooper.runUiThreadTasks();
        verifyAutocompleteStartZeroSuggest(url.getSpec(), url, pageClassification, title);
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

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");

        // Signal focus prior to initializing native; confirm that zero suggest is not triggered.
        mMediator.onOmniboxSessionStateChange(true);
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never()).startZeroSuggest(any());

        // Initialize native and ensure zero suggest is triggered.
        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
    }

    @Test
    @SmallTest
    public void onOmniboxSessionStateChange_trackSessionState() {
        mMediator.setAutocompleteProfile(mProfile);
        mMediator.onNativeInitialized();

        assertFalse(mMediator.isOmniboxSessionActiveForTesting());

        mMediator.onOmniboxSessionStateChange(true);
        assertTrue(mMediator.isOmniboxSessionActiveForTesting());

        mMediator.onOmniboxSessionStateChange(false);
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
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onOmniboxSessionStateChange(true);
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
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onOmniboxSessionStateChange(true);
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
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
        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onOmniboxSessionStateChange(true);
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
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
        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onOmniboxSessionStateChange(true);
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
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);
        UnsyncedSuggestionsListAnimationDriver.setAnimationsDisabledForTesting(true);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onOmniboxSessionStateChange(true);
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
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
        assertTrue(numTouchDownEvents < mSuggestionsList.size());
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
    public void onTopResumedActivityChanged_nonZeroSuggest() {
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
        verifyAutocompleteStart(url, pageClassification, "test", 4, false);

        mMediator.onTopResumedActivityChanged(false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mAutocompleteController, never()).start(any(), anyInt(), anyBoolean());

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("test");

        mMediator.onTopResumedActivityChanged(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifyAutocompleteStart(url, pageClassification, "test", 4, false);
    }

    @Test
    public void onTopResumedActivityChanged_zeroSuggest() {
        mMediator.setAutocompleteProfile(mProfile);

        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        GURL url = JUnitTestGURLs.BLUE_1;
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        mMediator.onOmniboxSessionStateChange(true);

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("", /* isOnFocusContext= */ false);
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);

        mMediator.onTopResumedActivityChanged(false);
        verify(mAutocompleteController, never()).startZeroSuggest(any());

        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("");

        mMediator.onTopResumedActivityChanged(true);
        verifyAutocompleteStartZeroSuggest("", url, pageClassification, title);
    }

    @Test
    public void onTextChanged_cachedZpsEligibleOnSelectPageClasses() {
        Set<Integer> eligibleClasses =
                Set.of(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE);

        doReturn(mAutocompleteResult)
                .when(mMockCachedZeroSuggestionsManager)
                .readFromCache(anyInt());

        for (var pageClass : PageClassification.values()) {
            mMediator.getAutocompleteInputForTesting().setPageClassification(pageClass.getNumber());
            mMediator.onTextChanged("", /* isOnFocusContext= */ false);

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
        doReturn(mAutocompleteResult)
                .when(mMockCachedZeroSuggestionsManager)
                .readFromCache(anyInt());

        for (var pageClass : PageClassification.values()) {
            setUpLocationBarDataProvider(PAGE_URL, PAGE_TITLE, pageClass.getNumber());

            mMediator.onTextChanged("text", /* isOnFocusContext= */ false);

            // Should only be invoked if page class is eligible.
            verify(mMockCachedZeroSuggestionsManager, never()).readFromCache(anyInt());
            verify(mMockCachedZeroSuggestionsManager, never()).saveToCache(anyInt(), any());

            clearInvocations(mMockCachedZeroSuggestionsManager);
        }
    }

    @Test
    public void onTextChanged_cachedZpsNotInvokedWithAutocompleteControllerReady() {
        doReturn(mAutocompleteResult)
                .when(mMockCachedZeroSuggestionsManager)
                .readFromCache(anyInt());
        mMediator.setAutocompleteProfile(mProfile);

        for (var pageClass : PageClassification.values()) {
            setUpLocationBarDataProvider(PAGE_URL, PAGE_TITLE, pageClass.getNumber());

            mMediator.onTextChanged("", /* isOnFocusContext= */ false);

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

        mMediator.onOmniboxSessionStateChange(true);
        doReturn(mAutocompleteResult)
                .when(mMockCachedZeroSuggestionsManager)
                .readFromCache(anyInt());
        doReturn(false).when(mAutocompleteResult).isFromCachedResult();

        for (var pageClass : PageClassification.values()) {
            mMediator.getAutocompleteInputForTesting().setPageClassification(pageClass.getNumber());

            mMediator.onTextChanged("", /* isOnFocusContext= */ false);

            // Should only be invoked if page class is eligible.
            int numTimesInvoked = eligibleClasses.contains(pageClass.getNumber()) ? 1 : 0;
            verify(mMockCachedZeroSuggestionsManager, times(numTimesInvoked))
                    .saveToCache(anyInt(), any());

            clearInvocations(mMockCachedZeroSuggestionsManager);
        }
    }

    @Test
    public void onTextChanged_dontCacheTypedSuggestions() {
        doReturn(mAutocompleteResult)
                .when(mMockCachedZeroSuggestionsManager)
                .readFromCache(anyInt());
        doReturn(false).when(mAutocompleteResult).isFromCachedResult();

        for (var pageClass : PageClassification.values()) {
            setUpLocationBarDataProvider(PAGE_URL, PAGE_TITLE, pageClass.getNumber());
            mMediator.onTextChanged("x", /* isOnFocusContext= */ false);
            verify(mMockCachedZeroSuggestionsManager, never()).saveToCache(anyInt(), any());
            clearInvocations(mMockCachedZeroSuggestionsManager);
        }
    }

    @Test
    public void onTextChanged_dontCacheCachedSuggestions() {
        doReturn(mAutocompleteResult)
                .when(mMockCachedZeroSuggestionsManager)
                .readFromCache(anyInt());
        doReturn(true).when(mAutocompleteResult).isFromCachedResult();

        for (var pageClass : PageClassification.values()) {
            setUpLocationBarDataProvider(PAGE_URL, PAGE_TITLE, pageClass.getNumber());
            mMediator.onTextChanged("", /* isOnFocusContext= */ false);
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
    public void clearSuggestions_informsVisualStateObserver() {
        mMediator.onNativeInitialized();
        mMediator.setAutocompleteProfile(mProfile);

        mMediator.clearSuggestions();
    }

    @Test
    public void propagateOmniboxSessionStateChange_informsVisualStateObserver() {
        setUpLocationBarDataProvider(
                new GURL("https://abc.xyz"), "title", PageClassification.ANDROID_HUB_VALUE);
        mMediator.initAutocompleteInput();

        mMediator.propagateOmniboxSessionStateChange(true);
        verify(mVisualStateObserver, atLeastOnce()).onOmniboxSessionStateChange(eq(true));

        mMediator.propagateOmniboxSessionStateChange(false);
        verify(mVisualStateObserver, atLeastOnce()).onOmniboxSessionStateChange(eq(false));
    }

    @Test
    public void propagateOmniboxSessionStateChange_hubSearchContainerVisible() {
        setUpLocationBarDataProvider(
                new GURL("https://abc.xyz"), "title", PageClassification.ANDROID_HUB_VALUE);
        mMediator.initAutocompleteInput();
        assertTrue(mListModel.get(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE));

        setUpLocationBarDataProvider(
                new GURL("https://abc.xyz"),
                "title",
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE);
        mMediator.initAutocompleteInput();
        assertFalse(mListModel.get(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE));
    }

    @Test
    public void onTopResumedActivityChanged_hubSearchContainerVisible() {
        setUpLocationBarDataProvider(
                new GURL("https://abc.xyz"), "title", PageClassification.ANDROID_HUB_VALUE);

        mMediator.initAutocompleteInput();
        assertTrue(mListModel.get(SuggestionListProperties.ACTIVITY_WINDOW_FOCUSED));

        mMediator.onTopResumedActivityChanged(false);
        assertTrue(mListModel.get(SuggestionListProperties.ACTIVITY_WINDOW_FOCUSED));
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

    @Test
    @SmallTest
    public void loadTypedOmniboxText_aimUrl() {
        mMediator.setAutocompleteProfile(mProfile);
        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("test");
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("test");
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);
        GURL url = JUnitTestGURLs.BLUE_2;
        when(mFuseboxCoordinator.getAimUrl(any())).thenReturn(url);

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
        mMediator.setAutocompleteProfile(mProfile);
        mMediator.onNativeInitialized();
        mMediator.onOmniboxSessionStateChange(true);
        when(mTextStateProvider.getTextWithoutAutocomplete()).thenReturn("test");
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("test");
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.IMAGE_GENERATION);
        GURL url1 = JUnitTestGURLs.BLUE_1;
        when(mFuseboxCoordinator.getAimUrl(any())).thenReturn(url1);
        GURL url2 = JUnitTestGURLs.BLUE_2;
        when(mFuseboxCoordinator.getImageGenerationUrl(any())).thenReturn(url2);

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
        mMediator
                .getAutocompleteInputForTesting()
                .setPageClassification(PageClassification.ANDROID_SEARCH_WIDGET_VALUE);

        doReturn(mAutocompleteResult)
                .when(mMockCachedZeroSuggestionsManager)
                .readFromCache(anyInt());

        // Cached suggestions should be suppressed when on an Incognito NTP with autofocus enabled
        // and zero suggest disabled.
        doReturn(true).when(ntpDelegate).isIncognitoNewTabPageCurrentlyVisible();
        mMediator.onTextChanged("", /* isOnFocusContext= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockCachedZeroSuggestionsManager, never()).readFromCache(anyInt());

        // Histogram should be recorded once.
        histogramWatcher.assertExpected();

        // When not on an Incognito NTP, cached suggestions should be shown.
        doReturn(false).when(ntpDelegate).isIncognitoNewTabPageCurrentlyVisible();
        mMediator.onTextChanged("", /* isOnFocusContext= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockCachedZeroSuggestionsManager, times(1)).readFromCache(anyInt());

        // Histogram record count should not be increased.
        histogramWatcher.assertExpected();
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

        mMediator
                .getAutocompleteInputForTesting()
                .setPageClassification(PageClassification.ANDROID_SEARCH_WIDGET_VALUE);
        doReturn(mAutocompleteResult)
                .when(mMockCachedZeroSuggestionsManager)
                .readFromCache(anyInt());

        // When the feature is enabled and zero suggest should be enabled,
        // cached suggestions should be shown, and the Incognito NTP check should be skipped.
        mMediator.onTextChanged("", /* isOnFocusContext= */ false);
        verify(mMockCachedZeroSuggestionsManager, times(1)).readFromCache(anyInt());
        verify(mLocationBarDataProvider, never()).getNewTabPageDelegate();

        histogramWatcher.assertExpected();
    }
}
