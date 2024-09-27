// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.ClipboardImpl;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Optional;

/** Unit tests for the "edit url" omnibox suggestion. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {EditUrlSuggestionProcessorUnitTest.ShadowSadTab.class})
public final class EditUrlSuggestionProcessorUnitTest {
    private static final String TAB_TITLE = "Tab Title";
    private static final String MATCH_TITLE = "Match Title";
    private static final int ACTION_SHARE = 0;
    private static final int ACTION_COPY = 1;
    private static final int ACTION_EDIT = 2;
    private static final GURL SEARCH_URL_1 = JUnitTestGURLs.SEARCH_URL;
    private static final GURL SEARCH_URL_2 = JUnitTestGURLs.SEARCH_2_URL;
    private static final GURL INVALID_ESCAPED_PATH_URL =
            new GURL("https://pl.wikipedia.org/wiki/G%X");

    public static final String ESCAPED_PATH_URL_STRING = "https://pl.wikipedia.org/wiki/Gżegżółka";
    public static final GURL ESCAPED_PATH_URL =
            new GURL("https://pl.wikipedia.org/wiki/G%C5%BCeg%C5%BC%C3%B3%C5%82ka");

    /** Used to simulate sad tabs. */
    @Implements(SadTab.class)
    static class ShadowSadTab {
        public static boolean reportSadTab;

        @Implementation
        public static boolean isShowing(Tab t) {
            return reportSadTab;
        }
    }

    public @Rule JniMocker mJniMocker = new JniMocker();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock ShareDelegate mShareDelegate;
    private @Mock Tab mTab;
    private @Mock OmniboxImageSupplier mImageSupplier;
    private @Mock SuggestionHost mSuggestionHost;
    private @Mock ClipboardManager mClipboardManager;
    private @Mock WebContents mWebContents;
    private @Mock Supplier<Tab> mTabSupplier;
    private @Mock Supplier<ShareDelegate> mShareDelegateSupplier;
    private @Mock UkmRecorder.Natives mUkmRecorderJniMock;

    // The original (real) ClipboardManager to be restored after a test run.
    private Context mContext;
    private AutocompleteMatch mMatch;
    private ClipboardManager mOldClipboardManager;
    private EditUrlSuggestionProcessor mProcessor;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mJniMocker.mock(UkmRecorderJni.TEST_HOOKS, mUkmRecorderJniMock);

        mOldClipboardManager =
                ((ClipboardImpl) Clipboard.getInstance())
                        .overrideClipboardManagerForTesting(mClipboardManager);

        mContext = ContextUtils.getApplicationContext();
        mMatch =
                new AutocompleteMatchBuilder(OmniboxSuggestionType.URL_WHAT_YOU_TYPED)
                        .setIsSearch(false)
                        .setDisplayText(SEARCH_URL_1.getSpec())
                        .setDescription(MATCH_TITLE)
                        .setUrl(SEARCH_URL_1)
                        .build();

        mProcessor =
                new EditUrlSuggestionProcessor(
                        mContext,
                        mSuggestionHost,
                        Optional.of(mImageSupplier),
                        mTabSupplier,
                        mShareDelegateSupplier);
        mModel = mProcessor.createModel();

        doReturn(mTab).when(mTabSupplier).get();
        doReturn(mShareDelegate).when(mShareDelegateSupplier).get();
        doReturn(SEARCH_URL_1).when(mTab).getUrl();
        doReturn(TAB_TITLE).when(mTab).getTitle();
        doReturn(true).when(mTab).isInitialized();

        mProcessor.onOmniboxSessionStateChange(true);
    }

    @After
    public void tearDown() {
        mProcessor.onOmniboxSessionStateChange(false);
        ((ClipboardImpl) Clipboard.getInstance())
                .overrideClipboardManagerForTesting(mOldClipboardManager);
    }

    @Test
    public void doesProcessSuggestion_rejectNonMatchingUrlWhatYouTyped() {
        var match =
                new AutocompleteMatchBuilder(OmniboxSuggestionType.URL_WHAT_YOU_TYPED)
                        .setUrl(SEARCH_URL_1)
                        .build();

        // URLs don't match - this suggestion should be ignored.
        doReturn(SEARCH_URL_2).when(mTab).getUrl();
        assertFalse(mProcessor.doesProcessSuggestion(match, 0));
        verifyNoMoreInteractions(mSuggestionHost, mShareDelegate, mClipboardManager);
    }

    @Test
    public void doesProcessSuggestion_acceptMatchingUrlWhatYouTyped() {
        assertTrue(mProcessor.doesProcessSuggestion(mMatch, 0));
        verifyNoMoreInteractions(mSuggestionHost, mShareDelegate, mClipboardManager);
    }

    @Test
    public void
            doesProcessSuggestion_acceptMatchingUrlWhatYouTypedWhenRetainOmniboxOnFocusDisabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.FALSE);
        assertTrue(mProcessor.doesProcessSuggestion(mMatch, 0));
        verifyNoMoreInteractions(mSuggestionHost, mShareDelegate, mClipboardManager);
    }

    @Test
    public void doesProcessSuggestion_rejectMatchWhenTabIsMissing() {
        doReturn(null).when(mTabSupplier).get();
        assertFalse(mProcessor.doesProcessSuggestion(mMatch, 0));
        verifyNoMoreInteractions(mSuggestionHost, mShareDelegate, mClipboardManager);
    }

    @Test
    public void doesProcessSuggestion_rejectMatchForNativeTab() {
        doReturn(true).when(mTab).isNativePage();
        assertFalse(mProcessor.doesProcessSuggestion(mMatch, 0));
        verifyNoMoreInteractions(mSuggestionHost, mShareDelegate, mClipboardManager);
    }

    @Test
    public void doesProcessSuggestion_rejectMatchForSadTab() {
        ShadowSadTab.reportSadTab = true;
        assertFalse(mProcessor.doesProcessSuggestion(mMatch, 0));
        verifyNoMoreInteractions(mSuggestionHost, mShareDelegate, mClipboardManager);
    }

    @Test
    public void doesProcessSuggestion_rejectMatchForUninitializedTab() {
        doReturn(false).when(mTab).isInitialized();
        assertFalse(mProcessor.doesProcessSuggestion(mMatch, 0));
        verifyNoMoreInteractions(mSuggestionHost, mShareDelegate, mClipboardManager);
    }

    @Test
    public void doesProcessSuggestion_rejectNonTopMatch() {
        assertFalse(mProcessor.doesProcessSuggestion(mMatch, 1));
        verifyNoMoreInteractions(mSuggestionHost, mShareDelegate, mClipboardManager);
    }

    @Test
    public void doesProcessSuggestion_rejectSearchWhatYouTyped() {
        var match =
                new AutocompleteMatchBuilder(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED)
                        .setUrl(SEARCH_URL_1)
                        .build();
        // Suggestion should be rejected even though URLs match.
        when(mTab.getUrl()).thenReturn(SEARCH_URL_1);
        assertFalse(mProcessor.doesProcessSuggestion(match, 0));
        verifyNoMoreInteractions(mSuggestionHost, mShareDelegate, mClipboardManager);
    }

    @Test
    public void doesProcessSuggestion_rejectMatchWhenRetainOmniboxOnFocusEnabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.TRUE);
        assertFalse(mProcessor.doesProcessSuggestion(mMatch, 0));
        verifyNoMoreInteractions(mSuggestionHost, mShareDelegate, mClipboardManager);
    }

    @Test
    public void populateModel_showInformationFromLoadedTab() {
        mProcessor.populateModel(mMatch, mModel, 0);

        assertEquals(3, mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS).size());
        assertEquals(TAB_TITLE, mModel.get(SuggestionViewProperties.TEXT_LINE_1_TEXT).toString());
        assertEquals(
                SEARCH_URL_1.getSpec(),
                mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT).toString());
    }

    @Test
    public void populateModel_substituteMatchInformationForLoadingTab() {
        doReturn(true).when(mTab).isLoading();
        mProcessor.populateModel(mMatch, mModel, 0);
        assertEquals(MATCH_TITLE, mModel.get(SuggestionViewProperties.TEXT_LINE_1_TEXT).toString());
    }

    @Test
    public void populateModel_substituteFallbackInformationForLoadingTab() {
        mMatch =
                new AutocompleteMatchBuilder(OmniboxSuggestionType.URL_WHAT_YOU_TYPED)
                        .setDisplayText(SEARCH_URL_1.getSpec())
                        .setDescription("")
                        .setUrl(SEARCH_URL_1)
                        .build();
        doReturn(true).when(mTab).isLoading();
        mProcessor.populateModel(mMatch, mModel, 0);
        assertEquals(
                mContext.getResources().getText(R.string.tab_loading_default_title),
                mModel.get(SuggestionViewProperties.TEXT_LINE_1_TEXT).toString());
    }

    @Test
    public void shareButton_click() {
        mProcessor.populateModel(mMatch, mModel, 0);

        var monitor = new UserActionTester();
        mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS).get(ACTION_SHARE).callback.run();
        verify(mSuggestionHost).finishInteraction();
        verify(mShareDelegate, times(1))
                .share(mTab, /* shareDirectly= */ false, ShareOrigin.EDIT_URL);
        // Note: UkmRecorder requires WebContents to report metrics.
        // In the even WebContents is not available, we should not interact with UkmRecorder.
        verifyNoMoreInteractions(mUkmRecorderJniMock);

        assertEquals(1, monitor.getActionCount("Omnibox.EditUrlSuggestion.Share"));
        assertEquals(1, monitor.getActions().size());
        monitor.tearDown();
    }

    @Test
    public void shareButton_click_reportsUkmEvent() {
        doReturn(mWebContents).when(mTab).getWebContents();

        mProcessor.populateModel(mMatch, mModel, 0);
        mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS).get(ACTION_SHARE).callback.run();
        verify(mSuggestionHost).finishInteraction();

        verify(mShareDelegate).share(mTab, /* shareDirectly= */ false, ShareOrigin.EDIT_URL);
        verify(mUkmRecorderJniMock)
                .recordEventWithBooleanMetric(
                        any(), eq("Omnibox.EditUrlSuggestion.Share"), eq("HasOccurred"));
    }

    @Test
    public void suggestionView_clickReloadsPage() {
        mProcessor.populateModel(mMatch, mModel, 0);

        var monitor = new UserActionTester();
        mModel.get(BaseSuggestionViewProperties.ON_CLICK).run();
        verify(mSuggestionHost).onSuggestionClicked(mMatch, 0, mMatch.getUrl());
        verifyNoMoreInteractions(mSuggestionHost);

        assertEquals(1, monitor.getActionCount("Omnibox.EditUrlSuggestion.Tap"));
        assertEquals(1, monitor.getActions().size());
        monitor.tearDown();
    }

    @Test
    public void copyButton_click() {
        mProcessor.populateModel(mMatch, mModel, 0);
        var monitor = new UserActionTester();
        mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS).get(ACTION_COPY).callback.run();

        ArgumentCaptor<ClipData> argument = ArgumentCaptor.forClass(ClipData.class);
        verify(mClipboardManager, times(1)).setPrimaryClip(argument.capture());

        // ClipData doesn't implement equals, but their string representations matching should be
        // good enough.
        ClipData clip =
                new ClipData(
                        "url",
                        new String[] {"text/x-moz-url", "text/plain"},
                        new ClipData.Item(SEARCH_URL_1.getSpec()));
        assertEquals(clip.toString(), argument.getValue().toString());
        verifyNoMoreInteractions(mSuggestionHost, mShareDelegate, mClipboardManager);

        assertEquals(1, monitor.getActionCount("Omnibox.EditUrlSuggestion.Copy"));
        assertEquals(1, monitor.getActions().size());
        monitor.tearDown();
    }

    @Test
    public void getViewTypeId_forFullTestCoverage() {
        assertEquals(OmniboxSuggestionUiType.EDIT_URL_SUGGESTION, mProcessor.getViewTypeId());
    }
}
