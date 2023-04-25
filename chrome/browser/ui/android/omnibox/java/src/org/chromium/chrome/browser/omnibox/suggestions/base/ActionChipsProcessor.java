// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.util.SparseBooleanArray;

import androidx.annotation.NonNull;
import androidx.collection.ArraySet;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.ActionChipsDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionsMetrics;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionInSuggest;
import org.chromium.components.omnibox.action.OmniboxActionType;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Set;

/**
 * A class that handles model creation for the Action Chips.
 */
public class ActionChipsProcessor {
    // Only show action chips for the top 3 suggestions.
    private static final int MAX_POSITION = 3;

    private final @NonNull Context mContext;
    private final @NonNull ActionChipsDelegate mActionChipsDelegate;
    private final @NonNull SuggestionHost mSuggestionHost;
    private final @NonNull Set<Integer> mLastVisiblePedals = new ArraySet<>();
    private final @NonNull SparseBooleanArray mActionInSuggestShownOrUsed =
            new SparseBooleanArray();
    private int mJourneysActionShownPosition = -1;

    /**
     * @param context An Android context.
     * @param suggestionHost Component receiving suggestion events.
     * @param actionChipsDelegate A delegate that will responsible for pedals.
     */
    public ActionChipsProcessor(@NonNull Context context, @NonNull SuggestionHost suggestionHost,
            @NonNull ActionChipsDelegate actionChipsDelegate) {
        mContext = context;
        mSuggestionHost = suggestionHost;
        mActionChipsDelegate = actionChipsDelegate;
    }

    public void onUrlFocusChange(boolean hasFocus) {
        if (!hasFocus) {
            recordActionsShown();
        }
    }

    public void onSuggestionsReceived() {
        mActionInSuggestShownOrUsed.clear();
    }

    /**
     * Setup ActionChips for the suggestion.
     *
     * @param suggestion The suggestion to process.
     * @param model Property model to update.
     * @param position The position of the suggestion with OmniboxAction(s) on the suggestion list.
     */
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        if (!doesProcessSuggestion(suggestion, position)) {
            model.set(ActionChipsProperties.ACTION_CHIPS, null);
            return;
        }

        var actionChipList = suggestion.getActions();
        var modelList = new ModelList();

        // The header item increases lead-in padding before the first actual chip is shown.
        // In default state, the chips will align with the suggestion text, but when scrolled
        // the chips may show up under the decoration.
        modelList.add(new ListItem(ActionChipsProperties.ViewType.HEADER, new PropertyModel()));

        for (OmniboxAction chip : actionChipList) {
            final var chipModel =
                    new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                            .with(ChipProperties.TEXT, chip.hint)
                            .with(ChipProperties.CONTENT_DESCRIPTION,
                                    mContext.getString(
                                            R.string.accessibility_omnibox_pedal, chip.hint))
                            .with(ChipProperties.ENABLED, true)
                            .with(ChipProperties.CLICK_HANDLER, m -> executeAction(chip, position))
                            .with(ChipProperties.ICON, chip.icon.iconRes)
                            .with(ChipProperties.APPLY_ICON_TINT, chip.icon.tintWithTextColor)
                            .build();

            modelList.add(new ListItem(ActionChipsProperties.ViewType.CHIP, chipModel));

            // TODO(crbug/1418077): Move this to appropriate implementations.
            switch (chip.actionId) {
                case OmniboxActionType.PEDAL:
                    mLastVisiblePedals.add(OmniboxPedal.from(chip).pedalId);
                    break;

                case OmniboxActionType.HISTORY_CLUSTERS:
                    mJourneysActionShownPosition = position;
                    break;

                case OmniboxActionType.ACTION_IN_SUGGEST:
                    var actionType = OmniboxActionInSuggest.from(chip)
                                             .actionInfo.getActionType()
                                             .getNumber();
                    mActionInSuggestShownOrUsed.put(actionType, false);
                    break;
            }
        }

        model.set(ActionChipsProperties.ACTION_CHIPS, modelList);
    }

    private boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        return suggestion.getActions().size() > 0 && position < MAX_POSITION;
    }

    /**
     * Invoke action associated with the ActionChip.
     *
     * TODO(crbug/1418077): Move this to appropriate implementations.
     */
    private void executeAction(@NonNull OmniboxAction action, int position) {
        switch (action.actionId) {
            case OmniboxActionType.HISTORY_CLUSTERS:
                SuggestionsMetrics.recordResumeJourneyClick(position);
                break;

            case OmniboxActionType.ACTION_IN_SUGGEST:
                var actionType =
                        OmniboxActionInSuggest.from(action).actionInfo.getActionType().getNumber();
                mActionInSuggestShownOrUsed.put(actionType, true);
                break;
        }
        mSuggestionHost.finishInteraction();
        mActionChipsDelegate.execute(action);
    }

    /**
     * Record the actions shown for all action types (Journeys + any pedals).
     *
     * TODO(crbug/1418077): Move this to appropriate implementations.
     */
    private void recordActionsShown() {
        for (Integer pedal : mLastVisiblePedals) {
            SuggestionsMetrics.recordPedalShown(pedal);
        }

        for (var actionIndex = 0; actionIndex < mActionInSuggestShownOrUsed.size(); actionIndex++) {
            int actionType = mActionInSuggestShownOrUsed.keyAt(actionIndex);
            boolean wasUsed = mActionInSuggestShownOrUsed.valueAt(actionIndex);
            SuggestionsMetrics.recordActionInSuggestShown(actionType);
            if (wasUsed) {
                SuggestionsMetrics.recordActionInSuggestUsed(actionType);
            }
        }

        SuggestionsMetrics.recordResumeJourneyShown(mJourneysActionShownPosition);

        mJourneysActionShownPosition = -1;
        mLastVisiblePedals.clear();
    }
}
