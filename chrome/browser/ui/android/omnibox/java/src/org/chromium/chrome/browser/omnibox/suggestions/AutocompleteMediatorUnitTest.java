// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Handler;
import android.util.SparseArray;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowPausedSystemClock;

import org.chromium.base.ActivityState;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteMediator.EditSessionState;
import org.chromium.chrome.browser.omnibox.suggestions.base.HistoryClustersProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionFactoryJni;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.ShadowGURL;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link AutocompleteMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowLog.class, ShadowLooper.class, ShadowGURL.class})
@Features.EnableFeatures({ChromeFeatureList.CLEAR_OMNIBOX_FOCUS_AFTER_NAVIGATION})
public class AutocompleteMediatorUnitTest {
    private static final int SUGGESTION_MIN_HEIGHT = 20;
    private static final int HEADER_MIN_HEIGHT = 15;

    public @Rule TestRule mProcessor = new Features.JUnitProcessor();
    public @Rule JniMocker mJniMocker = new JniMocker();
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock AutocompleteDelegate mAutocompleteDelegate;
    private @Mock UrlBarEditingTextStateProvider mTextStateProvider;
    private @Mock SuggestionProcessor mMockProcessor;
    private @Mock HeaderProcessor mMockHeaderProcessor;
    private @Mock AutocompleteControllerProvider mAutocompleteProvider;
    private @Mock AutocompleteController mAutocompleteController;
    private @Mock LocationBarDataProvider mLocationBarDataProvider;
    private @Mock ModalDialogManager mModalDialogManager;
    private @Mock Profile mProfile;
    private @Mock Tab mTab;
    private @Mock TabModel mTabModel;
    private @Mock TabWindowManager mTabManager;
    private @Mock WindowAndroid mMockWindowAndroid;
    private @Mock OmniboxActionDelegate mOmniboxActionDelegate;
    private @Mock LargeIconBridge.Natives mLargeIconBridgeJniMock;
    private @Mock HistoryClustersProcessor.OpenHistoryClustersDelegate mOpenHistoryClustersDelegate;
    private @Mock OmniboxActionFactoryJni mActionFactoryJni;

    private PropertyModel mListModel;
    private AutocompleteMediator mMediator;
    private List<AutocompleteMatch> mSuggestionsList;
    private AutocompleteResult mAutocompleteResult;
    private ModelList mSuggestionModels;
    private ObservableSupplierImpl<TabWindowManager> mTabWindowManagerSupplier;

    @Before
    public void setUp() {
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeJniMock);
        mJniMocker.mock(OmniboxActionFactoryJni.TEST_HOOKS, mActionFactoryJni);

        doReturn(mAutocompleteController).when(mAutocompleteProvider).get(any());

        // clang-format off
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSuggestionModels = new ModelList();
            mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
            mListModel.set(SuggestionListProperties.SUGGESTION_MODELS, mSuggestionModels);

            mTabWindowManagerSupplier = new ObservableSupplierImpl<>();

            mMediator = new AutocompleteMediator(ContextUtils.getApplicationContext(),
                    mAutocompleteProvider,
                    mAutocompleteDelegate, mTextStateProvider, mListModel,
                    new Handler(), () -> mModalDialogManager, null, null,
                    mLocationBarDataProvider, tab -> {}, mTabWindowManagerSupplier, url -> false,
                    mOmniboxActionDelegate, mOpenHistoryClustersDelegate);
            mMediator.setAutocompleteProfile(mProfile);
        });
        // clang-format on
        mMediator.getDropdownItemViewInfoListBuilderForTest().registerSuggestionProcessor(
                mMockProcessor);
        mMediator.getDropdownItemViewInfoListBuilderForTest().setHeaderProcessorForTest(
                mMockHeaderProcessor);

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

        mSuggestionsList = buildDummySuggestionsList(10, "Suggestion");
        mAutocompleteResult = AutocompleteResult.fromCache(mSuggestionsList, null);
        doReturn(true).when(mAutocompleteDelegate).isKeyboardActive();
        setUpLocationBarDataProvider(
                "chrome-native://newtab", "New Tab Page", PageClassification.NTP_VALUE);
    }

    /**
     * Build a fake suggestions list with elements named 'Suggestion #', where '#' is the suggestion
     * index (1-based).
     *
     * @return List of suggestions.
     */
    private List<AutocompleteMatch> buildDummySuggestionsList(int count, String prefix) {
        List<AutocompleteMatch> list = new ArrayList<>();
        for (int index = 0; index < count; ++index) {
            list.add(AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                             .setDisplayText(prefix + (index + 1))
                             .build());
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
    private SparseArray<String> buildDummyGroupHeaders(int count, int baseKey, String prefix) {
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
    void setUpLocationBarDataProvider(String url, String title, int pageClassification) {
        when(mLocationBarDataProvider.hasTab()).thenReturn(true);
        when(mLocationBarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mLocationBarDataProvider.getTitle()).thenReturn(title);
        when(mLocationBarDataProvider.getPageClassification(false, false))
                .thenReturn(pageClassification);
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_worksWithNullList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(AutocompleteResult.EMPTY_RESULT, "", true);

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_worksWithEmptyList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(AutocompleteResult.EMPTY_RESULT, "", true);

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_scrolEventsWithConcealedItemsTogglesKeyboardVisibility() {
        mMediator.onNativeInitialized();

        final int heightWithOneConcealedItem =
                (mSuggestionsList.size() - 2) * SUGGESTION_MIN_HEIGHT;

        mMediator.onSuggestionDropdownHeightChanged(heightWithOneConcealedItem);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), "", true);

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
                AutocompleteResult.fromCache(mSuggestionsList, null), "", true);

        // Build separate list of suggestions so that these are accepted as a new set.
        // We want to follow the same restrictions as the original list (specifically: have a
        // resulting list of suggestions taller than the space in dropdown view), so make sure
        // the list sizes are same.
        List<AutocompleteMatch> newList =
                buildDummySuggestionsList(mSuggestionsList.size(), "SuggestionB");
        mMediator.onSuggestionDropdownHeightChanged(heightWithOneConcealedItem);
        mMediator.onSuggestionsReceived(AutocompleteResult.fromCache(newList, null), "", true);
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
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), "", true);

        // "Hide keyboard", report larger area and re-evaluate the results. We should see no
        // difference, as the logic should only evaluate presence of items concealed when keyboard
        // is active.
        when(mAutocompleteDelegate.isKeyboardActive()).thenReturn(false);
        mMediator.onSuggestionDropdownHeightChanged(heightOfOAllSuggestions);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), "", true);
    }

    @Test
    @SmallTest
    public void onTextChanged_emptyTextTriggersZeroSuggest() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("", "");
        verify(mAutocompleteController).startZeroSuggest("", url, pageClassification, title);
    }

    @Test
    @SmallTest
    public void onTextChanged_nonEmptyTextTriggersSuggestions() {
        String url = "http://www.example.com";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, url, pageClassification);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("test", "testing");
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mAutocompleteController).start(url, pageClassification, "test", 4, false);
    }

    @Test
    @SmallTest
    public void onTextChanged_cancelsPendingRequests() {
        String url = "http://www.example.com";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, url, pageClassification);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("test", "testing");
        mMediator.onTextChanged("nottest", "nottesting");
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mAutocompleteController, times(1))
                .start(url, pageClassification, "nottest", 4, false);
        verify(mAutocompleteController, times(1))
                .start(any(), anyInt(), any(), anyInt(), anyBoolean());
    }

    @Test
    @SmallTest
    public void onUrlFocusChange_onlyOneZeroSuggestRequestIsInvoked() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Simulate URL being focus changes.
        mMediator.onUrlFocusChange(true);
        mMediator.onUrlFocusChange(false);
        mMediator.onUrlFocusChange(true);
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never()).startZeroSuggest(any(), any(), anyInt(), any());

        // Simulate native being inititalized. Make sure we only ever issue one request.
        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, times(1))
                .startZeroSuggest("", url, pageClassification, title);
    }

    @Test
    @SmallTest
    public void onUrlFocusChange_preventsZeroSuggestRequestOnFocusLost() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Simulate URL being focus changes.
        mMediator.onUrlFocusChange(true);
        mMediator.onUrlFocusChange(false);
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never()).startZeroSuggest(any(), any(), anyInt(), any());

        // Simulate native being inititalized. Make sure no suggest requests are sent.
        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never()).startZeroSuggest(any(), any(), anyInt(), any());
    }

    @Test
    @SmallTest
    public void onUrlFocusChange_textChangeCancelsOustandingZeroSuggestRequest() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("A");

        // Simulate URL being focus changes, and that user typed text and deleted it.
        mMediator.onUrlFocusChange(true);
        mMediator.onTextChanged("A", "Abc");
        mMediator.onTextChanged("", "");
        mMediator.onTextChanged("A", "Abc");

        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never())
                .start(any(), anyInt(), any(), anyInt(), anyBoolean());
        verify(mAutocompleteController, never()).startZeroSuggest(any(), any(), anyInt(), any());

        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, times(1)).start(url, pageClassification, "A", 0, true);
        verify(mAutocompleteController, times(1))
                .start(any(), anyInt(), any(), anyInt(), anyBoolean());
        verify(mAutocompleteController, never()).startZeroSuggest(any(), any(), anyInt(), any());
    }

    @Test
    @SmallTest
    public void onUrlFocusChange_textChangeCancelsIntermediateZeroSuggestRequests() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Simulate URL being focus changes, and that user typed text and deleted it.
        mMediator.onTextChanged("A", "Abc");
        mMediator.onTextChanged("", "");

        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never())
                .start(any(), anyInt(), any(), anyInt(), anyBoolean());
        verify(mAutocompleteController, never()).startZeroSuggest(any(), any(), anyInt(), any());

        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never())
                .start(any(), anyInt(), any(), anyInt(), anyBoolean());
        verify(mAutocompleteController, times(1)).startZeroSuggest(any(), any(), anyInt(), any());
    }

    @Test
    @SmallTest
    public void onSuggestionsReceived_sendsOnSuggestionsChanged() {
        mMediator.onNativeInitialized();
        mMediator.onUrlFocusChange(true);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), "inline_autocomplete", true);
        verify(mAutocompleteDelegate).onSuggestionsChanged("inline_autocomplete", true);

        // Ensure duplicate requests are not suppressed, to preserve the
        // relationship between Native and Java AutocompleteResult objects.
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), "inline_autocomplete2", true);
        verify(mAutocompleteDelegate).onSuggestionsChanged("inline_autocomplete", true);
    }

    @Test
    @SmallTest
    public void setLayoutDirection_beforeInitialization() {
        mMediator.onNativeInitialized();
        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), "", true);
        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @Test
    @SmallTest
    public void setLayoutDirection_afterInitialization() {
        mMediator.onNativeInitialized();
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        mMediator.onSuggestionsReceived(
                AutocompleteResult.fromCache(mSuggestionsList, null), "", true);
        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_LTR,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @Test
    @SmallTest
    public void onUrlFocusChange_triggersZeroSuggest_nativeInitialized() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn(url);

        mMediator.onNativeInitialized();
        mMediator.onUrlFocusChange(true);
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController).startZeroSuggest(url, url, pageClassification, title);
    }

    @Test
    @SmallTest
    public void onUrlFocusChange_triggersZeroSuggest_nativeNotInitialized() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Signal focus prior to initializing native; confirm that zero suggest is not triggered.
        mMediator.onUrlFocusChange(true);
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController, never()).startZeroSuggest(any(), any(), anyInt(), any());

        // Initialize native and ensure zero suggest is triggered.
        mMediator.onNativeInitialized();
        ShadowLooper.runUiThreadTasks();
        verify(mAutocompleteController).startZeroSuggest("", url, pageClassification, title);
    }

    @Test
    @SmallTest
    public void onTextChanged_editSessionActivatedByUserInput() {
        mMediator.onNativeInitialized();
        mMediator.onUrlFocusChange(true);
        Assert.assertEquals(mMediator.getEditSessionStateForTest(), EditSessionState.INACTIVE);
        mMediator.onTextChanged("n", "news");
        Assert.assertEquals(
                mMediator.getEditSessionStateForTest(), EditSessionState.ACTIVATED_BY_USER_INPUT);

        mMediator.onUrlFocusChange(false);
        Assert.assertEquals(mMediator.getEditSessionStateForTest(), EditSessionState.INACTIVE);
    }

    @Test
    @SmallTest
    public void switchToTab_noTargetTab() {
        // There is no Tab to switch to.
        doReturn(null).when(mAutocompleteController).getMatchingTabForSuggestion(anyInt());
        Assert.assertFalse(mMediator.maybeSwitchToTab(0));
    }

    @Test
    @SmallTest
    public void switchToTab_noTabManager() {
        // We have a tab, but no tab manager.
        doReturn(mTab).when(mAutocompleteController).getMatchingTabForSuggestion(anyInt());
        Assert.assertFalse(mMediator.maybeSwitchToTab(0));
    }

    @Test
    @SmallTest
    public void switchToTab_tabAttachedToStoppedActivity() {
        // We have a tab, and tab manager. The tab is part of the stopped activity.
        doReturn(mTab).when(mAutocompleteController).getMatchingTabForSuggestion(anyInt());
        mTabWindowManagerSupplier.set(mTabManager);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.STOPPED).when(mMockWindowAndroid).getActivityState();
        Assert.assertTrue(mMediator.maybeSwitchToTab(0));
    }

    @Test
    @SmallTest
    public void switchToTab_noTabModelForTab() {
        // We have a tab, and tab manager. The tab is part of the running activity.
        // The tab is not a part of the model though (eg. it has just been closed).
        // https://crbug.com/1300447
        doReturn(mTab).when(mAutocompleteController).getMatchingTabForSuggestion(anyInt());
        mTabWindowManagerSupplier.set(mTabManager);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.RESUMED).when(mMockWindowAndroid).getActivityState();
        doReturn(null).when(mTabManager).getTabModelForTab(any());
        Assert.assertFalse(mMediator.maybeSwitchToTab(0));
    }

    @Test
    @SmallTest
    public void switchToTab_invalidTabModelAssociation() {
        // We have a tab, and tab manager. The tab is part of the running activity.
        // The tab reports association with an existing model, but the model thinks otherwise.
        // https://crbug.com/1300447
        doReturn(mTab).when(mAutocompleteController).getMatchingTabForSuggestion(anyInt());
        mTabWindowManagerSupplier.set(mTabManager);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.RESUMED).when(mMockWindowAndroid).getActivityState();
        doReturn(mTabModel).when(mTabManager).getTabModelForTab(any());

        // Make sure that this indeed returns no association.
        Assert.assertEquals(
                TabModel.INVALID_TAB_INDEX, TabModelUtils.getTabIndexById(mTabModel, mTab.getId()));
        Assert.assertFalse(mMediator.maybeSwitchToTab(0));
    }

    @Test
    @SmallTest
    public void switchToTab_validTabModelAssociation() {
        // We have a tab, and tab manager. The tab is part of the running activity.
        // The tab reports association with an existing model; the model confirms this.
        doReturn(mTab).when(mAutocompleteController).getMatchingTabForSuggestion(anyInt());
        mTabWindowManagerSupplier.set(mTabManager);
        doReturn(mMockWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(ActivityState.RESUMED).when(mMockWindowAndroid).getActivityState();
        doReturn(mTabModel).when(mTabManager).getTabModelForTab(any());
        doReturn(1).when(mTabModel).getCount();
        doReturn(mTab).when(mTabModel).getTabAt(anyInt());
        Assert.assertTrue(mMediator.maybeSwitchToTab(0));
    }

    /**
     * Verify the values recorded by SuggestionList.RequestToUiModel.* histograms.
     * @param firstHistogramTotalCount total number of recorded values for the
     *         RequestToUiModel.First histogram
     * @param firstHistogramTime the value to expect to be recorded as RequestToUiModel.First, or
     *         null if this histogram should not be recorded
     * @param lastHistogramTotalCount total number of recorded values for the
     *         RequestToUiModel.Last histogram
     * @param lastHistogramTime the value to expect to be recorded as RequestToUiModel.Last, or
     *         null if this histogram should not be recorded
     */
    private void verifySuggestionRequestToUiModelHistograms(int firstHistogramTotalCount,
            @Nullable Integer firstHistogramTime, int lastHistogramTotalCount,
            @Nullable Integer lastHistogramTime) {
        Assert.assertEquals(firstHistogramTotalCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_FIRST));
        Assert.assertEquals(lastHistogramTotalCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_LAST));

        if (firstHistogramTime != null) {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_FIRST,
                            firstHistogramTime));
        }

        if (lastHistogramTime != null) {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            OmniboxMetrics.HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_LAST,
                            lastHistogramTime));
        }
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_recordedForZps() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onUrlFocusChange(true);
        verify(mAutocompleteController).startZeroSuggest("", url, pageClassification, title);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report first results. Observe first results histogram reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(100));
        mMediator.onSuggestionsReceived(mAutocompleteResult, "", /*isFinal=*/false);
        verifySuggestionRequestToUiModelHistograms(1, 100, 0, null);

        // Report next results. Observe first results histogram not reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(300));
        mMediator.onSuggestionsReceived(mAutocompleteResult, "", /*isFinal=*/false);
        verifySuggestionRequestToUiModelHistograms(1, 100, 0, null);

        // Report last results. Observe two histograms reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(100));
        mMediator.onSuggestionsReceived(mAutocompleteResult, "", /*isFinal=*/true);
        verifySuggestionRequestToUiModelHistograms(1, 100, 1, 500);
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_notRecordedWhenCanceled_LastResult() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onUrlFocusChange(true);
        verify(mAutocompleteController).startZeroSuggest("", url, pageClassification, title);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report first results. Observe first results histogram reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(10));
        mMediator.onSuggestionsReceived(mAutocompleteResult, "", /*isFinal=*/false);
        verifySuggestionRequestToUiModelHistograms(1, 10, 0, null);

        // Cancel the interaction.
        mMediator.onUrlFocusChange(false);

        // Report last results. Observe no final report.
        verifySuggestionRequestToUiModelHistograms(1, 10, 0, null);
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_notRecordedWhenCanceled_FirstResult() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onUrlFocusChange(true);
        verify(mAutocompleteController).startZeroSuggest("", url, pageClassification, title);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Cancel the interaction.
        mMediator.onUrlFocusChange(false);

        // Report first results. Observe no report (no focus).
        mMediator.onSuggestionsReceived(mAutocompleteResult, "", /*isFinal=*/false);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report last results. Observe no final report (no focus).
        mMediator.onSuggestionsReceived(mAutocompleteResult, "", /*isFinal=*/true);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_recordsBothHistogramsWhenFirstResponseIsFinal() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onUrlFocusChange(true);
        verify(mAutocompleteController).startZeroSuggest("", url, pageClassification, title);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report first result as final. Observe both metrics reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(150));
        mMediator.onSuggestionsReceived(mAutocompleteResult, "", /*isFinal=*/true);
        verifySuggestionRequestToUiModelHistograms(1, 150, 1, 150);
    }

    @Test
    @SmallTest
    public void requestToUiModelTime_subsequentKeyStrokesReportTimeSinceLastKeystroke() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);
        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");
        mMediator.onNativeInitialized();

        mMediator.onUrlFocusChange(true);
        verify(mAutocompleteController).startZeroSuggest("", url, pageClassification, title);
        verifySuggestionRequestToUiModelHistograms(0, null, 0, null);

        // Report first result as final. Observe both metrics reported.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(150));
        mMediator.onSuggestionsReceived(mAutocompleteResult, "", /*isFinal=*/false);
        verifySuggestionRequestToUiModelHistograms(1, 150, 0, null);

        // No change on key press. No unexpected recordings.
        // Need to run looper here to flush the pending operation.
        mMediator.onTextChanged("a", "a");
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verifySuggestionRequestToUiModelHistograms(1, 150, 0, null);

        // No change on key press. No unexpected recordings.
        ShadowPausedSystemClock.advanceBy(Duration.ofMillis(100));
        mMediator.onSuggestionsReceived(mAutocompleteResult, "", /*isFinal=*/true);
        verifySuggestionRequestToUiModelHistograms(2, 100, 1, 100);
    }
}
