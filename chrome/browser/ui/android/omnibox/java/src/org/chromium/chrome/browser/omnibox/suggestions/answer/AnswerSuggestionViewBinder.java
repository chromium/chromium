// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A mechanism binding AnswerSuggestion properties to its view. */
public class AnswerSuggestionViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT == propertyKey) {
            TextView tv = view.findViewById(R.id.omnibox_answer_line_1);
            tv.setText(model.get(AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT == propertyKey) {
            TextView tv = view.findViewById(R.id.omnibox_answer_line_2);
            tv.setText(model.get(AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION
                == propertyKey) {
            TextView tv = view.findViewById(R.id.omnibox_answer_line_1);
            tv.setContentDescription(
                    model.get(
                            AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION
                == propertyKey) {
            TextView tv = view.findViewById(R.id.omnibox_answer_line_2);
            tv.setContentDescription(
                    model.get(
                            AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES == propertyKey) {
            TextView tv = view.findViewById(R.id.omnibox_answer_line_1);
            tv.setMaxLines(model.get(AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES == propertyKey) {
            TextView tv = view.findViewById(R.id.omnibox_answer_line_2);
            tv.setMaxLines(model.get(AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES));
        } else if (AnswerSuggestionViewProperties.RIGHT_PADDING == propertyKey) {
            view.setPadding(0, 0, model.get(AnswerSuggestionViewProperties.RIGHT_PADDING), 0);
        }
    }
}
