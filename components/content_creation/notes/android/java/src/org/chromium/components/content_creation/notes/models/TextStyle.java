// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.LeadingMarginSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.ColorInt;

import org.chromium.components.content_creation.notes.ui.TextHighlightSpan;

/**
 * Model class for a template's text style.
 */
public class TextStyle {
    // Value in pixels of the leading padding when text has a highlight.
    private static final int HIGHLIGHT_LEADING_PADDING = 10;

    public final String fontName;
    public final @ColorInt int fontColor;
    public final int weight;
    public final boolean allCaps;
    public final TextAlignment alignment;
    public final @ColorInt int highlightColor;

    /** Constructor. */
    public TextStyle(String fontName, @ColorInt int fontColor, int weight, boolean allCaps,
            TextAlignment alignment, @ColorInt int highlightColor) {
        this.fontName = fontName;
        this.fontColor = fontColor;
        this.weight = weight;
        this.allCaps = allCaps;
        this.alignment = alignment;
        this.highlightColor = highlightColor;
    }

    /**
     * Returns true if this text style specifies a highlight color to draw behind the
     * text but on top of background colors.
     */
    public boolean hasHighlightColor() {
        // Sometimes colors' integers overflow, so negative numbers are valid.
        return this.highlightColor != 0;
    }

    /**
     * Applies the current styling to the |text| when setting it on |textView|.
     */
    public void apply(TextView textView, String text) {
        textView.setTextColor(this.fontColor);
        textView.setAllCaps(this.allCaps);
        textView.setGravity(TextAlignment.toGravity(this.alignment));

        if (this.hasHighlightColor()) {
            int start = 0;
            int end = text.length();

            SpannableString spannableString = new SpannableString(text);

            boolean isRtl = textView.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
            TextHighlightSpan highlightSpan =
                    new TextHighlightSpan(this.highlightColor, this.alignment, isRtl);
            spannableString.setSpan(highlightSpan, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

            // Needs a small leading margin span otherwise the highlight appears as if it was
            // clipped.
            LeadingMarginSpan.Standard marginSpan =
                    new LeadingMarginSpan.Standard(HIGHLIGHT_LEADING_PADDING);
            spannableString.setSpan(marginSpan, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

            textView.setText(spannableString, TextView.BufferType.SPANNABLE);
        } else {
            textView.setText(text);
        }
    }
}
