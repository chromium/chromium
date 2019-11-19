// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.graphics.Color;

/**
 * Helper functions for working with colors.
 */
public class ColorUtils {
    private static final float CONTRAST_LIGHT_ITEM_THRESHOLD = 3f;
    private static final float LIGHTNESS_OPAQUE_BOX_THRESHOLD = 0.82f;
    private static final float MAX_LUMINANCE_FOR_VALID_THEME_COLOR = 0.94f;
    private static final float THEMED_FOREGROUND_BLACK_FRACTION = 0.64f;

    /** Percentage to darken a color by when setting the status bar color. */
    private static final float DARKEN_COLOR_FRACTION = 0.6f;

    /**
     * Computes the lightness value in HSL standard for the given color.
     */
    public static float getLightnessForColor(int color) {
        int red = Color.red(color);
        int green = Color.green(color);
        int blue = Color.blue(color);
        int largest = Math.max(red, Math.max(green, blue));
        int smallest = Math.min(red, Math.min(green, blue));
        int average = (largest + smallest) / 2;
        return average / 255.0f;
    }

    /**
     * Calculates the contrast between the given color and white, using the algorithm provided by
     * the WCAG v2 in http://www.w3.org/TR/WCAG20/#contrast-ratiodef.
     */
    private static float getContrastForColor(int color) {
        float bgR = Color.red(color) / 255f;
        float bgG = Color.green(color) / 255f;
        float bgB = Color.blue(color) / 255f;
        bgR = (bgR < 0.03928f) ? bgR / 12.92f : (float) Math.pow((bgR + 0.055f) / 1.055f, 2.4f);
        bgG = (bgG < 0.03928f) ? bgG / 12.92f : (float) Math.pow((bgG + 0.055f) / 1.055f, 2.4f);
        bgB = (bgB < 0.03928f) ? bgB / 12.92f : (float) Math.pow((bgB + 0.055f) / 1.055f, 2.4f);
        float bgL = 0.2126f * bgR + 0.7152f * bgG + 0.0722f * bgB;
        return Math.abs((1.05f) / (bgL + 0.05f));
    }

    /**
     * Get a color when overlayed with a different color.
     * @param baseColor The base Android color.
     * @param overlayColor The overlay Android color.
     * @param overlayAlpha The alpha |overlayColor| should have on the base color.
     */
    public static int getColorWithOverlay(int baseColor, int overlayColor, float overlayAlpha) {
        return Color.rgb((int) MathUtils.interpolate(
                                 Color.red(baseColor), Color.red(overlayColor), overlayAlpha),
                (int) MathUtils.interpolate(
                        Color.green(baseColor), Color.green(overlayColor), overlayAlpha),
                (int) MathUtils.interpolate(
                        Color.blue(baseColor), Color.blue(overlayColor), overlayAlpha));
    }

    /**
     * Darkens the given color to use on the status bar.
     * @param color Color which should be darkened.
     * @return Color that should be used for Android status bar.
     */
    public static int getDarkenedColorForStatusBar(int color) {
        return getDarkenedColor(color, DARKEN_COLOR_FRACTION);
    }

    /**
     * Darken a color to a fraction of its current brightness.
     * @param color The input color.
     * @param darkenFraction The fraction of the current brightness the color should be.
     * @return The new darkened color.
     */
    public static int getDarkenedColor(int color, float darkenFraction) {
        float[] hsv = new float[3];
        Color.colorToHSV(color, hsv);
        hsv[2] *= darkenFraction;
        return Color.HSVToColor(hsv);
    }

    /**
     * Check whether lighter or darker foreground elements (i.e. text, drawables etc.)
     * should be used depending on the given background color.
     * @param backgroundColor The background color value which is being queried.
     * @return Whether light colored elements should be used.
     */
    public static boolean shouldUseLightForegroundOnBackground(int backgroundColor) {
        return getContrastForColor(backgroundColor) >= CONTRAST_LIGHT_ITEM_THRESHOLD;
    }

    /**
     * Check which version of the textbox background should be used depending on the given
     * color.
     * @param color The color value we are querying for.
     * @return Whether the transparent version of the background should be used.
     */
    public static boolean shouldUseOpaqueTextboxBackground(int color) {
        return getLightnessForColor(color) > LIGHTNESS_OPAQUE_BOX_THRESHOLD;
    }

    /**
     * Returns an opaque version of the given color.
     * @param color Color for which an opaque version should be returned.
     * @return Opaque version of the given color.
     */
    public static int getOpaqueColor(int color) {
        return Color.rgb(Color.red(color), Color.green(color), Color.blue(color));
    }

    /**
     * Determine if a theme color is valid. A theme color is invalid if its luminance is > 0.94.
     * @param color The color to test.
     * @return True if the theme color is valid.
     */
    public static boolean isValidThemeColor(int color) {
        return ColorUtils.getLightnessForColor(color) <= MAX_LUMINANCE_FOR_VALID_THEME_COLOR;
    }

    /**
     * Compute a color to use for assets that sit on top of a themed background.
     * @param themeColor The base theme color.
     * @return A color to use for elements in the foreground (on top of the base theme color).
     */
    public static int getThemedAssetColor(int themeColor, boolean isIncognito) {
        if (ColorUtils.shouldUseLightForegroundOnBackground(themeColor) || isIncognito) {
            // Dark theme.
            return Color.WHITE;
        } else {
            // Light theme.
            return ColorUtils.getColorWithOverlay(
                    themeColor, Color.BLACK, THEMED_FOREGROUND_BLACK_FRACTION);
        }
    }
}
