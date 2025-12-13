// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Intent;
import android.net.Uri;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.SuggestTemplateInfoProto.SuggestTemplateInfo;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;

/** Tests for {@link OmniboxActionInSuggest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OmniboxActionInSuggestUnitTest {
    private static final List<Integer> sKnownActionTypes =
            List.of(
                    SuggestTemplateInfo.TemplateAction.ActionType.CALL_VALUE,
                    SuggestTemplateInfo.TemplateAction.ActionType.DIRECTIONS_VALUE,
                    SuggestTemplateInfo.TemplateAction.ActionType.REVIEWS_VALUE,
                    SuggestTemplateInfo.TemplateAction.ActionType.CHROME_AIM_VALUE,
                    SuggestTemplateInfo.TemplateAction.ActionType.CHROME_TAB_SWITCH_VALUE);
    private static final SuggestTemplateInfo.TemplateAction EMPTY_INFO =
            SuggestTemplateInfo.TemplateAction.getDefaultInstance();

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock OmniboxActionDelegate mDelegate;
    private @Captor ArgumentCaptor<Intent> mIntentCaptor;
    private @Captor ArgumentCaptor<String> mUrlCaptor;

    @Test
    public void creation_usesCustomIconForKnownActionTypes() {
        for (var kesemActionType : sKnownActionTypes) {
            var action =
                    new OmniboxActionInSuggest(
                            0,
                            "hint",
                            "accessibility",
                            kesemActionType,
                            "",
                            /* tabId= */ 0,
                            /* showAsActionButton= */ false);
            assertNotEquals(OmniboxAction.DEFAULT_ICON, action.icon);
        }
    }

    @Test
    public void creation_usesFallbackIconForUnknownActionTypes() {
        for (var kesemActionType : SuggestTemplateInfo.TemplateAction.ActionType.values()) {
            if (sKnownActionTypes.contains(kesemActionType.getNumber())) continue;
            var action =
                    new OmniboxActionInSuggest(
                            0,
                            "hint",
                            "accessibility",
                            kesemActionType.getNumber(),
                            "",
                            /* tabId= */ 0,
                            /* showAsActionButton= */ false);
            assertEquals(OmniboxAction.DEFAULT_ICON, action.icon);
        }
    }

    @Test
    public void creation_failsWithNullHint() {
        assertThrows(
                AssertionError.class,
                () ->
                        new OmniboxActionInSuggest(
                                0,
                                null,
                                "",
                                SuggestTemplateInfo.TemplateAction.ActionType.CALL_VALUE,
                                "",
                                /* tabId= */ 0,
                                /* showAsActionButton= */ false));
    }

    @Test
    public void creation_failsWithEmptyHint() {
        assertThrows(
                AssertionError.class,
                () ->
                        new OmniboxActionInSuggest(
                                0,
                                "",
                                "",
                                SuggestTemplateInfo.TemplateAction.ActionType.CALL_VALUE,
                                "",
                                /* tabId= */ 0,
                                /* showAsActionButton= */ false));
    }

    @Test
    public void safeCasting_assertsWithNull() {
        assertThrows(AssertionError.class, () -> OmniboxActionInSuggest.from(null));
    }

    @Test
    public void safeCasting_assertsWithWrongClassType() {
        assertThrows(
                AssertionError.class,
                () ->
                        OmniboxActionInSuggest.from(
                                new OmniboxAction(
                                        OmniboxActionId.ACTION_IN_SUGGEST,
                                        0,
                                        "hint",
                                        "accessibility",
                                        null,
                                        R.style.TextAppearance_ChipText,
                                        /* showAsActionButton= */ false,
                                        WindowOpenDisposition.CURRENT_TAB) {
                                    @Override
                                    public void execute(OmniboxActionDelegate d) {}
                                }));
    }

    @Test
    public void safeCasting_successWithFactoryBuiltAction() {
        OmniboxActionInSuggest.from(
                OmniboxActionFactoryImpl.get()
                        .buildActionInSuggest(
                                0,
                                "hint",
                                "accessibility",
                                SuggestTemplateInfo.TemplateAction.ActionType.REVIEWS_VALUE,
                                "",
                                /* tabId= */ 0,
                                /* showAsActionButton= */ false));
    }

    /** Create Action in Suggest with a supplied definition. */
    private OmniboxAction buildActionInSuggest(
            SuggestTemplateInfo.TemplateAction.ActionType type, Intent intent) {
        var uri = intent.toUri(Intent.URI_INTENT_SCHEME);
        return new OmniboxActionInSuggest(
                0,
                "wink",
                "accessibility",
                type.getNumber(),
                uri,
                /* tabId= */ 0,
                /* showAsActionButton= */ false);
    }

    @Test
    public void executeActionInSuggest_executeDirectionsWithMaps() {
        doReturn(false).when(mDelegate).isIncognito();
        doReturn(true).when(mDelegate).startActivity(any());

        buildActionInSuggest(
                        SuggestTemplateInfo.TemplateAction.ActionType.DIRECTIONS,
                        new Intent("Magic Intent Action"))
                .execute(mDelegate);

        verify(mDelegate, times(1)).isIncognito();
        verify(mDelegate, times(1)).startActivity(mIntentCaptor.capture());
        var intent = mIntentCaptor.getValue();

        assertEquals("Magic Intent Action", intent.getAction());

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        OmniboxMetrics.ActionInSuggestIntentResult.SUCCESS));
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executeActionInSuggest_executeDirectionsInBrowserForIncognitoMode() {
        doReturn(true).when(mDelegate).isIncognito();

        var intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));

        buildActionInSuggest(SuggestTemplateInfo.TemplateAction.ActionType.DIRECTIONS, intent)
                .execute(mDelegate);

        verify(mDelegate, times(1)).isIncognito();

        // Should not be recorded.
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult"));

        verify(mDelegate, times(1)).loadPageInCurrentTab(mUrlCaptor.capture());

        var url = mUrlCaptor.getValue();
        assertNotNull(url);
        assertEquals(UrlConstants.CHROME_DINO_URL, url);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executeActionInSuggest_redirectDirectionsActionToLocalTabIfAvailable() {
        doReturn(false).when(mDelegate).isIncognito();
        doReturn(false).when(mDelegate).startActivity(any());

        var intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));

        buildActionInSuggest(SuggestTemplateInfo.TemplateAction.ActionType.DIRECTIONS, intent)
                .execute(mDelegate);

        verify(mDelegate, times(1)).isIncognito();

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        OmniboxMetrics.ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND));

        verify(mDelegate, times(1)).loadPageInCurrentTab(mUrlCaptor.capture());
        verify(mDelegate, times(1)).startActivity(any());

        var url = mUrlCaptor.getValue();
        assertNotNull(url);
        assertEquals(UrlConstants.CHROME_DINO_URL, url);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executeActionInSuggest_executeCallActionWithDialer() {
        doReturn(false).when(mDelegate).isIncognito();
        doReturn(true).when(mDelegate).startActivity(any());

        buildActionInSuggest(
                        SuggestTemplateInfo.TemplateAction.ActionType.CALL, new Intent(Intent.ACTION_CALL))
                .execute(mDelegate);

        verify(mDelegate, times(1)).isIncognito();
        verify(mDelegate, times(1)).startActivity(mIntentCaptor.capture());
        var intent = mIntentCaptor.getValue();

        // OBSERVE: We rewrite ACTION_CALL with ACTION_DIAL, which does not carry high permission
        // requirements.
        assertEquals(Intent.ACTION_DIAL, intent.getAction());

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        OmniboxMetrics.ActionInSuggestIntentResult.SUCCESS));
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executeActionInSuggest_dontRedirectCallActionToLocalTab() {
        doReturn(false).when(mDelegate).isIncognito();
        doReturn(false).when(mDelegate).startActivity(any());

        var intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));

        buildActionInSuggest(SuggestTemplateInfo.TemplateAction.ActionType.CALL, intent).execute(mDelegate);

        verify(mDelegate, times(1)).isIncognito();
        verify(mDelegate, times(1)).startActivity(any());

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        OmniboxMetrics.ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND));
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executeActionInSuggest_executeReviewsInTab() {
        var intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));

        buildActionInSuggest(SuggestTemplateInfo.TemplateAction.ActionType.REVIEWS, intent)
                .execute(mDelegate);

        verify(mDelegate, times(1)).isIncognito();

        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult"));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        OmniboxMetrics.ActionInSuggestIntentResult.SUCCESS));

        verify(mDelegate, times(1)).loadPageInCurrentTab(mUrlCaptor.capture());

        var url = mUrlCaptor.getValue();
        assertNotNull(url);
        assertEquals(UrlConstants.CHROME_DINO_URL, url);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executeActionInSuggest_executeAim() {
        var intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        OmniboxMetrics.ActionInSuggestIntentResult.SUCCESS);

        buildActionInSuggest(SuggestTemplateInfo.TemplateAction.ActionType.CHROME_AIM, intent)
                .execute(mDelegate);

        verify(mDelegate, times(1)).isIncognito();

        histogramWatcher.assertExpected();

        verify(mDelegate, times(1)).loadPageInCurrentTab(mUrlCaptor.capture());

        var url = mUrlCaptor.getValue();
        assertNotNull(url);
        assertEquals(UrlConstants.CHROME_DINO_URL, url);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void getDisposition() {
        for (var actionType : sKnownActionTypes) {
            var action =
                    new OmniboxActionInSuggest(
                            0,
                            "hint",
                            "accessibility",
                            actionType,
                            "",
                            /* tabId= */ 0,
                            /* showAsActionButton= */ false);
            assertEquals(
                    actionType
                                    == SuggestTemplateInfo.TemplateAction.ActionType
                                            .CHROME_TAB_SWITCH_VALUE
                            ? WindowOpenDisposition.SWITCH_TO_TAB
                            : WindowOpenDisposition.CURRENT_TAB,
                    action.disposition);
        }
    }
}
