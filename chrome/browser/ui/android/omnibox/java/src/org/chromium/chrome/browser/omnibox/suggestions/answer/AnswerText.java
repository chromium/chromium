// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.text.SpannableStringBuilder;

/**
 * AnswerText specifies details to be presented in a single line of an omnibox suggestion with an
 * associated answer.
 */
public interface AnswerText {

    /** Content of the line of text in omnibox suggestion. */
    SpannableStringBuilder getText();

    /**
     * Accessibility description - used to announce the details of the answer. This carries text to
     * be read out loud to the user when talkback mode is enabled. Content of the Accessibility
     * Description may be different from the content of presented string:
     *
     * <ul>
     *   <li>visually we want to highlight the answer part of AiS suggestion,
     *   <li>audibly we want to make sure the AiS suggestion is clear to understand.
     * </ul>
     *
     * This frequently means we are presenting answers in different order than we're announcing
     * them.
     */
    String getAccessibilityDescription();

    /** How many additional lines content can wrap around to present more details. */
    int getMaxLines();
}
