// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

import androidx.annotation.ColorInt;

/**
 * Model class for a template's text style.
 */
public class TextStyle {
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

        if (highlightColor > 0) {
            this.highlightColor = highlightColor;
        } else {
            this.highlightColor = 0;
        }
    }

    /**
     * Returns true if this text style specifies a highlight color to draw behind the
     * text but on top of background colors.
     */
    public boolean hasHighlightColor() {
        return this.highlightColor > 0;
    }
}
