// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.suggestions;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The different types of view that a suggestion can be.
 *
 * <p>When modifying this list, please also update the - {@link PreWarmingRecycledViewPool}, -
 * OmniboxSuggestionUiType histogram enum to reflect the expected/anticipated volume of views that
 * may be reused and appropriate histogram details.
 *
 * <p>Please note that the types below are also being recorded in a separate histogram, see: -
 * SuggestionsMetrics#recordSuggestionsViewCreatedType() -
 * SuggestionsMetrics#recordSuggestionsViewReusedType().
 */
@IntDef({
    OmniboxSuggestionUiType.DEFAULT,
    OmniboxSuggestionUiType.EDIT_URL_SUGGESTION,
    OmniboxSuggestionUiType.ANSWER_SUGGESTION,
    OmniboxSuggestionUiType.ENTITY_SUGGESTION,
    OmniboxSuggestionUiType.TAIL_SUGGESTION,
    OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION,
    OmniboxSuggestionUiType.HEADER,
    OmniboxSuggestionUiType.TILE_NAVSUGGEST,
    OmniboxSuggestionUiType.GROUP_SEPARATOR,
    OmniboxSuggestionUiType.QUERY_TILES,
    OmniboxSuggestionUiType.COUNT
})
@Retention(RetentionPolicy.SOURCE)
public @interface OmniboxSuggestionUiType {
    int DEFAULT = 0;
    int EDIT_URL_SUGGESTION = 1;
    int ANSWER_SUGGESTION = 2;
    int ENTITY_SUGGESTION = 3;
    int TAIL_SUGGESTION = 4;
    int CLIPBOARD_SUGGESTION = 5;
    int HEADER = 6;
    int TILE_NAVSUGGEST = 7;
    int GROUP_SEPARATOR = 8;
    int QUERY_TILES = 9;

    int COUNT = 10;
}
