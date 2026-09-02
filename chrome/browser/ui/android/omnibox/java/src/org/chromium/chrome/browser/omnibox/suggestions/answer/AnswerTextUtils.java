// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.text.Html;

import org.chromium.build.annotations.NullMarked;

/** Shared logic for implementations of {@link AnswerText}. */
@NullMarked
class AnswerTextUtils {
    // Utils class, no member state.
    private AnswerTextUtils() {}

    /**
     * Process, if applicable, the content of the answer text, modifying it to improve readability.
     *
     * @param text Source text.
     * @return Text stripped of HTML tags
     */
    static String processAnswerText(String text) {
        return Html.fromHtml(text, Html.FROM_HTML_MODE_LEGACY).toString();
    }
}
