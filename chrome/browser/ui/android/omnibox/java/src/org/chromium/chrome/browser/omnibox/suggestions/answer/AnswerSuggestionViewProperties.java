// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.text.Spannable;

import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with rendering the answer suggestion view. */
@interface AnswerSuggestionViewProperties {
    /** The maximum number of lines to be shown for the first line of text. */
    static final WritableIntPropertyKey TEXT_LINE_1_MAX_LINES = new WritableIntPropertyKey();

    /** The actual text content for the first line of text. */
    static final WritableObjectPropertyKey<Spannable> TEXT_LINE_1_TEXT =
            new WritableObjectPropertyKey<>();

    /** The accessibility description to be announced with this line. */
    static final WritableObjectPropertyKey<String> TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** The maximum number of lines to be shown for the second line of text. */
    static final WritableIntPropertyKey TEXT_LINE_2_MAX_LINES = new WritableIntPropertyKey();

    /** The actual text content for the second line of text. */
    static final WritableObjectPropertyKey<Spannable> TEXT_LINE_2_TEXT =
            new WritableObjectPropertyKey<>();

    /** The accessibility description to be announced with this line. */
    static final WritableObjectPropertyKey<String> TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /**
     * The right padding to apply to the answer suggestion content view. This is distinct from the
     * padding for the containing BaseSuggestionView, which is controlled separately.
     */
    WritableIntPropertyKey RIGHT_PADDING = new WritableIntPropertyKey();

    static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {
                TEXT_LINE_1_TEXT,
                TEXT_LINE_1_MAX_LINES,
                TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION,
                TEXT_LINE_2_TEXT,
                TEXT_LINE_2_MAX_LINES,
                TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION,
                RIGHT_PADDING,
            };

    static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, BaseSuggestionViewProperties.ALL_KEYS);
}
