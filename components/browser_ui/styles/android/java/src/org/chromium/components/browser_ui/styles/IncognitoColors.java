// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.util.ColorUtils;

/**
 * Provides helper methods for fetching colors and text styles with a boolean param that will load
 * either the normal day/night adaptive dynamic resource, or the incognito baseline version. Each
 * use case should ensure they have intermediate semantic names before calling into this class. This
 * also means that any day/night divergence likely does not belong in this file.
 */
@NullMarked
public class IncognitoColors {
    /** {@see SemanticColorUtils#getColorSurfaceContainerHigh} */
    public static @ColorInt int getColorSurfaceContainerHigh(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.gm3_baseline_surface_container_high_dark)
                : SemanticColorUtils.getColorSurfaceContainerHigh(context);
    }

    /** {@see SemanticColorUtils#getColorSurfaceContainerLow} */
    public static @ColorInt int getColorSurfaceContainerLow(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.gm3_baseline_surface_container_low_dark)
                : SemanticColorUtils.getColorSurfaceContainerLow(context);
    }

    /** {@see SemanticColorUtils#getColorPrimaryContainer} */
    public static @ColorInt int getColorPrimaryContainer(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.baseline_primary_30)
                : SemanticColorUtils.getColorPrimaryContainer(context);
    }

    /** {@see SemanticColorUtils#getColorPrimary} */
    public static @ColorInt int getColorPrimary(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.baseline_primary_80)
                : SemanticColorUtils.getColorPrimary(context);
    }

    /** {@see SemanticColorUtils#getDefaultIconColor} */
    public static @ColorInt int getDefaultIconColor(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.baseline_neutral_90)
                : SemanticColorUtils.getDefaultIconColor(context);
    }

    /** {@see SemanticColorUtils#getDefaultIconColorSecondary} */
    public static @ColorInt int getDefaultIconColorSecondary(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.baseline_neutral_variant_80)
                : SemanticColorUtils.getDefaultIconColorSecondary(context);
    }

    /** {@see SemanticColorUtils#getColorOnSurface} */
    public static @ColorInt int getColorOnSurface(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.baseline_neutral_90)
                : SemanticColorUtils.getColorOnSurface(context);
    }

    /** Returns colorOnSurface but with the alpha channel set to 16%. */
    public static @ColorInt int getColorOnSurfaceWithAlpha16(Context context, boolean isIncognito) {
        @ColorInt int colorOnSurface = getColorOnSurface(context, isIncognito);
        return ColorUtils.setAlphaComponentWithFloat(colorOnSurface, /* alpha= */ 0.16f);
    }

    /** {@see SemanticColorUtils#getDividerLineBgColor} */
    public static @ColorInt int getDividerLineBgColor(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.divider_line_bg_color_light)
                : SemanticColorUtils.getDividerLineBgColor(context);
    }

    /** {@see SemanticColorUtils#getColorOnPrimary} */
    public static @ColorInt int getColorOnPrimary(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.baseline_primary_20)
                : SemanticColorUtils.getColorOnPrimary(context);
    }

    /** Returns the correct text appearance style res for primary colored medium text. */
    public static @StyleRes int getTextMediumPrimary(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextMedium_Primary_Baseline_Light
                : R.style.TextAppearance_TextMedium_Primary;
    }

    /** Returns the correct text appearance style res for TODO colored TODO text. */
    public static @StyleRes int getTextMediumThickPrimary(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextMediumThick_Primary_Baseline_Light
                : R.style.TextAppearance_TextMediumThick_Primary;
    }

    /** Returns the correct text appearance style res for TODO colored TODO text. */
    public static @StyleRes int getTextMediumThickAccent1(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextMediumThick_Blue_Baseline_Light
                : R.style.TextAppearance_TextMediumThick_Accent1;
    }

    /** Returns the correct text appearance style res for TODO colored TODO text. */
    public static @StyleRes int getTextMediumThickSecondary(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextMediumThick_Secondary_Baseline_Light
                : R.style.TextAppearance_TextMediumThick_Secondary;
    }
}
