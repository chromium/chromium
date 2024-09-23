// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.text.Html;

import org.chromium.components.omnibox.AnswerTypeProto.AnswerType;

/** Shared logic for implementations of {@link AnswerText} */
class AnswerTextUtils {
    // Utils class, no member state.
    private AnswerTextUtils() {}

    /**
     * Process, if applicable, the content of the answer text, modifying it to improve readability.
     *
     * @param text Source text.
     * @param isAnswerLine Whether the text represents an answer to the query.
     * @param answerType The type of answer.
     * @return Text stripped of HTML tags and, if applicable, shortened to make currency answers
     *     more readable.
     */
    static String processAnswerText(String text, boolean isAnswerLine, AnswerType answerType) {
        text = Html.fromHtml(text, Html.FROM_HTML_MODE_LEGACY).toString();
        if (isAnswerLine && answerType == AnswerType.ANSWER_TYPE_CURRENCY) {
            // Modify the content of answer to present only the value after conversion, that is:
            //    1,000 United State Dollar = 1,330.75 Canadian Dollar
            // becomes
            //    1,330.75 Canadian Dollar
            int offset = text.indexOf(" = ");
            if (offset > 0) {
                text = text.substring(offset + 3);
            }
        }
        return text;
    }
}
