// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import java.util.Optional;

/**
 * A class that can be observed to be notified of changes to the visual state of the omnibox
 * suggestions.
 */
public interface OmniboxSuggestionsVisualState {

    /**
     * Sets the current observer to be notified of changes to the visual state of the omnibox
     * suggestions. To simply remove the current observer, pass in an empty Optional.
     */
    void setOmniboxSuggestionsVisualStateObserver(
            Optional<AutocompleteCoordinator.OmniboxSuggestionsVisualStateObserver>
                    omniboxSuggestionsVisualStateObserver);
}
