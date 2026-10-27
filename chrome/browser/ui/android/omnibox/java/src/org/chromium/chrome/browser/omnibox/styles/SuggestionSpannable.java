// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.BackgroundColorSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.ImageSpan;
import android.text.style.StyleSpan;
import android.text.style.TextAppearanceSpan;
import android.text.style.UpdateAppearance;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * SpannableString that supplies .equals method for content comparison. This code assumes the use of
 * a limited subset of span types in suggestion text and may need revisiting if/when the scope of
 * these types expands (eg. different colors are introduced). With the limited set of Span features
 * we use this code is working well. When this is done, consider sharing this with
 * UrlBarMediator#isNewTextEquivalentToExistingText.
 */
@NullMarked
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
     * <p>Only spans implementing {@link UpdateAppearance} are compared. Of those, the types handled
     * by {@link #contentEquals} (ForegroundColorSpan, BackgroundColorSpan, StyleSpan,
     * TextAppearanceSpan, ImageSpan) are compared structurally, and any other UpdateAppearance span
     * causes the strings to be reported as different. Spans that do not implement UpdateAppearance
     * (e.g. paragraph spans such as AlignmentSpan or LeadingMarginSpan) are ignored, so suggestion
     * text must not rely on them without extending this method.
     *
     * <p>This code simply addresses some issues in SpannableString.equals() method, therefore does
     * not come paired with hashCode() call. hashCode() is correctly supplied by SpannableString.
     */
    @Override
    public final boolean equals(@Nullable Object obj) {
        return obj instanceof SuggestionSpannable other && contentEquals(this, other);
    }

    /**
     * Whether {@code a} and {@code b} render identically: same characters and, position for
     * position, the same visual spans with the same extents, flags and attributes. Spans are
     * compared in the order {@link Spanned#getSpans} returns them (insertion order), so two strings
     * whose equal spans were inserted in a different order compare as different; that is a false
     * negative, never a false positive.
     *
     * <p>Useful for comparing a {@link SuggestionSpannable} against the copy a {@link
     * android.widget.TextView} keeps after {@code setText}, which preserves span order.
     *
     * @param a First text to compare.
     * @param b Second text to compare.
     * @return true if both texts would render identically.
     */
    public static boolean contentEquals(Spanned a, Spanned b) {
        if (!TextUtils.equals(a, b)) return false;
        if (TextUtils.isEmpty(a)) return true;

        final UpdateAppearance[] aSpans = a.getSpans(0, a.length(), UpdateAppearance.class);
        final UpdateAppearance[] bSpans = b.getSpans(0, b.length(), UpdateAppearance.class);

        if (aSpans.length != bSpans.length) return false;

        for (int i = 0; i < aSpans.length; i++) {
            final UpdateAppearance aSpan = aSpans[i];
            final UpdateAppearance bSpan = bSpans[i];

            if (a.getSpanStart(aSpan) != b.getSpanStart(bSpan)
                    || a.getSpanEnd(aSpan) != b.getSpanEnd(bSpan)
                    || a.getSpanFlags(aSpan) != b.getSpanFlags(bSpan)
                    || aSpan.getClass() != bSpan.getClass()) {
                return false;
            }

            switch (aSpan) {
                case ForegroundColorSpan ta1 -> {
                    ForegroundColorSpan ta2 = (ForegroundColorSpan) bSpan;
                    if (ta1.getForegroundColor() != ta2.getForegroundColor()) {
                        return false;
                    }
                }
                case BackgroundColorSpan ta1 -> {
                    BackgroundColorSpan ta2 = (BackgroundColorSpan) bSpan;
                    if (ta1.getBackgroundColor() != ta2.getBackgroundColor()) {
                        return false;
                    }
                }
                case StyleSpan ta1 -> {
                    StyleSpan ta2 = (StyleSpan) bSpan;
                    if (ta1.getStyle() != ta2.getStyle()) {
                        return false;
                    }
                }
                case TextAppearanceSpan ta1 -> {
                    TextAppearanceSpan ta2 = (TextAppearanceSpan) bSpan;
                    if (!TextUtils.equals(ta1.getFamily(), ta2.getFamily())
                            || !ta1.getLinkTextColor().equals(ta2.getLinkTextColor())
                            || !ta1.getTextColor().equals(ta2.getTextColor())
                            || ta1.getTextSize() != ta2.getTextSize()
                            || ta1.getTextStyle() != ta2.getTextStyle()) {
                        return false;
                    }
                }
                case ImageSpan ia -> {
                    ImageSpan ib = (ImageSpan) bSpan;
                    // Drawables have no meaningful equals(); only identical instances are known
                    // to render the same. Anything else costs at most one extra setText.
                    if (ia.getVerticalAlignment() != ib.getVerticalAlignment()
                            || ia.getDrawable() != ib.getDrawable()) {
                        return false;
                    }
                }
                default -> {
                    // Unknown span type: we cannot prove the two render identically, so report them
                    // as different. This only costs an unnecessary TextView.setText.
                    return false;
                }
            }
        }
        return true;
    }
}
