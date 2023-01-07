// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.ui;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.text.style.LineBackgroundSpan;

import androidx.annotation.ColorInt;

import org.chromium.components.content_creation.notes.models.HighlightStyle;
import org.chromium.components.content_creation.notes.models.TextAlignment;

/**
 * Class used to draw colored highlight behind text.
 */
public class TextHighlightSpan implements LineBackgroundSpan {
    private static final int RIGHT_PADDING = 10;

    private final HighlightStyle mStyle;
    private final @ColorInt int mColor;
    private final boolean mIsCentered;
    private final boolean mIsLeftAligned;

    /** Constructor. */
    public TextHighlightSpan(
            HighlightStyle style, @ColorInt int color, TextAlignment alignment, boolean isRtl) {
        assert style != HighlightStyle.NONE;

        this.mStyle = style;
        this.mColor = color;
        this.mIsCentered = alignment == TextAlignment.CENTER;
        this.mIsLeftAligned = (!isRtl && alignment == TextAlignment.START)
                || (isRtl && alignment == TextAlignment.END);
    }

    @Override
    public void drawBackground(Canvas canvas, Paint paint, int left, int right, int top,
            int baseline, int bottom, CharSequence text, int start, int end, int lineNumber) {
        CharSequence lineText = text.subSequence(start, end);
        if (lineText.toString().trim().length() == 0) {
            // No need to draw a background for lines with no characters.
            return;
        }

        final int textWidth = Math.round(paint.measureText(text, start, end));
        final int paintColor = paint.getColor();

        paint.setColor(this.mColor);

        if (this.mIsCentered) {
            int middle = ((right - left) / 2) + left;
            int textWidthOffset = (textWidth / 2);
            left = middle - textWidthOffset;
            right = middle + textWidthOffset;
        } else if (this.mIsLeftAligned) {
            right = left + textWidth;
        } else {
            left = right - textWidth;
        }

        if (this.mStyle == HighlightStyle.HALF) {
            top = top + (bottom - top) / 2;
        }

        // Add some padding to the right to prevent most of the clipping. It
        // will still occur when the line takes the full width of the canvas,
        // but it looks much better like this.
        canvas.drawRect(left, top, right + RIGHT_PADDING, bottom, paint);

        // Reset initial paint color to ensure the text gets drawn properly.
        paint.setColor(paintColor);
    }
}