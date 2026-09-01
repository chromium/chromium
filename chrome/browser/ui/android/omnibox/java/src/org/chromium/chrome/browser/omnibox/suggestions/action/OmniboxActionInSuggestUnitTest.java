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

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.SuggestTemplateInfoProto.SuggestTemplateInfo.TemplateAction.ActionType;
import org.chromium.components.omnibox.action.ActionPresentationMode;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;

/** Tests for {@link OmniboxActionInSuggest}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxActionInSuggestUnitTest {
    private static final List<Integer> sKnownActionTypes =
            List.of(
                    ActionType.CALL_VALUE,
                    ActionType.DIRECTIONS_VALUE,
                    ActionType.REVIEWS_VALUE,
                    ActionType.CHROME_AIM_VALUE,
                    ActionType.CHROME_TAB_SWITCH_VALUE);

    @Rule public final MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock private OmniboxActionDelegate mDelegate;
    @Captor private ArgumentCaptor<Intent> mIntentCaptor;
    @Captor private ArgumentCaptor<String> mUrlCaptor;

    @Test
    public void creation_usesCustomIconForKnownActionTypes() {
        for (var entitySuggestionActionType : sKnownActionTypes) {
            var action =
                    new OmniboxActionInSuggest(
                            0,
                            "hint",
                            "accessibility",
                            entitySuggestionActionType,
                            "",
                            /* tabId= */ 0,
                            ActionPresentationMode.CHIP);
            assertNotEquals(OmniboxAction.DEFAULT_ICON, action.icon);
        }
    }

    @Test
    public void creation_usesFallbackIconForUnknownActionTypes() {
        for (var entitySuggestionActionType : ActionType.values()) {
            if (sKnownActionTypes.contains(entitySuggestionActionType.getNumber())) continue;
            var action =
                    new OmniboxActionInSuggest(
                            0,
                            "hint",
                            "accessibility",
                            entitySuggestionActionType.getNumber(),
                            "",
                            /* tabId= */ 0,
                            ActionPresentationMode.CHIP);
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
                                ActionType.CALL_VALUE,
                                "",
                                /* tabId= */ 0,
                                ActionPresentationMode.CHIP));
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
                                ActionType.CALL_VALUE,
                                "",
                                /* tabId= */ 0,
                                ActionPresentationMode.CHIP));
    }

    @Test
    public void safeCasting_assertsWithNull() {
        assertThrows(AssertionError.class, () -> OmniboxActionInSuggest.from(null));
    }

    @Test
    public void safeCasting_assertsWithWrongClassType() {
        OmniboxAction action =
                new OmniboxAction(
                        OmniboxActionId.ACTION_IN_SUGGEST,
                        0,
                        "hint",
                        "accessibility",
                        null,
                        R.style.TextAppearance_ChipText,
                        ActionPresentationMode.CHIP,
                        WindowOpenDisposition.CURRENT_TAB) {
                    @Override
                    public boolean execute(OmniboxActionDelegate d) {
                        return true;
                    }
                };
        assertThrows(AssertionError.class, () -> OmniboxActionInSuggest.from(action));
    }

    @Test
    public void safeCasting_successWithFactoryBuiltAction() {
        OmniboxActionInSuggest.from(
                OmniboxActionFactory.buildActionInSuggest(
                        0,
                        "hint",
                        "accessibility",
                        ActionType.REVIEWS_VALUE,
                        "",
                        /* tabId= */ 0,
                        ActionPresentationMode.CHIP));
    }

    /** Create Action in Suggest with a supplied definition. */
    private OmniboxAction buildActionInSuggest(ActionType type, Intent intent) {
        var uri = intent.toUri(Intent.URI_INTENT_SCHEME);
        return new OmniboxActionInSuggest(
                0,
                "wink",
                "accessibility",
                type.getNumber(),
                uri,
                /* tabId= */ 0,
                ActionPresentationMode.CHIP);
    }

    @Test
    public void executeActionInSuggest_executeDirectionsWithMaps() {
        doReturn(false).when(mDelegate).isIncognito();
        doReturn(true).when(mDelegate).startActivity(any());

        buildActionInSuggest(ActionType.DIRECTIONS, new Intent("Magic Intent Action"))
                .execute(mDelegate);

        verify(mDelegate).isIncognito();
        verify(mDelegate).startActivity(mIntentCaptor.capture());
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

        buildActionInSuggest(ActionType.DIRECTIONS, intent).execute(mDelegate);

        verify(mDelegate).isIncognito();

        // Should not be recorded.
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult"));

        verify(mDelegate).loadPageInCurrentTab(mUrlCaptor.capture());

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

        buildActionInSuggest(ActionType.DIRECTIONS, intent).execute(mDelegate);

        verify(mDelegate).isIncognito();

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        OmniboxMetrics.ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND));

        verify(mDelegate).loadPageInCurrentTab(mUrlCaptor.capture());
        verify(mDelegate).startActivity(any());

        var url = mUrlCaptor.getValue();
        assertNotNull(url);
        assertEquals(UrlConstants.CHROME_DINO_URL, url);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executeActionInSuggest_executeCallActionWithDialer() {
        doReturn(false).when(mDelegate).isIncognito();
        doReturn(true).when(mDelegate).startActivity(any());

        buildActionInSuggest(ActionType.CALL, new Intent(Intent.ACTION_CALL)).execute(mDelegate);

        verify(mDelegate).isIncognito();
        verify(mDelegate).startActivity(mIntentCaptor.capture());
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

        buildActionInSuggest(ActionType.CALL, intent).execute(mDelegate);

        verify(mDelegate).isIncognito();
        verify(mDelegate).startActivity(any());

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

        buildActionInSuggest(ActionType.REVIEWS, intent).execute(mDelegate);

        verify(mDelegate).isIncognito();

        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult"));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Omnibox.ActionInSuggest.IntentResult",
                        OmniboxMetrics.ActionInSuggestIntentResult.SUCCESS));

        verify(mDelegate).loadPageInCurrentTab(mUrlCaptor.capture());

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

        buildActionInSuggest(ActionType.CHROME_AIM, intent).execute(mDelegate);

        verify(mDelegate).isIncognito();

        histogramWatcher.assertExpected();

        verify(mDelegate).loadPageInCurrentTab(mUrlCaptor.capture());

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
                            ActionPresentationMode.CHIP);
            assertEquals(
                    actionType == ActionType.CHROME_TAB_SWITCH_VALUE
                            ? WindowOpenDisposition.SWITCH_TO_TAB
                            : WindowOpenDisposition.CURRENT_TAB,
                    action.disposition);
        }
    }
}
