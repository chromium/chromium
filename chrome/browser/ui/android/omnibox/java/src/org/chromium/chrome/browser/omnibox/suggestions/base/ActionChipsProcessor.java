// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** A class that handles model creation for the Action Chips. */
@NullMarked
public class ActionChipsProcessor {
    private final SuggestionHost mSuggestionHost;

    /**
     * @param suggestionHost Component receiving suggestion events.
     */
    public ActionChipsProcessor(SuggestionHost suggestionHost) {
        mSuggestionHost = suggestionHost;
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

        var actions = suggestion.getActions();
        var modelList = new ModelList();

        for (OmniboxAction action : actions) {
            // Skip the action that is shown as button, instead of chip.
            if (action.showAsActionButton) {
                continue;
            }

            final var chipModel =
                    new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                            .with(ChipProperties.TEXT, action.hint)
                            .with(ChipProperties.CONTENT_DESCRIPTION, action.accessibilityHint)
                            .with(ChipProperties.ENABLED, true)
                            .with(
                                    ChipProperties.CLICK_HANDLER,
                                    m -> executeAction(action, position))
                            .with(ChipProperties.ICON, action.icon.chipIconRes)
                            .with(ChipProperties.APPLY_ICON_TINT, action.icon.tintWithTextColor)
                            .with(
                                    ChipProperties.PRIMARY_TEXT_APPEARANCE,
                                    action.primaryTextAppearance)
                            .build();

            modelList.add(new ListItem(ActionChipsProperties.ViewType.CHIP, chipModel));
        }
        if (modelList.size() == 0) {
            return;
        }

        model.set(ActionChipsProperties.ACTION_CHIPS, modelList);
    }

    /** Invoke action associated with the ActionChip. */
    private void executeAction(OmniboxAction action, int position) {
        mSuggestionHost.onOmniboxActionClicked(action, position);
    }
}
