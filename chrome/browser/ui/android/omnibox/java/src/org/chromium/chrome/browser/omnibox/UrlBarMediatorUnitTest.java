// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Color;
import android.text.Selection;
import android.text.SpannableStringBuilder;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.search_engines.settings.SiteSearchSettings;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.DisplayState;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer.UrlEmphasisColorSpan;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;
import org.chromium.url.GURL;

/** Unit tests for {@link UrlBarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UrlBarMediatorUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Clipboard mClipboard;
    @Mock private PropertyObserver mPropertyObserver;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private UrlBarDelegate mUrlBarDelegate;
    private Context mContext;
    private PropertyModel mModel;
    private UrlBarMediator mMediator;
    private UrlBarDelegate mDelegate;

    @Before
    public void setUp() {
        OmniboxResourceProvider.setUrlBarPrimaryTextColorForTesting(Color.LTGRAY);
        OmniboxResourceProvider.setUrlBarHintTextColorForTesting(Color.LTGRAY);
        Clipboard.setInstanceForTesting(mClipboard);
        mContext = ContextUtils.getApplicationContext();
        mDelegate = mUrlBarDelegate;
        mModel =
                new PropertyModel.Builder(UrlBarProperties.ALL_KEYS)
                        .with(UrlBarProperties.DELEGATE, mDelegate)
                        .build();
        mMediator =
                new UrlBarMediator(
                        ContextUtils.getApplicationContext(),
                        mModel,
                        /* textChangeListener= */ null,
                        /* richTextChangeListener= */ null) {
                    @Override
                    protected String sanitizeTextForPaste(String text) {
                        return text.trim();
                    }
                };
    }

    @Test
    @SuppressWarnings("unchecked")
    public void setUrlData_SendsUpdates() {
        UrlBarData baseData =
                UrlBarData.create(
                        /* url= */ new GURL("http://www.example.com"),
                        /* displayText= */ spannable("www.example.com"),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 14,
                        /* editingText= */ "Blah");
        UrlBarData dataWithDifferentDisplay =
                UrlBarData.create(
                        /* url= */ new GURL("http://www.example.com"),
                        /* displayText= */ spannable("www.foo.com"),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 11,
                        /* editingText= */ "Blah");
        UrlBarData dataWithDifferentEditing =
                UrlBarData.create(
                        /* url= */ new GURL("http://www.example.com"),
                        /* displayText= */ spannable("www.example.com"),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 14,
                        /* editingText= */ "Bar");

        assertTrue(
                mMediator.setUrlBarData(
                        baseData, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END));

        mModel.addObserver(mPropertyObserver);
        clearInvocations(mPropertyObserver);

        assertTrue(
                mMediator.setUrlBarData(
                        dataWithDifferentDisplay,
                        UrlBar.ScrollType.SCROLL_TO_TLD,
                        TextSelection.SELECT_END));
        assertTrue(
                mMediator.setUrlBarData(
                        dataWithDifferentEditing,
                        UrlBar.ScrollType.SCROLL_TO_TLD,
                        TextSelection.SELECT_END));
        assertTrue(
                mMediator.setUrlBarData(
                        dataWithDifferentEditing,
                        UrlBar.ScrollType.SCROLL_TO_BEGINNING,
                        TextSelection.SELECT_END));

        verify(mPropertyObserver, times(3)).onPropertyChanged(mModel, UrlBarProperties.TEXT_STATE);
    }

    @Test
    @SuppressWarnings("unchecked")
    public void setUrlData_PreventsDuplicateUpdates() {
        UrlBarData data1 =
                UrlBarData.create(
                        /* url= */ new GURL("http://www.example.com"),
                        /* displayText= */ spannable("www.example.com"),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 0,
                        /* editingText= */ "Blah");
        UrlBarData data2 =
                UrlBarData.create(
                        /* url= */ new GURL("http://www.example.com"),
                        /* displayText= */ spannable("www.example.com"),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 0,
                        /* editingText= */ "Blah");

        assertTrue(
                mMediator.setUrlBarData(
                        data1, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END));

        mModel.addObserver(mPropertyObserver);
        clearInvocations(mPropertyObserver);

        assertFalse(
                mMediator.setUrlBarData(
                        data1, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END));
        assertFalse(
                mMediator.setUrlBarData(
                        data2, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END));

        verify(mPropertyObserver, never()).onPropertyChanged(any(), any());
    }

    @Test
    public void setUrlData_ScrollStateForDataUrl() {
        String displayText = "data:text/html,blah";
        UrlBarData data =
                UrlBarData.create(
                        /* url= */ new GURL("data:text/html,blah,blah"),
                        /* displayText= */ spannable(displayText),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ displayText.length(),
                        /* editingText= */ null);
        assertTrue(
                mMediator.setUrlBarData(
                        data, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_ALL));

        // The scroll state should be overridden to SCROLL_TO_BEGINNING for file-type schemes.
        assertEquals(
                UrlBar.ScrollType.SCROLL_TO_BEGINNING,
                mModel.get(UrlBarProperties.TEXT_STATE).scrollType);
    }

    @Test
    public void setUrlData_ScrollStateForAboutUrl() {
        String displayText = "about:blank#verylongurl.totallylegit.notsuspicious.url.com";
        UrlBarData data =
                UrlBarData.create(
                        /* url= */ new GURL(displayText),
                        /* displayText= */ spannable(displayText),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ displayText.length(),
                        /* editingText= */ null);
        assertTrue(
                mMediator.setUrlBarData(
                        data, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_ALL));

        // The scroll state should be overridden to SCROLL_TO_BEGINNING for file-type schemes.
        assertEquals(
                UrlBar.ScrollType.SCROLL_TO_BEGINNING,
                mModel.get(UrlBarProperties.TEXT_STATE).scrollType);
    }

    @Test
    public void urlDataComparison_equals() {
        assertTrue(UrlBarMediator.isNewTextEquivalentToExistingText(null, null));

        // Empty display text, regardless of spanned state.
        assertTrue(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ spannable(""),
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ "",
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null)));

        // No editing text, equal display text
        assertTrue(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ spannable("Test"),
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ spannable("Test"),
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null)));

        // Equal display and editing text
        assertTrue(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ spannable("Test"),
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah"),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ spannable("Test"),
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah")));

        // Equal plain string display text
        assertTrue(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.forNonUrlText("Test"), UrlBarData.forNonUrlText("Test")));

        // Spanned (with no emphasis spans) vs plain string display text
        assertTrue(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ spannable("Test"),
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ "Test",
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null)));

        // Equal complex display text and editing text
        SpannableStringBuilder text1 = spannable("Test");
        text1.setSpan(new UrlEmphasisColorSpan(3), 0, 3, 0);
        text1.setSpan(new UrlEmphasisColorSpan(4), 1, 3, 0);
        text1.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisSecurityErrorSpan(), 0, 1, 0);

        SpannableStringBuilder text2 = spannable("Test");
        text2.setSpan(new UrlEmphasisColorSpan(3), 0, 3, 0);
        text2.setSpan(new UrlEmphasisColorSpan(4), 1, 3, 0);
        text2.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisSecurityErrorSpan(), 0, 1, 0);

        assertTrue(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ text1,
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah"),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ text2,
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah")));

        // Ensure adding non-emphasis spans does not mess up equality.
        text1.setSpan(new Object(), 0, 3, 0);
        Selection.setSelection(text2, 0, 1);
        assertTrue(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ text1,
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah"),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ text2,
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah")));
    }

    @Test
    public void urlDataComparison_notEquals() {
        assertFalse(UrlBarMediator.isNewTextEquivalentToExistingText(null, UrlBarData.EMPTY));
        assertFalse(UrlBarMediator.isNewTextEquivalentToExistingText(UrlBarData.EMPTY, null));

        // Different display texts
        assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ spannable("Test"),
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ spannable("Test2"),
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null)));

        // Mismatched spannable state of display text
        assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ spannable("Test"),
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ "Test2",
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null)));

        // Spanned with emphasis spans vs plain string display text
        SpannableStringBuilder textWithSpan = spannable("Test");
        textWithSpan.setSpan(new UrlEmphasisColorSpan(3), 0, 3, 0);
        assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ textWithSpan,
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ "Test",
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ null)));

        // Equal display text, different editing text
        assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ spannable("Test"),
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah"),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ spannable("Test"),
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah2")));

        // Equal display text content, but different emphasis spans
        SpannableStringBuilder text1 = spannable("Test");
        SpannableStringBuilder text2 = spannable("Test");
        text2.setSpan(new UrlEmphasisColorSpan(3), 0, 3, 0);
        text2.setSpan(new UrlEmphasisColorSpan(4), 1, 3, 0);
        text2.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisSecurityErrorSpan(), 0, 1, 0);

        assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ text1,
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah"),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ text2,
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah")));

        // Add a subset of emphasis spans, but not all.
        text1.setSpan(new UrlEmphasisColorSpan(3), 0, 3, 0);
        text1.setSpan(new UrlEmphasisColorSpan(4), 1, 3, 0);
        assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ text1,
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah"),
                        UrlBarData.create(
                                /* url= */ null,
                                /* displayText= */ text2,
                                /* originStartIndex= */ 0,
                                /* originEndIndex= */ 0,
                                /* editingText= */ "Blah")));
    }

    @Test
    public void pasteTextValidation() {
        doReturn(null).when(mClipboard).getCoercedText();
        assertNull(mMediator.getTextToPaste());

        doReturn("").when(mClipboard).getCoercedText();
        assertEquals("", mMediator.getTextToPaste());

        doReturn("test").when(mClipboard).getCoercedText();
        assertEquals("test", mMediator.getTextToPaste());

        doReturn("    test     ").when(mClipboard).getCoercedText();
        assertEquals("test", mMediator.getTextToPaste());
    }

    @Test
    public void cutCopyReplacementTextValidation() {
        String url = "https://www.test.com/blah";
        String displayText = "test.com/blah";
        String editingText = "www.test.com/blah";
        mMediator.setUrlBarData(
                UrlBarData.create(
                        /* url= */ new GURL(url),
                        /* displayText= */ displayText,
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 12,
                        /* editingText= */ editingText),
                UrlBar.ScrollType.NO_SCROLL,
                TextSelection.SELECT_ALL);

        // Replacement is only valid if selecting the full text.
        assertNull(mMediator.getReplacementCutCopyText(editingText, new TextSelection(1, 2)));

        // Editing text will be replaced with the full URL if selecting all of the text.
        assertEquals(
                url,
                mMediator.getReplacementCutCopyText(
                        editingText, new TextSelection(0, editingText.length())));

        // If selecting just the URL portion of the editing text, it should be replaced with the
        // unformatted URL.
        assertEquals(
                "https://www.test.com",
                mMediator.getReplacementCutCopyText(editingText, new TextSelection(0, 12)));

        // If the path changed in the editing text changed but the domain is untouched, it should
        // be replaced with the full domain from the unformatted URL.
        assertEquals(
                "https://www.test.com/foo",
                mMediator.getReplacementCutCopyText("www.test.com/foo", new TextSelection(0, 16)));
    }

    @Test
    public void cutCopyReplacementTextValidation_ReverseSelection() {
        String url = "https://www.test.com/blah";
        String displayText = "test.com/blah";
        String editingText = "www.test.com/blah";
        mMediator.setUrlBarData(
                UrlBarData.create(
                        /* url= */ new GURL(url),
                        /* displayText= */ displayText,
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 12,
                        /* editingText= */ editingText),
                UrlBar.ScrollType.NO_SCROLL,
                TextSelection.SELECT_ALL);

        // Reverse selection of full text should still be replaced with full URL.
        assertEquals(
                url,
                mMediator.getReplacementCutCopyText(
                        editingText, new TextSelection(editingText.length(), 0)));

        // Reverse selection of URL portion should still be replaced with unformatted URL.
        assertEquals(
                "https://www.test.com",
                mMediator.getReplacementCutCopyText(editingText, new TextSelection(12, 0)));
    }

    @Test
    public void setUrlBarHintText() {
        mMediator.setUrlBarHintText("Hint 1");
        assertEquals("Hint 1", mModel.get(UrlBarProperties.HINT_TEXT));
        mMediator.setUrlBarHintText("Incognito Hint");
        assertEquals("Incognito Hint", mModel.get(UrlBarProperties.HINT_TEXT));
    }

    @Test
    public void hintVisibility() {
        var sessionState = new FuseboxSessionState();
        UrlBarData baseData =
                UrlBarData.create(
                        /* url= */ new GURL("http://www.example.com"),
                        /* displayText= */ spannable("www.example.com"),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 14,
                        /* editingText= */ "Blah");
        mMediator.setUrlBarHintText("Hint 1");
        assertTrue(mModel.get(UrlBarProperties.SHOW_HINT_TEXT));
        doReturn(baseData).when(mDelegate).getUrlBarDataForCurrentInput();

        mMediator.beginInput(sessionState);
        mModel.get(UrlBarProperties.TEXT_CHANGE_LISTENER).onResult("");

        assertTrue(mModel.get(UrlBarProperties.SHOW_HINT_TEXT));

        mModel.get(UrlBarProperties.TEXT_CHANGE_LISTENER).onResult("f");
        assertFalse(mModel.get(UrlBarProperties.SHOW_HINT_TEXT));
        mMediator.setUrlBarData(UrlBarData.EMPTY, ScrollType.NO_SCROLL, TextSelection.SELECT_END);
        assertTrue(mModel.get(UrlBarProperties.SHOW_HINT_TEXT));

        mMediator.endInput();
        assertTrue(mModel.get(UrlBarProperties.SHOW_HINT_TEXT));
    }

    @Test
    public void setShowOriginOnly() {
        UrlBarData baseData =
                UrlBarData.create(
                        /* url= */ new GURL("http://www.example.com/a_path_to_ignore"),
                        /* displayText= */ spannable("http://www.example.com/a_path_to_ignore"),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 22,
                        /* editingText= */ "Blah");
        mMediator.setUrlBarData(
                baseData, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END);

        assertEquals(
                "http://www.example.com/a_path_to_ignore",
                mModel.get(UrlBarProperties.TEXT_STATE).text.toString());

        mMediator.setShowOriginOnly(true);
        assertEquals(
                "http://www.example.com", mModel.get(UrlBarProperties.TEXT_STATE).text.toString());

        mMediator.setShowOriginOnly(false);
        assertEquals(
                "http://www.example.com/a_path_to_ignore",
                mModel.get(UrlBarProperties.TEXT_STATE).text.toString());
    }

    @Test
    public void setShowOriginOnly_nonUrlText() {
        UrlBarData baseData = UrlBarData.forNonUrlText("non url");
        mMediator.setUrlBarData(baseData, ScrollType.NO_SCROLL, TextSelection.SELECT_END);
        assertEquals("non url", mModel.get(UrlBarProperties.TEXT_STATE).text.toString());

        mMediator.setShowOriginOnly(true);
        assertEquals("non url", mModel.get(UrlBarProperties.TEXT_STATE).text.toString());
    }

    @Test
    public void crossOriginNavigation() {
        UrlBarData baseData =
                UrlBarData.create(
                        /* url= */ new GURL("http://www.example.com"),
                        /* displayText= */ spannable("www.example.com"),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 14,
                        /* editingText= */ "Blah");
        UrlBarData dataWithSameDomain =
                UrlBarData.create(
                        /* url= */ new GURL("http://www.example.com/bar"),
                        /* displayText= */ spannable("www.example.com/bar"),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 14,
                        /* editingText= */ "Blah");
        UrlBarData dataWithDifferentDomain =
                UrlBarData.create(
                        /* url= */ new GURL("http://www.example.com.subdomain"),
                        /* displayText= */ spannable("www.example.com.subdomain"),
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 20,
                        /* editingText= */ "Blah");

        assertTrue(
                mMediator.setUrlBarData(
                        baseData, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END));
        assertTrue(
                mMediator.setUrlBarData(
                        dataWithSameDomain,
                        UrlBar.ScrollType.SCROLL_TO_TLD,
                        TextSelection.SELECT_END));
        assertFalse(mModel.get(UrlBarProperties.TEXT_STATE).originChanged);
        assertTrue(
                mMediator.setUrlBarData(
                        dataWithDifferentDomain,
                        UrlBar.ScrollType.SCROLL_TO_TLD,
                        TextSelection.SELECT_END));
        assertTrue(mModel.get(UrlBarProperties.TEXT_STATE).originChanged);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_SITE_SEARCH)
    public void testManageSearchEnginesCallback_tablet_featureEnabled() {
        RuntimeEnvironment.setQualifiers("sw600dp");
        mMediator =
                new UrlBarMediator(
                        mContext,
                        mModel,
                        /* textChangeListener= */ null,
                        /* richTextChangeListener= */ null);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        Runnable callback = mModel.get(UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK);
        assertNotNull(callback);

        callback.run();
        verify(mSettingsNavigation).startSettings(eq(mContext), eq(SiteSearchSettings.class));
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_SITE_SEARCH)
    public void testManageSearchEnginesCallback_tablet_featureDisabled() {
        RuntimeEnvironment.setQualifiers("sw600dp");
        mMediator =
                new UrlBarMediator(
                        mContext,
                        mModel,
                        /* textChangeListener= */ null,
                        /* richTextChangeListener= */ null);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        Runnable callback = mModel.get(UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK);
        assertNotNull(callback);

        callback.run();
        verify(mSettingsNavigation).startSettings(eq(mContext), eq(SearchEngineSettings.class));
    }

    @Test
    public void testManageSearchEnginesCallback_phone() {
        RuntimeEnvironment.setQualifiers("sw360dp");
        mMediator =
                new UrlBarMediator(
                        mContext,
                        mModel,
                        /* textChangeListener= */ null,
                        /* richTextChangeListener= */ null);
        Runnable callback = mModel.get(UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK);
        assertNull(callback);
    }

    @Test
    public void testPushCurrentInputToModel_withDelegate() {
        UrlBarData mockData = UrlBarData.forNonUrlText("Text");
        doReturn(mockData).when(mDelegate).getUrlBarDataForCurrentInput();

        var sessionState = new FuseboxSessionState();
        mMediator.beginInput(sessionState);

        verify(mDelegate).getUrlBarDataForCurrentInput();
        assertEquals("Text", mModel.get(UrlBarProperties.TEXT_STATE).text.toString());
    }

    @Test
    public void onTextChanged_synchronizesUrlBarDataInInputSession() {
        UrlBarData initialData = UrlBarData.forNonUrlText("initial");
        doReturn(initialData).when(mDelegate).getUrlBarDataForCurrentInput();

        var sessionState = new FuseboxSessionState();
        mMediator.beginInput(sessionState);
        assertEquals("initial", mMediator.getUrlBarData().displayText.toString());

        // Typing updates mUrlBarData when in input session.
        mModel.get(UrlBarProperties.TEXT_CHANGE_LISTENER).onResult("typed text");
        assertEquals("typed text", mMediator.getUrlBarData().displayText.toString());

        mMediator.endInput();

        // Typing does NOT update mUrlBarData when not in input session.
        mModel.get(UrlBarProperties.TEXT_CHANGE_LISTENER).onResult("after session");
        assertEquals("", mMediator.getUrlBarData().displayText.toString());
    }

    @Test
    public void onTextChanged_synchronizesSelectionInInputSession() {
        var sessionState = new FuseboxSessionState();
        UrlBarData initialData = UrlBarData.forNonUrlText("initial");
        doReturn(initialData).when(mDelegate).getUrlBarDataForCurrentInput();

        mMediator.beginInput(sessionState);

        // Simulate typing and moving cursor to selection (1, 1).
        TextSelection newSelection = new TextSelection(1, 1);
        sessionState.getAutocompleteInput().setUserText("typed", newSelection);
        mModel.get(UrlBarProperties.TEXT_CHANGE_LISTENER).onResult("typed");

        // Subsequent setUrlBarData with matching text and selection should be deduplicated.
        assertFalse(
                mMediator.setUrlBarData(
                        UrlBarData.forNonUrlText("typed"),
                        ScrollType.SCROLL_TO_BEGINNING,
                        newSelection));
    }

    @Test
    public void onTextChanged_synchronizesSelectionUpdatedByListener() {
        var sessionState = new FuseboxSessionState();
        UrlBarData initialData = UrlBarData.forNonUrlText("initial");
        doReturn(initialData).when(mDelegate).getUrlBarDataForCurrentInput();

        TextSelection typedSelection = new TextSelection(5, 5);
        mMediator =
                new UrlBarMediator(
                        ContextUtils.getApplicationContext(),
                        mModel,
                        text ->
                                sessionState
                                        .getAutocompleteInput()
                                        .setUserText(text, typedSelection),
                        /* richTextChangeListener= */ null);
        mMediator.beginInput(sessionState);

        // User types "hello", which triggers textChangeListener to update AutocompleteInput
        // selection.
        mModel.get(UrlBarProperties.TEXT_CHANGE_LISTENER).onResult("hello");

        // setUrlBarData with matching text and selection should be deduplicated.
        assertFalse(
                mMediator.setUrlBarData(
                        UrlBarData.forNonUrlText("hello"),
                        ScrollType.SCROLL_TO_BEGINNING,
                        typedSelection));
    }

    @Test
    public void onTextChanged_synchronizesRangeSelection() {
        var sessionState = new FuseboxSessionState();
        UrlBarData initialData = UrlBarData.forNonUrlText("initial");
        doReturn(initialData).when(mDelegate).getUrlBarDataForCurrentInput();

        TextSelection rangeSelection = new TextSelection(2, 5);
        mMediator =
                new UrlBarMediator(
                        ContextUtils.getApplicationContext(),
                        mModel,
                        text ->
                                sessionState
                                        .getAutocompleteInput()
                                        .setUserText(text, rangeSelection),
                        /* richTextChangeListener= */ null);
        mMediator.beginInput(sessionState);

        mModel.get(UrlBarProperties.TEXT_CHANGE_LISTENER).onResult("selected");

        assertFalse(
                mMediator.setUrlBarData(
                        UrlBarData.forNonUrlText("selected"),
                        ScrollType.SCROLL_TO_BEGINNING,
                        rangeSelection));
        assertTrue(
                mMediator.setUrlBarData(
                        UrlBarData.forNonUrlText("selected"),
                        ScrollType.SCROLL_TO_BEGINNING,
                        new TextSelection(0, 0)));
    }

    @Test
    public void setUrlBarData_inInputSession_selectionEquivalence() {
        var sessionState = new FuseboxSessionState();
        UrlBarData data = UrlBarData.forNonUrlText("test");
        doReturn(data).when(mDelegate).getUrlBarDataForCurrentInput();

        mMediator.beginInput(sessionState);
        assertTrue(
                mMediator.setUrlBarData(
                        data, UrlBar.ScrollType.NO_SCROLL, new TextSelection(0, 4)));

        // Same text, same scroll type, same selection -> deduplicated (false).
        assertFalse(
                mMediator.setUrlBarData(
                        data, UrlBar.ScrollType.NO_SCROLL, new TextSelection(0, 4)));

        // Same text, same scroll type, different selection -> not deduplicated (true).
        assertTrue(
                mMediator.setUrlBarData(
                        data, UrlBar.ScrollType.NO_SCROLL, new TextSelection(2, 2)));
    }

    @Test
    public void setUrlBarData_emptyDisplayText_scrollTypeEquivalent() {
        UrlBarData nonEmpty = UrlBarData.forNonUrlText("initial");
        assertTrue(
                mMediator.setUrlBarData(
                        nonEmpty, UrlBar.ScrollType.NO_SCROLL, TextSelection.SELECT_END));

        UrlBarData empty1 =
                UrlBarData.create(
                        /* url= */ null,
                        /* displayText= */ "",
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 0,
                        /* editingText= */ null);
        UrlBarData empty2 =
                UrlBarData.create(
                        /* url= */ null,
                        /* displayText= */ "",
                        /* originStartIndex= */ 0,
                        /* originEndIndex= */ 0,
                        /* editingText= */ null);

        assertTrue(
                mMediator.setUrlBarData(
                        empty1, UrlBar.ScrollType.NO_SCROLL, TextSelection.SELECT_END));

        // Different scroll type, but both texts are empty -> deduplicated (false).
        assertFalse(
                mMediator.setUrlBarData(
                        empty2, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END));
    }

    @Test
    public void endInput_clearsSessionBeforeResettingText() {
        var session = new FuseboxSessionState();
        var input = new AutocompleteInput(OmniboxFocusReason.DEFAULT_WITH_HARDWARE_KEYBOARD);
        input.setUserText("typed text", TextSelection.SELECT_END);
        session.applyAutocompleteInput(input);

        doReturn(UrlBarData.forNonUrlText("typed text"))
                .when(mDelegate)
                .getUrlBarDataForCurrentInput();

        mMediator.beginInput(session);
        assertTrue(mMediator.isInInputSession());

        mMediator.endInput();
        assertFalse(mMediator.isInInputSession());
    }

    @Test
    public void testAllowMultilineInput_drivenByDisplayState() {
        assertFalse(mModel.get(UrlBarProperties.ALLOW_MULTILINE_INPUT));

        var session = new FuseboxSessionState();
        var input = session.getAutocompleteInput();
        doReturn(UrlBarData.EMPTY).when(mDelegate).getUrlBarDataForCurrentInput();

        mMediator.beginInput(session);
        assertFalse(mModel.get(UrlBarProperties.ALLOW_MULTILINE_INPUT));

        input.setDisplayState(DisplayState.DRAFTING);
        assertFalse(mModel.get(UrlBarProperties.ALLOW_MULTILINE_INPUT));

        input.setDisplayState(DisplayState.DRAFTING_NO_FOCUS);
        assertFalse(mModel.get(UrlBarProperties.ALLOW_MULTILINE_INPUT));

        input.setDisplayState(DisplayState.WEBSITE);
        assertFalse(mModel.get(UrlBarProperties.ALLOW_MULTILINE_INPUT));

        input.setDisplayState(DisplayState.SUGGESTIONS);
        assertTrue(mModel.get(UrlBarProperties.ALLOW_MULTILINE_INPUT));

        input.setDisplayState(DisplayState.DRAFTING_NO_FOCUS);
        assertFalse(mModel.get(UrlBarProperties.ALLOW_MULTILINE_INPUT));

        mMediator.endInput();
        assertFalse(mModel.get(UrlBarProperties.ALLOW_MULTILINE_INPUT));

        input.setDisplayState(DisplayState.SUGGESTIONS);
        assertFalse(mModel.get(UrlBarProperties.ALLOW_MULTILINE_INPUT));
    }

    private static SpannableStringBuilder spannable(String text) {
        return new SpannableStringBuilder(text);
    }
}
