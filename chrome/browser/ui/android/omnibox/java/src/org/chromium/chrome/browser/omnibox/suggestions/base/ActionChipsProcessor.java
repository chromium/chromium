// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.collection.ArraySet;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.action.OmniboxActionType;
import org.chromium.chrome.browser.omnibox.suggestions.ActionChipsDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionsMetrics;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
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
    private @NonNull Set<Integer> mLastVisiblePedals = new ArraySet<>();
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

    /**
     * Setup ActionChips for the suggestion.
     *
     * @param suggestion The suggestion to process.
     * @param model Property model to update.
     * @param position The position for the list of OmniboxPedal among omnibox suggestions.
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

        for (OmniboxPedal chip : actionChipList) {
            final var chipIcon = mActionChipsDelegate.getIcon(chip);
            final var chipModel =
                    new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                            .with(ChipProperties.TEXT, chip.getHint())
                            .with(ChipProperties.CONTENT_DESCRIPTION,
                                    mContext.getString(
                                            R.string.accessibility_omnibox_pedal, chip.getHint()))
                            .with(ChipProperties.ENABLED, true)
                            .with(ChipProperties.CLICK_HANDLER, m -> executeAction(chip, position))
                            .with(ChipProperties.ICON, chipIcon.iconRes)
                            .with(ChipProperties.APPLY_ICON_TINT, chipIcon.tintWithTextColor)
                            .build();

            modelList.add(new ListItem(ActionChipsProperties.ViewType.CHIP, chipModel));

            if (chip.hasPedalId()) {
                mLastVisiblePedals.add(chip.getPedalID());
            } else if (chip.hasActionId()
                    && chip.getActionID() == OmniboxActionType.HISTORY_CLUSTERS) {
                mJourneysActionShownPosition = position;
            }
        }

        model.set(ActionChipsProperties.ACTION_CHIPS, modelList);
    }

    private boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        return suggestion.getActions().size() > 0 && position < MAX_POSITION;
    }

    private void executeAction(@NonNull OmniboxPedal omniboxPedal, int position) {
        if (omniboxPedal.hasActionId()
                && omniboxPedal.getActionID() == OmniboxActionType.HISTORY_CLUSTERS) {
            RecordHistogram.recordEnumeratedHistogram("Omnibox.SuggestionUsed.ResumeJourney",
                    position, SuggestionsMetrics.MAX_AUTOCOMPLETE_POSITION);
        }
        mSuggestionHost.finishInteraction();
        mActionChipsDelegate.execute(omniboxPedal);
    }

    /**
     * Record the actions shown for all action types (Journeys + any pedals).
     */
    private void recordActionsShown() {
        for (Integer pedal : mLastVisiblePedals) {
            SuggestionsMetrics.recordPedalShown(pedal);
        }
        if (mJourneysActionShownPosition != -1) {
            RecordHistogram.recordExactLinearHistogram("Omnibox.ResumeJourneyShown",
                    mJourneysActionShownPosition, SuggestionsMetrics.MAX_AUTOCOMPLETE_POSITION);
        }

        mJourneysActionShownPosition = -1;
        mLastVisiblePedals.clear();
    }
}
