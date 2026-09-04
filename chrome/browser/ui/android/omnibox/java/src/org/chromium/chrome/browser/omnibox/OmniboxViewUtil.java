// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.text.Spanned;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.components.omnibox.PageClassificationUtils;

/** Utilities for the Omnibox view component. */
@NullMarked
public class OmniboxViewUtil {

    /**
     * Returns whether {@code currentText} and {@code newText} contain equivalent spans of {@code
     * spanType}. Other span types are deliberately ignored because TextView and the IME attach
     * transient spans that do not represent Omnibox display state.
     */
    static <T> boolean haveEquivalentSpans(
            CharSequence currentText, CharSequence newText, Class<T> spanType) {
        if (!(currentText instanceof Spanned)) {
            return !(newText instanceof Spanned)
                    || ((Spanned) newText).getSpans(0, newText.length(), spanType).length == 0;
        }

        Spanned currentSpannedText = (Spanned) currentText;
        T[] currentSpans = currentSpannedText.getSpans(0, currentText.length(), spanType);
        if (!(newText instanceof Spanned)) return currentSpans.length == 0;

        Spanned newSpannedText = (Spanned) newText;
        T[] newSpans = newSpannedText.getSpans(0, newText.length(), spanType);
        if (currentSpans.length != newSpans.length) return false;

        for (int i = 0; i < currentSpans.length; i++) {
            T currentSpan = currentSpans[i];
            T newSpan = newSpans[i];
            if (!currentSpan.equals(newSpan)
                    || currentSpannedText.getSpanStart(currentSpan)
                            != newSpannedText.getSpanStart(newSpan)
                    || currentSpannedText.getSpanEnd(currentSpan)
                            != newSpannedText.getSpanEnd(newSpan)
                    || currentSpannedText.getSpanFlags(currentSpan)
                            != newSpannedText.getSpanFlags(newSpan)) {
                return false;
            }
        }

        return true;
    }

    /**
     * Sanitizing the given string to be safe to paste into the omnibox.
     *
     * @param clipboardString The string from the clipboard.
     * @return The sanitized version of the string.
     */
    public static String sanitizeTextForPaste(String clipboardString) {
        return OmniboxViewUtilJni.get().sanitizeTextForPaste(clipboardString);
    }

    /**
     * Returns whether the given page classification represents a regular tab context (i.e., not the
     * Hub, Custom Tabs, or Co-Browsing Composebox).
     *
     * @param pageClassification The PageClassification value to check.
     * @return True if it is a regular tab context.
     */
    public static boolean isRegularTabContext(@PageClassification int pageClassification) {
        // TODO(crbug.com/507471408): Revisit logic to guard it more strictly.
        return !PageClassificationUtils.isHubOrTabSearch(pageClassification)
                && pageClassification != PageClassification.OTHER_ON_CCT
                && pageClassification != PageClassification.ANDROID_SEARCH_WIDGET
                && pageClassification != PageClassification.ANDROID_SHORTCUTS_WIDGET;
    }

    @NativeMethods
    interface Natives {
        String sanitizeTextForPaste(String clipboardString);
    }
}
