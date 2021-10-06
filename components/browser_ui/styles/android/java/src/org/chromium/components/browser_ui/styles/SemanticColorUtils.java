// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import android.content.Context;

import androidx.annotation.ColorInt;

import com.google.android.material.color.MaterialColors;

/**
 * Provides semantic color values, typically in place of <macro>s which currently cannot be used in
 * Java code.
 */
public class SemanticColorUtils {
    private static final String TAG = "SemanticColorUtils";

    /**
     * Returns the semantic color value that corresponds to default_text_color.
     * @param context The {@link Context}.
     * @return The color.
     */
    public static @ColorInt int getDefaultTextColor(Context context) {
        return MaterialColors.getColor(context, R.attr.colorOnSurface, TAG);
    }

    /**
     * Returns the semantic color value that corresponds to default_text_color_accent1.
     * @param context The {@link Context}.
     * @return The color.
     */
    public static @ColorInt int getDefaultTextColorAccent1(Context context) {
        return MaterialColors.getColor(context, R.attr.colorPrimary, TAG);
    }
}
