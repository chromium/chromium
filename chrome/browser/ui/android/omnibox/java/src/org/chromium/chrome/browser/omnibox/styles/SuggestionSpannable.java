// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.BackgroundColorSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.text.style.TextAppearanceSpan;
import android.text.style.UpdateAppearance;

/**
 * SpannableString that supplies .equals method for content comparison. This code assumes the use of
 * a limited subset of span types in suggestion text and may need revisiting if/when the scope of
 * these types expands (eg. different colors are introduced). With the limited set of Span features
 * we use this code is working well. When this is done, consider sharing this with
 * UrlBarMediator#isNewTextEquivalentToExistingText.
 */
public class SuggestionSpannable extends SpannableString {
    public SuggestionSpannable(CharSequence text) {
        super(text);
    }

    /**
     * Custom equals method that addresses some of the issues found in SpannableStringInternal:
     *
     * <ul>
     *   <li>getSpan may reorder returned spans that was not reflected in original code until
     *       http://b2/73359036 (http://shortn/_vKcawFBIoL)
     *   <li>Individual Span objects still do not necessarily override .equals themselves (eg.
     *       ForegroundColorSpan), making them always fail the comparison.
     * </ul>
     *
     * <p>This code simply addresses some issues in SpannableString.equals() method, therefore does
     * not come paired with hashCode() call. hashCode() is correctly supplied by SpannableString.
     */
    @Override
    public final boolean equals(Object obj) {
        if (!(obj instanceof SuggestionSpannable)) return false;

        final SuggestionSpannable other = (SuggestionSpannable) obj;
        if (!TextUtils.equals(this, other)) return false;
        if (TextUtils.isEmpty(this)) return true;

        final UpdateAppearance[] thisSpans = getSpans(0, length(), UpdateAppearance.class);
        final UpdateAppearance[] otherSpans =
                other.getSpans(0, other.length(), UpdateAppearance.class);

        if (thisSpans.length != otherSpans.length) return false;

        for (int i = 0; i < thisSpans.length; i++) {
            final UpdateAppearance thisSpan = thisSpans[i];
            final UpdateAppearance otherSpan = otherSpans[i];

            if (getSpanStart(thisSpan) != other.getSpanStart(otherSpan)
                    || getSpanEnd(thisSpan) != other.getSpanEnd(otherSpan)
                    || getSpanFlags(thisSpan) != other.getSpanFlags(otherSpan)
                    || thisSpan.getClass() != otherSpan.getClass()) {
                return false;
            }

            if (thisSpan instanceof ForegroundColorSpan) {
                ForegroundColorSpan ta1 = (ForegroundColorSpan) thisSpan;
                ForegroundColorSpan ta2 = (ForegroundColorSpan) otherSpan;
                if (ta1.getForegroundColor() != ta2.getForegroundColor()) {
                    return false;
                }
            } else if (thisSpan instanceof BackgroundColorSpan) {
                BackgroundColorSpan ta1 = (BackgroundColorSpan) thisSpan;
                BackgroundColorSpan ta2 = (BackgroundColorSpan) otherSpan;
                if (ta1.getBackgroundColor() != ta2.getBackgroundColor()) {
                    return false;
                }
            } else if (thisSpan instanceof StyleSpan) {
                StyleSpan ta1 = (StyleSpan) thisSpan;
                StyleSpan ta2 = (StyleSpan) otherSpan;
                if (ta1.getStyle() != ta2.getStyle()) {
                    return false;
                }
            } else if (thisSpan instanceof TextAppearanceSpan) {
                TextAppearanceSpan ta1 = (TextAppearanceSpan) thisSpan;
                TextAppearanceSpan ta2 = (TextAppearanceSpan) otherSpan;
                if (!TextUtils.equals(ta1.getFamily(), ta2.getFamily())
                        || !ta1.getLinkTextColor().equals(ta2.getLinkTextColor())
                        || !ta1.getTextColor().equals(ta2.getTextColor())
                        || ta1.getTextSize() != ta2.getTextSize()
                        || ta1.getTextStyle() != ta2.getTextStyle()) {
                    return false;
                }
            } else {
                assert false : "Unexpected visual span specified: " + thisSpan.getClass();
                return false;
            }
        }
        return true;
    }
}
