// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.LeadingMarginSpan;
import android.util.TypedValue;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.core.widget.TextViewCompat;

import org.chromium.components.content_creation.notes.ui.TextHighlightSpan;

/**
 * Model class for a template's text style.
 */
public class TextStyle {
    // Value in pixels of the leading padding when text has a highlight.
    private static final int HIGHLIGHT_LEADING_PADDING = 10;

    public final String fontName;
    public final int weight;

    private final @ColorInt int mFontColor;
    private final boolean mAllCaps;
    private final TextAlignment mAlignment;
    private final int mMinTextSizeSP;
    private final int mMaxTextSizeSP;
    private final @ColorInt int mHighlightColor;
    private final HighlightStyle mHighlightStyle;

    /** Constructor. */
    public TextStyle(String fontName, @ColorInt int fontColor, int weight, boolean allCaps,
            TextAlignment alignment, int minTextSizeSP, int maxTextSizeSP,
            @ColorInt int highlightColor, HighlightStyle highlightStyle) {
        this.fontName = fontName;
        this.weight = weight;

        this.mFontColor = fontColor;
        this.mAllCaps = allCaps;
        this.mAlignment = alignment;
        this.mMinTextSizeSP = minTextSizeSP;
        this.mMaxTextSizeSP = maxTextSizeSP;
        this.mHighlightColor = highlightColor;
        this.mHighlightStyle = highlightStyle;
    }

    /**
     * Returns true if this text style specifies a highlight color to draw behind the
     * text but on top of background colors.
     */
    public boolean hasHighlight() {
        // Sometimes colors' integers overflow, so negative numbers are valid.
        return this.mHighlightColor != 0 && this.mHighlightStyle != HighlightStyle.NONE;
    }

    /**
     * Applies the current styling to the |text| when setting it on |textView|.
     */
    public void apply(TextView textView, String text) {
        textView.setTextColor(this.mFontColor);
        textView.setAllCaps(this.mAllCaps);
        textView.setGravity(TextAlignment.toGravity(this.mAlignment));

        TextViewCompat.setAutoSizeTextTypeUniformWithConfiguration(textView, mMinTextSizeSP,
                mMaxTextSizeSP, /*autoSizeStepGranularity=*/1, TypedValue.COMPLEX_UNIT_SP);

        if (this.hasHighlight()) {
            int start = 0;
            int end = text.length();

            SpannableString spannableString = new SpannableString(text);

            boolean isRtl = textView.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
            TextHighlightSpan highlightSpan = new TextHighlightSpan(
                    this.mHighlightStyle, this.mHighlightColor, this.mAlignment, isRtl);
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
