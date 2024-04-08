// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.NonNull;

import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A processor of omnibox suggestions. Implementers are provided the opportunity to analyze a
 * suggestion and create a custom model.
 */
public interface SuggestionProcessor extends DropdownItemProcessor {
    /**
     * @param suggestion The suggestion to process.
     * @param position The position of the suggestion in the list.
     * @return Whether this suggestion processor handles this type of suggestion at this position.
     */
    boolean doesProcessSuggestion(@NonNull AutocompleteMatch suggestion, int position);

    /**
     * Populate a model for the given suggestion.
     *
     * @param suggestion The suggestion to populate the model for.
     * @param model The model to populate.
     * @param position The position of the suggestion in the list.
     */
    void populateModel(
            @NonNull AutocompleteMatch suggestion, @NonNull PropertyModel model, int position);
}
