// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.components.omnibox.action.OmniboxActionJni;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Tests for {@link ActionChipsProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionChipsProcessorUnitTest {
    private static final int MATCH_POS = 1234;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule JniMocker mJniMocker = new JniMocker();

    private @Mock OmniboxActionJni mOmniboxActionJni;
    private @Mock SuggestionHost mSuggestionHost;

    private ActionChipsProcessor mProcessor;
    private PropertyModel mModel;
    private ModelList mActionModel;

    @Before
    public void setUp() {
        mJniMocker.mock(OmniboxActionJni.TEST_HOOKS, mOmniboxActionJni);

        mProcessor = new ActionChipsProcessor(mSuggestionHost);
        mModel = new PropertyModel(ActionChipsProperties.ALL_UNIQUE_KEYS);
    }

    /**
     * Create an instance of OmniboxAction associated with specific native handle.
     *
     * @param handle the native handle to associate the instance with. 0 indicates invalid action.
     */
    private OmniboxAction actionWithHandle(long handle) {
        return actionWithHandleAndTextAppearance(handle, R.style.TextAppearance_ChipText);
    }

    private OmniboxAction actionWithHandleAndTextAppearance(long handle, int textAppearance) {
        return new OmniboxAction(
                OmniboxActionId.ACTION_IN_SUGGEST,
                handle,
                "hint",
                "accessibility hint",
                OmniboxAction.DEFAULT_ICON,
                textAppearance) {
            @Override
            public void execute(OmniboxActionDelegate delegate) {}
        };
    }

    /** Create a suggestion with supplied OmniboxActions (if any) and populate model. */
    private void populateModelForActions(OmniboxAction... actions) {
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED)
                        .setActions(List.of(actions))
                        .build();
        mProcessor.populateModel(match, mModel, MATCH_POS);
        mActionModel = mModel.get(ActionChipsProperties.ACTION_CHIPS);
    }

    /** Simulate focus lost; confirm no more histograms are recorded. */
    private void verifyNoFollowUpRecords() {
        clearInvocations(mOmniboxActionJni);
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_USED)
                        .expectNoRecords(OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_VALID)
                        .build();
        mProcessor.onOmniboxSessionStateChange(false);
        watcher.assertExpected();
        verifyNoMoreInteractions(mOmniboxActionJni);
    }

    @Test
    public void onOmniboxSessionStateChange_noRecordsEverOnActivation() {
        // This is a perfectly normal scenario. Simulate that we have a suggestion with actions, we
        // click on one action, and then emit a "focus" signal. There should be NO uma records.
        populateModelForActions(actionWithHandle(1), actionWithHandle(/* handle= */ 0));
        assertEquals(2, mActionModel.size());
        mActionModel.get(0).model.get(ChipProperties.CLICK_HANDLER).onResult(/* model= */ null);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_USED)
                        .expectNoRecords(OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_VALID)
                        .build();
        mProcessor.onOmniboxSessionStateChange(true);
        watcher.assertExpected();
        verifyNoMoreInteractions(mOmniboxActionJni);
    }

    @Test
    public void onOmniboxSessionStateChange_noRecordsWhenNoActionsWereAvailable() {
        populateModelForActions(/* no actions */ );
        verifyNoFollowUpRecords();
    }

    @Test
    public void onOmniboxSessionStateChange_recordNoUsage() {
        populateModelForActions(actionWithHandle(1));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_USED, false)
                        .build();

        // Finish interaction.
        mProcessor.onOmniboxSessionStateChange(false);
        verify(mOmniboxActionJni).recordActionShown(1L, MATCH_POS, false);
        verifyNoMoreInteractions(mOmniboxActionJni);
        histogramWatcher.assertExpected();

        verifyNoFollowUpRecords();
    }

    @Test
    public void onOmniboxSessionStateChange_recordUsage() {
        populateModelForActions(actionWithHandle(1));
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_USED, true)
                        .build();

        // Click!
        assertEquals(1, mActionModel.size());
        mActionModel.get(0).model.get(ChipProperties.CLICK_HANDLER).onResult(/* model= */ null);
        mProcessor.onOmniboxSessionStateChange(false);

        verify(mOmniboxActionJni).recordActionShown(1L, MATCH_POS, /* used= */ true);
        histogramWatcher.assertExpected();

        verifyNoFollowUpRecords();
    }

    @Test
    public void onOmniboxSessionStateChange_recordActionValid() {
        populateModelForActions(actionWithHandle(1), actionWithHandle(2));
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_VALID, true, 2)
                        .build();

        mProcessor.onOmniboxSessionStateChange(false);

        verify(mOmniboxActionJni).recordActionShown(1L, MATCH_POS, false);
        verify(mOmniboxActionJni).recordActionShown(2L, MATCH_POS, false);
        verifyNoMoreInteractions(mOmniboxActionJni);
        histogramWatcher.assertExpected();

        verifyNoFollowUpRecords();
    }

    @Test
    public void onOmniboxSessionStateChange_recordActionNotValidAfterDestroyCalled() {
        var action1 = actionWithHandle(1);
        var action2 = actionWithHandle(2);
        populateModelForActions(action1, action2);

        action1.destroy();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_VALID, true, 1)
                        .expectBooleanRecordTimes(
                                OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_VALID, false, 1)
                        .build();

        mProcessor.onOmniboxSessionStateChange(false);

        verify(mOmniboxActionJni).recordActionShown(2L, MATCH_POS, false);
        verifyNoMoreInteractions(mOmniboxActionJni);
        histogramWatcher.assertExpected();

        verifyNoFollowUpRecords();
    }

    @Test
    public void onOmniboxSessionStateChange_actionsFromMultipleMatchesAreAggregated() {
        // Three matches. 2 valid, 1 invalid.
        // Note that typically all the ModelLists are associated with individual PropertyModels, but
        // our tests use just a single PropertyModel, so these ModelLists get replaced.
        populateModelForActions(actionWithHandle(1), actionWithHandle(2));
        populateModelForActions(actionWithHandle(3), actionWithHandle(4));
        populateModelForActions(actionWithHandle(0), actionWithHandle(0));

        // Only the latest ModelList is available (previos two have been overwritten).
        assertEquals(2, mActionModel.size());
        mActionModel.get(0).model.get(ChipProperties.CLICK_HANDLER).onResult(/* model= */ null);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_VALID, true, 4)
                        .expectBooleanRecordTimes(
                                OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_VALID, false, 2)
                        .build();

        mProcessor.onOmniboxSessionStateChange(false);

        verify(mOmniboxActionJni).recordActionShown(1L, MATCH_POS, false);
        verify(mOmniboxActionJni).recordActionShown(2L, MATCH_POS, false);
        verify(mOmniboxActionJni).recordActionShown(3L, MATCH_POS, false);
        verify(mOmniboxActionJni).recordActionShown(4L, MATCH_POS, false);
        verifyNoMoreInteractions(mOmniboxActionJni);
        histogramWatcher.assertExpected();

        verifyNoFollowUpRecords();
    }

    @Test
    public void onOmniboxSessionStateChange_recordActionInvalid() {
        populateModelForActions(actionWithHandle(1), actionWithHandle(/* handle= */ 0));
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_VALID, true, 1)
                        .expectBooleanRecordTimes(
                                OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_VALID, false, 1)
                        .build();

        mProcessor.onOmniboxSessionStateChange(false);
        verify(mOmniboxActionJni).recordActionShown(1L, MATCH_POS, false);
        histogramWatcher.assertExpected();

        verifyNoFollowUpRecords();
    }

    @Test
    public void onSuggestionsReceived_resetsLastState() {
        populateModelForActions(actionWithHandle(1), actionWithHandle(/* handle= */ 0));
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_VALID, true, 1)
                        .expectBooleanRecordTimes(
                                OmniboxMetrics.HISTOGRAM_OMNIBOX_ACTION_VALID, false, 1)
                        .build();

        // Simulate new set of suggestions.
        mProcessor.onSuggestionsReceived();
        verifyNoFollowUpRecords();
    }

    @Test
    public void chipTextAppearance() {
        populateModelForActions(
                actionWithHandleAndTextAppearance(1, R.style.TextAppearance_ChipText),
                actionWithHandleAndTextAppearance(
                        2, R.style.TextAppearance_TextMediumThick_Primary_Baseline));

        ModelList chipModel = mModel.get(ActionChipsProperties.ACTION_CHIPS);
        assertEquals(
                R.style.TextAppearance_ChipText,
                chipModel.get(0).model.get(ChipProperties.PRIMARY_TEXT_APPEARANCE));
        assertEquals(
                R.style.TextAppearance_TextMediumThick_Primary_Baseline,
                chipModel.get(1).model.get(ChipProperties.PRIMARY_TEXT_APPEARANCE));
    }
}
