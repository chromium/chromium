// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.widget.ListView;

/**
 * Utility methods providing access to package-private methods in {@link AutocompleteCoordinator}
 * for tests.
 */
public class AutocompleteCoordinatorTestUtils {
    /**
     * Sets the autocomplete controller for the location bar.
     *
     * @param controller The controller that will handle autocomplete/omnibox suggestions.
     * @note Only used for testing.
     */
    public static void setAutocompleteController(
            AutocompleteCoordinator coordinator, AutocompleteController controller) {
        ((AutocompleteCoordinatorImpl) coordinator).setAutocompleteController(controller);
    }

    /** Allows injecting autocomplete suggestions for testing. */
    public static AutocompleteController.OnSuggestionsReceivedListener
    getSuggestionsReceivedListenerForTest(AutocompleteCoordinator coordinator) {
        return ((AutocompleteCoordinatorImpl) coordinator).getSuggestionsReceivedListenerForTest();
    }

    /**
     * @return The suggestion list popup containing the omnibox results (or null if it has not yet
     *         been created).
     */
    public static ListView getSuggestionList(AutocompleteCoordinator coordinator) {
        return ((AutocompleteCoordinatorImpl) coordinator).getSuggestionList();
    }
}
