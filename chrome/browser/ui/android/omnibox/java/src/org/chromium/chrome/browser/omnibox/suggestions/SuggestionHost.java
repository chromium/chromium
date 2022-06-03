// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.NonNull;

import org.chromium.components.omnibox.AutocompleteMatch;
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
     * Triggered when the user long presses the omnibox suggestion.
     *
     * @param suggestion Long-pressed Suggestion.
     * @param position The position of the suggestion on the list.
     */
    void onSuggestionLongClicked(@NonNull AutocompleteMatch suggestion, int position);

    /**
     * Triggered when the user selects a switch to tab action.
     *
     * @param suggestion Suggestion for which a corresponding tab is already open.
     * @param position The position of the suggestion on the list.
     */
    void onSwitchToTab(@NonNull AutocompleteMatch suggestion, int position);

    /**
     * Toggle expanded state of suggestion items belonging to specific group.
     *
     * @param groupId ID of Suggestion Group whose visibility changed.
     * @param isCollapsed True if group should appear collapsed, otherwise false.
     */
    void setGroupCollapsedState(int groupId, boolean isCollapsed);

    /**
     * Update the content of the Omnibox without triggering the Navigation.
     *
     * @param text The text to be displayed in the Omnibox.
     */
    void setOmniboxEditingText(@NonNull String text);
}
