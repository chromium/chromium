// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.function.Supplier;

/** The properties associated with rendering the default suggestion view. */
@NullMarked
public @interface SuggestionViewProperties {
    /**
     * Lazily-evaluated custom content description for the suggestion view.
     *
     * <p>Formatting the accessibility announcement is comparatively expensive (string resource
     * lookup, multi-argument {@code String.format}) and is paid for every row on every suggestion
     * update. The supplier is only resolved by the view binder when an accessibility service is
     * running; otherwise the description is left unset.
     */
    @VisibleForTesting
    WritableObjectPropertyKey<Supplier<String>> CONTENT_DESCRIPTION_SUPPLIER =
            new WritableObjectPropertyKey<>();

    /** Whether suggestion is a search suggestion. */
    WritableBooleanPropertyKey IS_SEARCH_SUGGESTION = new WritableBooleanPropertyKey();

    /** The actual text content for the first line of text. */
    @VisibleForTesting
    WritableObjectPropertyKey<SuggestionSpannable> TEXT_LINE_1_TEXT =
            new WritableObjectPropertyKey<>();

    /** Whether suggestions can wrap-around long search query to second line. */
    @VisibleForTesting
    WritableBooleanPropertyKey TEXT_LINE_1_WRAP = new WritableBooleanPropertyKey();

    /** The actual text content for the second line of text. */
    @VisibleForTesting
    WritableObjectPropertyKey<SuggestionSpannable> TEXT_LINE_2_TEXT =
            new WritableObjectPropertyKey<>();

    /** Whether suggestions can wrap-around secondary text to second line. */
    @VisibleForTesting
    WritableBooleanPropertyKey TEXT_LINE_2_WRAP = new WritableBooleanPropertyKey();

    PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {
                CONTENT_DESCRIPTION_SUPPLIER,
                IS_SEARCH_SUGGESTION,
                TEXT_LINE_1_TEXT,
                TEXT_LINE_1_WRAP,
                TEXT_LINE_2_TEXT,
                TEXT_LINE_2_WRAP
            };

    PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, BaseSuggestionViewProperties.ALL_KEYS);
}
