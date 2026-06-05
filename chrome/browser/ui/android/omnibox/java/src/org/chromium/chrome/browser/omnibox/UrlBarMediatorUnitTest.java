// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.ClipData;
import android.content.ClipboardManager;
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
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.search_engines.settings.SiteSearchSettings;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer.UrlEmphasisColorSpan;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;
import org.chromium.url.GURL;

/** Unit tests for {@link UrlBarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UrlBarMediatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock Callback<Boolean> mFocusChangeCallback;

    Context mContext;
    PropertyModel mModel;
    UrlBarMediator mMediator;

    @Before
    public void setUp() {
        OmniboxResourceProvider.setUrlBarPrimaryTextColorForTesting(Color.LTGRAY);
        OmniboxResourceProvider.setUrlBarHintTextColorForTesting(Color.LTGRAY);
        mContext = ContextUtils.getApplicationContext();
        mModel = new PropertyModel(UrlBarProperties.ALL_KEYS);
        mMediator =
                new UrlBarMediator(
                        ContextUtils.getApplicationContext(),
                        mModel,
                        mFocusChangeCallback,
                        /* textChangeListener= */ null,
                        /* richTextChangeListener= */ null,
                        /* keyDownListener= */ null) {
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
                        new GURL("http://www.example.com"),
                        spannable("www.example.com"),
                        0,
                        14,
                        "Blah");
        UrlBarData dataWithDifferentDisplay =
                UrlBarData.create(
                        new GURL("http://www.example.com"),
                        spannable("www.foo.com"),
                        0,
                        11,
                        "Blah");
        UrlBarData dataWithDifferentEditing =
                UrlBarData.create(
                        new GURL("http://www.example.com"),
                        spannable("www.example.com"),
                        0,
                        14,
                        "Bar");

        assertTrue(
                mMediator.setUrlBarData(
                        baseData, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END));

        PropertyObserver<PropertyKey> observer = mock(PropertyObserver.class);
        mModel.addObserver(observer);
        reset(observer);

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

        verify(observer, times(3)).onPropertyChanged(mModel, UrlBarProperties.TEXT_STATE);
    }

    @Test
    @SuppressWarnings("unchecked")
    public void setUrlData_PreventsDuplicateUpdates() {
        UrlBarData data1 =
                UrlBarData.create(
                        new GURL("http://www.example.com"),
                        spannable("www.example.com"),
                        0,
                        0,
                        "Blah");
        UrlBarData data2 =
                UrlBarData.create(
                        new GURL("http://www.example.com"),
                        spannable("www.example.com"),
                        0,
                        0,
                        "Blah");

        assertTrue(
                mMediator.setUrlBarData(
                        data1, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END));

        PropertyObserver<PropertyKey> observer = mock(PropertyObserver.class);
        mModel.addObserver(observer);
        reset(observer);

        assertFalse(
                mMediator.setUrlBarData(
                        data1, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END));
        assertFalse(
                mMediator.setUrlBarData(
                        data2, UrlBar.ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END));

        verifyNoMoreInteractions(observer);
    }

    @Test
    public void setUrlData_ScrollStateForDataUrl() {
        String displayText = "data:text/html,blah";
        UrlBarData data =
                UrlBarData.create(
                        new GURL("data:text/html,blah,blah"),
                        spannable(displayText),
                        0,
                        displayText.length(),
                        null);
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
                        new GURL(displayText),
                        spannable(displayText),
                        0,
                        displayText.length(),
                        null);
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
                        UrlBarData.create(null, spannable(""), 0, 0, null),
                        UrlBarData.create(null, "", 0, 0, null)));

        // No editing text, equal display text
        assertTrue(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(null, spannable("Test"), 0, 0, null),
                        UrlBarData.create(null, spannable("Test"), 0, 0, null)));

        // Equal display and editing text
        assertTrue(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(null, spannable("Test"), 0, 0, "Blah"),
                        UrlBarData.create(null, spannable("Test"), 0, 0, "Blah")));

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
                        UrlBarData.create(null, text1, 0, 0, "Blah"),
                        UrlBarData.create(null, text2, 0, 0, "Blah")));

        // Ensure adding non-emphasis spans does not mess up equality.
        text1.setSpan(new Object(), 0, 3, 0);
        Selection.setSelection(text2, 0, 1);
        assertTrue(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(null, text1, 0, 0, "Blah"),
                        UrlBarData.create(null, text2, 0, 0, "Blah")));
    }

    @Test
    public void urlDataComparison_notEquals() {
        assertFalse(UrlBarMediator.isNewTextEquivalentToExistingText(null, UrlBarData.EMPTY));
        assertFalse(UrlBarMediator.isNewTextEquivalentToExistingText(UrlBarData.EMPTY, null));

        // Different display texts
        assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(null, spannable("Test"), 0, 0, null),
                        UrlBarData.create(null, spannable("Test2"), 0, 0, null)));

        // Mismatched spannable state of display text
        assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(null, spannable("Test"), 0, 0, null),
                        UrlBarData.create(null, "Test2", 0, 0, null)));

        // Equal display text, different editing text
        assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(null, spannable("Test"), 0, 0, "Blah"),
                        UrlBarData.create(null, spannable("Test"), 0, 0, "Blah2")));

        // Equal display text content, but different emphasis spans
        SpannableStringBuilder text1 = spannable("Test");
        SpannableStringBuilder text2 = spannable("Test");
        text2.setSpan(new UrlEmphasisColorSpan(3), 0, 3, 0);
        text2.setSpan(new UrlEmphasisColorSpan(4), 1, 3, 0);
        text2.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisSecurityErrorSpan(), 0, 1, 0);

        assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(null, text1, 0, 0, "Blah"),
                        UrlBarData.create(null, text2, 0, 0, "Blah")));

        // Add a subset of emphasis spans, but not all.
        text1.setSpan(new UrlEmphasisColorSpan(3), 0, 3, 0);
        text1.setSpan(new UrlEmphasisColorSpan(4), 1, 3, 0);
        assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(
                        UrlBarData.create(null, text1, 0, 0, "Blah"),
                        UrlBarData.create(null, text2, 0, 0, "Blah")));
    }

    @Test
    public void pasteTextValidation() {
        ClipboardManager clipboard =
                (ClipboardManager)
                        RuntimeEnvironment.application.getSystemService(Context.CLIPBOARD_SERVICE);
        clipboard.setPrimaryClip(null);
        assertNull(mMediator.getTextToPaste());

        clipboard.setPrimaryClip(ClipData.newPlainText("", ""));
        assertEquals("", mMediator.getTextToPaste());

        clipboard.setPrimaryClip(ClipData.newPlainText("", "test"));
        assertEquals("test", mMediator.getTextToPaste());

        clipboard.setPrimaryClip(ClipData.newPlainText("", "    test     "));
        assertEquals("test", mMediator.getTextToPaste());
    }

    @Test
    public void cutCopyReplacementTextValidation() {
        String url = "https://www.test.com/blah";
        String displayText = "test.com/blah";
        String editingText = "www.test.com/blah";
        mMediator.setUrlBarData(
                UrlBarData.create(new GURL(url), displayText, 0, 12, editingText),
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
                UrlBarData.create(new GURL(url), displayText, 0, 12, editingText),
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
        UrlBarData baseData =
                UrlBarData.create(
                        new GURL("http://www.example.com"),
                        spannable("www.example.com"),
                        0,
                        14,
                        "Blah");
        mMediator.setUrlBarHintText("Hint 1");
        assertTrue(mModel.get(UrlBarProperties.SHOW_HINT_TEXT));
        mMediator.setUrlBarData(baseData, ScrollType.NO_SCROLL, TextSelection.SELECT_END);
        mModel.get(UrlBarProperties.FOCUS_CHANGE_CALLBACK).onResult(true);
        mModel.get(UrlBarProperties.TEXT_CHANGE_LISTENER).onResult("");

        assertTrue(mModel.get(UrlBarProperties.SHOW_HINT_TEXT));

        mModel.get(UrlBarProperties.TEXT_CHANGE_LISTENER).onResult("f");
        assertFalse(mModel.get(UrlBarProperties.SHOW_HINT_TEXT));
        mMediator.setUrlBarData(UrlBarData.EMPTY, ScrollType.NO_SCROLL, TextSelection.SELECT_END);
        assertTrue(mModel.get(UrlBarProperties.SHOW_HINT_TEXT));

        mModel.get(UrlBarProperties.FOCUS_CHANGE_CALLBACK).onResult(false);
        assertTrue(mModel.get(UrlBarProperties.SHOW_HINT_TEXT));
    }

    @Test
    public void setShowOriginOnly() {
        UrlBarData baseData =
                UrlBarData.create(
                        new GURL("http://www.example.com/a_path_to_ignore"),
                        spannable("http://www.example.com/a_path_to_ignore"),
                        0,
                        22,
                        "Blah");
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
                        new GURL("http://www.example.com"),
                        spannable("www.example.com"),
                        0,
                        14,
                        "Blah");
        UrlBarData dataWithSameDomain =
                UrlBarData.create(
                        new GURL("http://www.example.com/bar"),
                        spannable("www.example.com/bar"),
                        0,
                        14,
                        "Blah");
        UrlBarData dataWithDifferentDomain =
                UrlBarData.create(
                        new GURL("http://www.example.com.subdomain"),
                        spannable("www.example.com.subdomain"),
                        0,
                        20,
                        "Blah");

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
    public void reparentingDropsFocusChangeEvents() {
        mMediator.startReparenting();
        mModel.get(UrlBarProperties.FOCUS_CHANGE_CALLBACK).onResult(true);
        mModel.get(UrlBarProperties.FOCUS_CHANGE_CALLBACK).onResult(false);
        verifyNoInteractions(mFocusChangeCallback);

        mMediator.finishReparenting();
        mModel.get(UrlBarProperties.FOCUS_CHANGE_CALLBACK).onResult(true);
        verify(mFocusChangeCallback).onResult(true);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_SITE_SEARCH)
    public void testManageSearchEnginesCallback_featureEnabled() {
        SettingsNavigation settingsNavigation = mock(SettingsNavigation.class);
        SettingsNavigationFactory.setInstanceForTesting(settingsNavigation);

        Runnable callback = mModel.get(UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK);
        assertNotNull(callback);

        callback.run();
        verify(settingsNavigation).startSettings(eq(mContext), eq(SiteSearchSettings.class));
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_SITE_SEARCH)
    public void testManageSearchEnginesCallback_featureDisabled() {
        Runnable callback = mModel.get(UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK);
        assertNull(callback);
    }

    private static SpannableStringBuilder spannable(String text) {
        return new SpannableStringBuilder(text);
    }
}
