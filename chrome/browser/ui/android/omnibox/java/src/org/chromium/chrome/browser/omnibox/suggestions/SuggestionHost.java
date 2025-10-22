// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.url.GURL;

/** A mechanism for creating {@link SuggestionViewDelegate}s. */
@NullMarked
public interface SuggestionHost {
    /**
     * Triggered when the user selects to refine one of the omnibox suggestions.
     *
     * @param suggestion Suggestion to use to refine Omnibox query.
     */
    void onRefineSuggestion(AutocompleteMatch suggestion);

    /**
     * Triggered when the user selects one of the omnibox suggestions to navigate to.
     *
     * @param suggestion User-selected Suggestion.
     * @param position The position of the suggestion on the list.
     * @param url The specific URL associated with the suggestion to navigate to.
     */
    void onSuggestionClicked(AutocompleteMatch suggestion, int position, GURL url);

    /**
     * Triggered when the user touches down on a suggestion. Only called for search suggestions.
     *
     * @param suggestion Touch-downed Suggestion.
     * @param position The position of the suggestion on the list.
     */
    void onSuggestionTouchDown(AutocompleteMatch suggestion, int position);

    /**
     * Triggered when the user clicks one of the OmniboxActions attached to Suggestion.
     *
     * @param action the action the user interacted with
     * @param position The position of the associated suggestion.
     */
    void onOmniboxActionClicked(OmniboxAction action, int position);

    /**
     * Triggered when the user long presses the omnibox suggestion. A delete confirmation dialog
     * will be shown.
     *
     * <p>Deletes the entire AutocompleteMatch. Execution of this method implies removal of the
     * AutocompleteMatch.
     *
     * @param suggestion The Suggestion to delete.
     * @param titleText The title to display in the delete dialog. Delete the match
     */
    void confirmDeleteMatch(AutocompleteMatch suggestion, String titleText);

    /**
     * Triggered when the user clicks on the remove button to delete the suggestion immediately.
     *
     * <p>Deletes the entire AutocompleteMatch. Execution of this method implies removal of the
     * AutocompleteMatch.
     *
     * @param suggestion The Suggestion to delete.
     */
    void deleteMatch(AutocompleteMatch suggestion);

    /**
     * Triggered when the user long presses the omnibox suggestion element (eg. tile). Performs
     * partial deletion of an AutocompleteMatch, focusing on the supplied element. Execution of this
     * method does not imply removal of the AutocompleteMatch.
     *
     * @param suggestion Long-pressed Suggestion.
     * @param titleText The title to display in the delete dialog.
     * @param element Element of the suggestion to be deleted.
     */
    void onDeleteMatchElement(AutocompleteMatch suggestion, String titleText, int element);

    /**
     * Update the content of the Omnibox without triggering the Navigation.
     *
     * @param text The text to be displayed in the Omnibox.
     */
    void setOmniboxEditingText(String text);

    /** Clear focus, close the suggestions list and complete the interaction with the Omnibox. */
    void finishInteraction();
}
