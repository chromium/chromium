// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with rendering the default suggestion view. */
public @interface SuggestionViewProperties {
    /** Whether suggestion is a search suggestion. */
    static final WritableBooleanPropertyKey IS_SEARCH_SUGGESTION = new WritableBooleanPropertyKey();

    /** The actual text content for the first line of text. */
    @VisibleForTesting
    static final WritableObjectPropertyKey<SuggestionSpannable> TEXT_LINE_1_TEXT =
            new WritableObjectPropertyKey<>();

    /** The actual text content for the second line of text. */
    @VisibleForTesting
    static final WritableObjectPropertyKey<SuggestionSpannable> TEXT_LINE_2_TEXT =
            new WritableObjectPropertyKey<>();

    /** Whether suggestions can wrap-around long search query to second line. */
    @VisibleForTesting
    static final WritableBooleanPropertyKey ALLOW_WRAP_AROUND = new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {
                IS_SEARCH_SUGGESTION, TEXT_LINE_1_TEXT, TEXT_LINE_2_TEXT, ALLOW_WRAP_AROUND
            };

    static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, BaseSuggestionViewProperties.ALL_KEYS);
}
