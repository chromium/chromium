// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.text.Spannable;

import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties associated with rendering the answer suggestion view.
 */
class AnswerSuggestionViewProperties {
    /** The maximum number of lines to be shown for the first line of text. */
    public static final WritableIntPropertyKey TEXT_LINE_1_MAX_LINES = new WritableIntPropertyKey();
    /** The actual text content for the first line of text. */
    public static final WritableObjectPropertyKey<Spannable> TEXT_LINE_1_TEXT =
            new WritableObjectPropertyKey<>();
    /** The accessibility description to be announced with this line. */
    public static final WritableObjectPropertyKey<String> TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** The maximum number of lines to be shown for the second line of text. */
    public static final WritableIntPropertyKey TEXT_LINE_2_MAX_LINES = new WritableIntPropertyKey();
    /** The actual text content for the second line of text. */
    public static final WritableObjectPropertyKey<Spannable> TEXT_LINE_2_TEXT =
            new WritableObjectPropertyKey<>();
    /** The accessibility description to be announced with this line. */
    public static final WritableObjectPropertyKey<String> TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {TEXT_LINE_1_TEXT,
            TEXT_LINE_1_MAX_LINES, TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION, TEXT_LINE_2_TEXT,
            TEXT_LINE_2_MAX_LINES, TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, BaseSuggestionViewProperties.ALL_KEYS);
}
