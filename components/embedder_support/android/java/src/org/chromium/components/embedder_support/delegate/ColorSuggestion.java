// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

/**
 * Color suggestion container used to store information for each color button that will be shown in
 * the simple color picker.
 */
public class ColorSuggestion {
    final int mColor;
    final String mLabel;

    /**
     * Constructs a color suggestion container.
     * @param color The suggested color.
     * @param label The label for the suggestion.
     */
    public ColorSuggestion(int color, String label) {
        mColor = color;
        mLabel = label;
    }
}
