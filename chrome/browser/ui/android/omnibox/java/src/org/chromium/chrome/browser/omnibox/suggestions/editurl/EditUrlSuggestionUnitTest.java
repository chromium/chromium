// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.ClipboardImpl;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

import java.util.List;

/**
 * Unit tests for the "edit url" omnibox suggestion.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGURL.class})
public final class EditUrlSuggestionUnitTest {
    private static final String TEST_TITLE = "Test Page";
    private static final String FOOBAR_SEARCH_TERMS = "foobar";
    private static final String BARBAZ_SEARCH_TERMS = "barbaz";

    private static final int ACTION_SHARE = 0;
    private static final int ACTION_COPY = 1;
    private static final int ACTION_EDIT = 2;

    private static final GURL WEB_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
    private static final GURL SEARCH_URL_1 = JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL);
    private static final GURL SEARCH_URL_2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_2_URL);

    public @Rule TestRule mFeaturesProcessor = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock ShareDelegate mShareDelegate;
    private @Mock Tab mTab;
    private @Mock SadTab mSadTab;
    private @Mock AutocompleteMatch mWhatYouTypedSuggestion;
    private @Mock AutocompleteMatch mOtherSuggestion;
    private @Mock AutocompleteMatch mSearchSuggestion;
    private @Mock UrlBarDelegate mUrlBarDelegate;
    private @Mock View mEditButton;
    private @Mock View mSuggestionView;
    private @Mock FaviconFetcher mIconFetcher;
    private @Mock TemplateUrlService mTemplateUrlService;
    private @Mock SuggestionHost mSuggestionHost;
    private @Mock ClipboardManager mClipboardManager;

    // The original (real) ClipboardManager to be restored after a test run.
    private ClipboardManager mOldClipboardManager;
    private UserDataHost mUserDataHost;
    private EditUrlSuggestionProcessor mProcessor;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mUserDataHost = new UserDataHost();
            mUserDataHost.setUserData(SadTab.class, mSadTab);

            mOldClipboardManager = ((ClipboardImpl) Clipboard.getInstance())
                                           .overrideClipboardManagerForTesting(mClipboardManager);

            mModel = new PropertyModel.Builder(SuggestionViewProperties.ALL_KEYS).build();

            mProcessor = new EditUrlSuggestionProcessor(ContextUtils.getApplicationContext(),
                    mSuggestionHost, mUrlBarDelegate, mIconFetcher,
                    () -> mTab, () -> mShareDelegate);
        });

        doReturn(WEB_URL).when(mTab).getUrl();
        doReturn(TEST_TITLE).when(mTab).getTitle();
        doReturn(false).when(mTab).isNativePage();
        doReturn(true).when(mTab).isInitialized();

        // Simulate that all our test tabs are never 'sad'.
        doReturn(mUserDataHost).when(mTab).getUserDataHost();
        doReturn(false).when(mSadTab).isShowing();
        doReturn(OmniboxSuggestionType.URL_WHAT_YOU_TYPED).when(mWhatYouTypedSuggestion).getType();
        doReturn(WEB_URL.getSpec()).when(mWhatYouTypedSuggestion).getDisplayText();
        doReturn(WEB_URL).when(mWhatYouTypedSuggestion).getUrl();

        doReturn(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED).when(mSearchSuggestion).getType();
        doReturn(SEARCH_URL_1).when(mSearchSuggestion).getUrl();
        doReturn(FOOBAR_SEARCH_TERMS).when(mSearchSuggestion).getFillIntoEdit();

        doReturn(OmniboxSuggestionType.SEARCH_HISTORY).when(mOtherSuggestion).getType();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((ClipboardImpl) Clipboard.getInstance())
                    .overrideClipboardManagerForTesting(mOldClipboardManager);
        });
    }

    /** Test that the suggestion is triggered. */
    @Test
    @SmallTest
    public void testUrlSuggestionTriggered() {
        verifyUrlSuggestionTriggered(/* isIncognito */ false);
    }

    /** Test that the suggestion is triggered in Incognito. */
    @Test
    @SmallTest
    public void testSuggestionTriggered_Incognito() {
        verifyUrlSuggestionTriggered(/* isIncognito */ true);
    }

    /** Test that the suggestion is not triggered if its url doesn't match the current page's. */
    @Test
    @SmallTest
    public void testWhatYouTypedWrongUrl() {
        verifyWhatYouTypedWrongUrl(/* isIncognito */ false);
    }

    /**
     * Test that the suggestion is not triggered if its url doesn't match the current page's in
     * Incognito.
     */
    @Test
    @SmallTest
    public void testWhatYouTypedWrongUrl_Incognito() {
        verifyWhatYouTypedWrongUrl(/* isIncognito */ true);
    }

    /** Test the edit button is pressed, the correct method in the URL bar delegate is triggered. */
    @Test
    @SmallTest
    public void testEditButtonPress() {
        verifyEditButtonPress(/* isIncognito */ false);
    }

    /**
     * Test the edit button is pressed, the correct method in the URL bar delegate is triggered in
     * Incognito.
     */
    @Test
    @SmallTest
    public void testEditButtonPress_Incognito() {
        verifyEditButtonPress(/* isIncognito */ true);
    }

    /** Test the share button is pressed, we trigger the share menu. */
    @Test
    @SmallTest
    public void testShareButtonPress() {
        verifyShareButtonPress(/* isIncognito */ false);
    }

    /** Test the share button is pressed, we trigger the share menu in Incognito. */
    @Test
    @SmallTest
    public void testShareButtonPress_Incognito() {
        verifyShareButtonPress(/* isIncognito */ true);
    }

    /** Test the copy button is pressed, we update clipboard. */
    @Test
    @SmallTest
    public void testCopyButtonPress() {
        verifyCopyButtonPress(/* isIncognito */ false);
    }

    /** Test the copy button is pressed, we update clipboard in Incognito. */
    @Test
    @SmallTest
    public void testCopyButtonPress_Incognito() {
        verifyCopyButtonPress(/* isIncognito */ true);
    }

    @Test
    @SmallTest
    public void testSearchSuggestionTriggered() {
        verifySearchSuggestionTriggered(/* isIncognito */ false);
    }

    @Test
    @SmallTest
    public void testSearchSuggestionTriggered_Incognito() {
        verifySearchSuggestionTriggered(/* isIncognito */ true);
    }

    public void verifyUrlSuggestionTriggered(boolean isIncognito) {
        setIncognito(isIncognito);
        mProcessor.onUrlFocusChange(true);

        Assert.assertTrue("The processor should handle the \"what you typed\" suggestion.",
                mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion, 0));

        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        Assert.assertEquals("The model should have the title set.", TEST_TITLE,
                mModel.get(SuggestionViewProperties.TEXT_LINE_1_TEXT).toString());

        Assert.assertEquals("The model should have the URL set to the tab's URL", WEB_URL.getSpec(),
                mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT).toString());
    }

    public void verifyWhatYouTypedWrongUrl(boolean isIncognito) {
        setIncognito(isIncognito);
        mProcessor.onUrlFocusChange(true);

        when(mWhatYouTypedSuggestion.getUrl()).thenReturn(SEARCH_URL_1);
        Assert.assertFalse("The processor should not handle the suggestion.",
                mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion, 0));
    }

    public void verifyEditButtonPress(boolean isIncognito) {
        setIncognito(isIncognito);
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion, 0);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        List<Action> actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        Assert.assertEquals("EditUrl suggestion should have 3 action buttons.", 3, actions.size());
        actions.get(ACTION_EDIT).callback.run();
        verify(mUrlBarDelegate).setOmniboxEditingText(WEB_URL.getSpec());
    }

    private void verifyShareButtonPress(boolean isIncognito) {
        setIncognito(isIncognito);
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion, 0);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        List<Action> actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        Assert.assertEquals("EditUrl suggestion should have 3 action buttons.", 3, actions.size());
        actions.get(ACTION_SHARE).callback.run();
        verify(mShareDelegate, times(1))
                .share(mTab, false /* shareDirectly */, ShareOrigin.EDIT_URL);
    }

    private void verifyCopyButtonPress(boolean isIncognito) {
        setIncognito(isIncognito);
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion, 0);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        List<Action> actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);

        Assert.assertEquals("EditUrl suggestion should have 3 action buttons.", 3, actions.size());
        ArgumentCaptor<ClipData> argument = ArgumentCaptor.forClass(ClipData.class);
        actions.get(ACTION_COPY).callback.run();
        verify(mClipboardManager, times(1)).setPrimaryClip(argument.capture());

        ClipData clip = new ClipData("url", new String[] {"text/x-moz-url", "text/plain"},
                new ClipData.Item(WEB_URL.getSpec()));

        // ClipData doesn't implement equals, but their string representations matching should be
        // good enough.
        Assert.assertEquals(clip.toString(), argument.getValue().toString());
    }

    private void verifySearchSuggestionTriggered(boolean isIncognito) {
        setIncognito(isIncognito);
        when(mTab.getUrl()).thenReturn(SEARCH_URL_1);
        mProcessor.onUrlFocusChange(true);
        when(mTemplateUrlService.getSearchQueryForUrl(SEARCH_URL_1))
                .thenReturn(FOOBAR_SEARCH_TERMS);
        when(mTemplateUrlService.getSearchQueryForUrl(SEARCH_URL_2))
                .thenReturn(BARBAZ_SEARCH_TERMS);

        Assert.assertFalse(mProcessor.doesProcessSuggestion(mSearchSuggestion, 0));
        Assert.assertFalse(mProcessor.doesProcessSuggestion(mSearchSuggestion, 1));

        when(mSearchSuggestion.getUrl()).thenReturn(SEARCH_URL_2);
        when(mSearchSuggestion.getFillIntoEdit()).thenReturn(BARBAZ_SEARCH_TERMS);

        Assert.assertFalse(mProcessor.doesProcessSuggestion(mSearchSuggestion, 0));
        Assert.assertFalse(mProcessor.doesProcessSuggestion(mSearchSuggestion, 1));
    }

    private void setIncognito(boolean isIncognito) {
        when(mTab.isIncognito()).thenReturn(isIncognito);
    }
}
