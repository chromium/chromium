// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import android.content.Context;

import androidx.annotation.AttrRes;
import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;

import com.google.android.material.color.MaterialColors;

/**
 * Provides semantic color values, typically in place of <macro>s which currently cannot be used in
 * Java code.
 */
public class SemanticColorUtils {
    private static final String TAG = "SemanticColorUtils";
    // Temporarily disabled flag because cached features cannot easily be read from components. For
    // testing changes this can be flipped to true. See https://crrev.com/c/3255853 for context.
    private static final boolean IS_FULL_DYNAMIC_COLORS = false;

    private static @ColorInt int resolve(
            @AttrRes int attrRes, @ColorRes int colorRes, Context context) {
        if (IS_FULL_DYNAMIC_COLORS) {
            return MaterialColors.getColor(context, attrRes, TAG);
        } else {
            return context.getResources().getColor(colorRes);
        }
    }

    /** Returns the semantic color value that corresponds to default_text_color. */
    public static @ColorInt int getDefaultTextColor(Context context) {
        return resolve(R.attr.colorOnSurface, R.color.default_text_color_baseline, context);
    }

    /** Returns the semantic color value that corresponds to default_text_color_accent1. */
    public static @ColorInt int getDefaultTextColorAccent1(Context context) {
        return resolve(R.attr.colorPrimary, R.color.default_text_color_blue, context);
    }

    /** Returns the semantic color value that corresponds to default_icon_color_accent1. */
    public static @ColorInt int getDefaultIconColorAccent1(Context context) {
        return resolve(R.attr.colorPrimary, R.color.default_icon_color_accent1_baseline, context);
    }
}
