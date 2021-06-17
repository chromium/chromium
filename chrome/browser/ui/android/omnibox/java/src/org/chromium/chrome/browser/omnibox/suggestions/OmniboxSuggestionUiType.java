// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The different types of view that a suggestion can be. */
@IntDef({OmniboxSuggestionUiType.DEFAULT, OmniboxSuggestionUiType.EDIT_URL_SUGGESTION,
        OmniboxSuggestionUiType.ANSWER_SUGGESTION, OmniboxSuggestionUiType.ENTITY_SUGGESTION,
        OmniboxSuggestionUiType.TAIL_SUGGESTION, OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION,
        OmniboxSuggestionUiType.TILE_SUGGESTION, OmniboxSuggestionUiType.TILE_NAVSUGGEST})
@Retention(RetentionPolicy.SOURCE)
public @interface OmniboxSuggestionUiType {
    int DEFAULT = 0;
    int EDIT_URL_SUGGESTION = 1;
    int ANSWER_SUGGESTION = 2;
    int ENTITY_SUGGESTION = 3;
    int TAIL_SUGGESTION = 4;
    int CLIPBOARD_SUGGESTION = 5;
    int TILE_SUGGESTION = 6;
    int HEADER = 7;
    int TILE_NAVSUGGEST = 8;
}
