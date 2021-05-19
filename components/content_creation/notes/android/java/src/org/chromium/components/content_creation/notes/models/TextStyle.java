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

    /** Constructor. */
    public TextStyle(String fontName, @ColorInt int fontColor, int weight, boolean allCaps,
            TextAlignment alignment) {
        this.fontName = fontName;
        this.fontColor = fontColor;
        this.weight = weight;
        this.allCaps = allCaps;
        this.alignment = alignment;
    }
}
