// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;

/**
 * Provides helper methods for fetching colors and text styles with a boolean param that will load
 * either the normal day/night adaptive dynamic resource, or the incognito baseline version. Each
 * use case should ensure they have intermediate semantic names before calling into this class. This
 * also means that any day/night divergence likely does not belong in this file.
 */
@NullMarked
public class IncognitoColors {
    /** {@see SemanticColorUtils#getColorSurface} */
    public static @ColorInt int getColorSurface(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.gm3_baseline_surface_dark)
                : SemanticColorUtils.getColorSurface(context);
    }

    /** {@see SemanticColorUtils#getColorSurfaceBright} */
    public static @ColorInt int getColorSurfaceBright(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.gm3_baseline_surface_bright_dark)
                : SemanticColorUtils.getColorSurfaceBright(context);
    }

    /** {@see SemanticColorUtils#getColorSurfaceContainerHigh} */
    public static @ColorInt int getColorSurfaceContainerHigh(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.gm3_baseline_surface_container_high_dark)
                : SemanticColorUtils.getColorSurfaceContainerHigh(context);
    }

    /** {@see SemanticColorUtils#getColorSurfaceContainerHighest} */
    public static @ColorInt int getColorSurfaceContainerHighest(
            Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.gm3_baseline_surface_container_highest_dark)
                : SemanticColorUtils.getColorSurfaceContainerHighest(context);
    }

    /** {@see SemanticColorUtils#getColorSurfaceContainerLow} */
    public static @ColorInt int getColorSurfaceContainerLow(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.gm3_baseline_surface_container_low_dark)
                : SemanticColorUtils.getColorSurfaceContainerLow(context);
    }

    /** {@see SemanticColorUtils#getInteractableChipBgColor} */
    public static @ColorInt int getInteractableChipBgColor(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.gm3_baseline_surface_container_high_dark)
                : SemanticColorUtils.getInteractableChipBgColor(context);
    }

    /** Returns a color state list for the surface container color. */
    public static ColorStateList getColorSurfaceContainerTintList(
            Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColorStateList(R.color.color_surface_container_incognito_tint_list)
                : SemanticColorUtils.getColorSurfaceContainerTintList(context);
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

    /** {@see SemanticColorUtils#getDividerLineBgColor} */
    public static @ColorInt int getDividerLineBgColor(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.divider_color_light)
                : SemanticColorUtils.getDividerColor(context);
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

    /**
     * Returns the correct text appearance style res for primary colored medium text on an accent 1
     * container.
     */
    public static @StyleRes int getTextMediumPrimaryOnAccent1Container(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextMedium_OnAccent1Container_Baseline_Light
                : R.style.TextAppearance_TextMedium_OnAccent1Container;
    }

    /** Returns the correct text appearance style res for primary colored medium thick text. */
    public static @StyleRes int getTextMediumThickPrimary(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextMediumThick_Primary_Baseline_Light
                : R.style.TextAppearance_TextMediumThick_Primary;
    }

    /** Returns the correct text appearance style res for accent 1 colored medium thick text. */
    public static @StyleRes int getTextMediumThickAccent1(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextMediumThick_Blue_Baseline_Light
                : R.style.TextAppearance_TextMediumThick_Accent1;
    }

    /** Returns the correct text appearance style res for secondary colored medium thick text. */
    public static @StyleRes int getTextMediumThickSecondary(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextMediumThick_Secondary_Baseline_Light
                : R.style.TextAppearance_TextMediumThick_Secondary;
    }

    /** Returns the correct text appearance style res for secondary colored small text. */
    public static @StyleRes int getTextSmallSecondary(boolean isIncognito) {
        return isIncognito
                ? R.style.TextAppearance_TextSmall_Secondary_Baseline_Light
                : R.style.TextAppearance_TextSmall_Secondary;
    }
}
