// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.util.ArrayMap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** A class that handles model creation for the Action Chips. */
public class ActionChipsProcessor {
    private final @NonNull SuggestionHost mSuggestionHost;
    private final @NonNull ArrayMap<OmniboxAction, Integer> mVisibleActions;

    /** The action that was executed, or null if no action was executed by the user. */
    private @Nullable OmniboxAction mExecutedAction;

    /**
     * @param suggestionHost Component receiving suggestion events.
     */
    public ActionChipsProcessor(@NonNull SuggestionHost suggestionHost) {
        mSuggestionHost = suggestionHost;
        mVisibleActions = new ArrayMap<>();
    }

    public void onOmniboxSessionStateChange(boolean activated) {
        // Note: do not record any histograms if we did not show Actions.
        if (activated || mVisibleActions.isEmpty()) {
            return;
        }

        mVisibleActions.forEach(
                (OmniboxAction action, Integer position) -> {
                    var wasValid = action.recordActionShown(position, action == mExecutedAction);
                    OmniboxMetrics.recordOmniboxActionIsValid(wasValid);
                });

        OmniboxMetrics.recordOmniboxActionIsUsed(mExecutedAction != null);
        mVisibleActions.clear();
    }

    public void onSuggestionsReceived() {
        mVisibleActions.clear();
    }

    /**
     * Setup ActionChips for the suggestion.
     *
     * @param suggestion The suggestion to process.
     * @param model Property model to update.
     * @param position The position of the suggestion with OmniboxAction(s) on the suggestion list.
     */
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        if (suggestion.getActions().isEmpty()) {
            model.set(ActionChipsProperties.ACTION_CHIPS, null);
            return;
        }

        var actionChipList = suggestion.getActions();
        var modelList = new ModelList();

        for (OmniboxAction chip : actionChipList) {
            final var chipModel =
                    new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                            .with(ChipProperties.TEXT, chip.hint)
                            .with(ChipProperties.CONTENT_DESCRIPTION, chip.accessibilityHint)
                            .with(ChipProperties.ENABLED, true)
                            .with(ChipProperties.CLICK_HANDLER, m -> executeAction(chip, position))
                            .with(ChipProperties.ICON, chip.icon.iconRes)
                            .with(ChipProperties.APPLY_ICON_TINT, chip.icon.tintWithTextColor)
                            .with(
                                    ChipProperties.PRIMARY_TEXT_APPEARANCE,
                                    chip.primaryTextAppearance)
                            .build();

            modelList.add(new ListItem(ActionChipsProperties.ViewType.CHIP, chipModel));
            mVisibleActions.put(chip, position);
        }

        model.set(ActionChipsProperties.ACTION_CHIPS, modelList);
    }

    /** Invoke action associated with the ActionChip. */
    private void executeAction(@NonNull OmniboxAction action, int position) {
        mExecutedAction = action;
        mSuggestionHost.onOmniboxActionClicked(action, position);
    }
}
