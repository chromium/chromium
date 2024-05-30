// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.NonNull;

import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.url.GURL;

/** A mechanism for creating {@link SuggestionViewDelegate}s. */
public interface SuggestionHost {
    /**
     * Triggered when the user selects to refine one of the omnibox suggestions.
     *
     * @param suggestion Suggestion to use to refine Omnibox query.
     */
    void onRefineSuggestion(@NonNull AutocompleteMatch suggestion);

    /**
     * Triggered when the user selects one of the omnibox suggestions to navigate to.
     *
     * @param suggestion User-selected Suggestion.
     * @param position The position of the suggestion on the list.
     * @param url The specific URL associated with the suggestion to navigate to.
     */
    void onSuggestionClicked(
            @NonNull AutocompleteMatch suggestion, int position, @NonNull GURL url);

    /**
     * Triggered when the user touches down on a suggestion. Only called for search suggestions.
     *
     * @param suggestion Touch-downed Suggestion.
     * @param position The position of the suggestion on the list.
     */
    void onSuggestionTouchDown(@NonNull AutocompleteMatch suggestion, int position);

    /**
     * Triggered when the user clicks one of the OmniboxActions attached to Suggestion.
     *
     * @param action the action the user interacted with
     * @param position The position of the associated suggestion.
     */
    void onOmniboxActionClicked(@NonNull OmniboxAction action, int position);

    /**
     * Triggered when the user long presses the omnibox suggestion. Deletes the entire
     * AutocompleteMatch. Execution of this method implies removal of the AutocompleteMatch.
     *
     * @param suggestion Long-pressed Suggestion.
     * @param titleText The title to display in the delete dialog.
     */
    void onDeleteMatch(@NonNull AutocompleteMatch suggestion, @NonNull String titleText);

    /**
     * Triggered when the user long presses the omnibox suggestion element (eg. tile). Performs
     * partial deletion of an AutocompleteMatch, focusing on the supplied element. Execution of this
     * method does not imply removal of the AutocompleteMatch.
     *
     * @param suggestion Long-pressed Suggestion.
     * @param titleText The title to display in the delete dialog.
     * @param element Element of the suggestion to be deleted.
     */
    void onDeleteMatchElement(
            @NonNull AutocompleteMatch suggestion, @NonNull String titleText, int element);

    /**
     * Triggered when the user selects a switch to tab action.
     *
     * @param suggestion Suggestion for which a corresponding tab is already open.
     * @param position The position of the suggestion on the list.
     */
    void onSwitchToTab(@NonNull AutocompleteMatch suggestion, int position);

    /**
     * Update the content of the Omnibox without triggering the Navigation.
     *
     * @param text The text to be displayed in the Omnibox.
     */
    void setOmniboxEditingText(@NonNull String text);

    /** Clear focus, close the suggestions list and complete the interaction with the Omnibox. */
    void finishInteraction();
}
