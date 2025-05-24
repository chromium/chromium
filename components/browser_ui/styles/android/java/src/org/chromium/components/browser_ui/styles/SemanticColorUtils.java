// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import android.content.Context;

import androidx.annotation.AttrRes;
import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.NullMarked;

/**
 * Provides semantic color values, typically in place of <macro>s which currently cannot be used in
 * Java code, or for surface colors that must be calculated to Java code.
 */
@NullMarked
public class SemanticColorUtils {
    private static final String TAG = "SemanticColorUtils";

    private static @ColorInt int resolve(@AttrRes int attrRes, Context context) {
        return MaterialColors.getColor(context, attrRes, TAG);
    }

    // LINT.IfChange(SemanticColorUtils)

    /** Returns the semantic color value that corresponds to default_bg_color. */
    public static @ColorInt int getDefaultBgColor(Context context) {
        return resolve(R.attr.colorSurface, context);
    }

    /** Returns the semantic color value that corresponds to default_text_color. */
    public static @ColorInt int getDefaultTextColor(Context context) {
        return resolve(R.attr.colorOnSurface, context);
    }

    /** Returns the semantic color value that corresponds to default_text_color_accent1. */
    public static @ColorInt int getDefaultTextColorAccent1(Context context) {
        return getColorPrimary(context);
    }

    /** Returns the semantic color value that corresponds to default_icon_color_on_accent1. */
    public static @ColorInt int getDefaultIconColorOnAccent1(Context context) {
        return getColorOnPrimary(context);
    }

    /**
     * Returns the semantic color value that corresponds to default_icon_color_on_accent1_container.
     */
    public static @ColorInt int getDefaultIconColorOnAccent1Container(Context context) {
        return resolve(R.attr.colorOnPrimaryContainer, context);
    }

    /** Returns the semantic color value that corresponds to default_text_color_on_accent1. */
    public static @ColorInt int getDefaultTextColorOnAccent1(Context context) {
        return getColorOnPrimary(context);
    }

    /** Returns the semantic color value that corresponds to default_text_color_secondary. */
    public static @ColorInt int getDefaultTextColorSecondary(Context context) {
        return resolve(R.attr.colorOnSurfaceVariant, context);
    }

    /** Returns the semantic color value that corresponds to filled_button_bg_color. */
    public static @ColorInt int getFilledButtonBgColor(Context context) {
        return getColorPrimary(context);
    }

    /** Returns the semantic color value that corresponds to default_icon_color. */
    public static @ColorInt int getDefaultIconColor(Context context) {
        return resolve(R.attr.colorOnSurface, context);
    }

    /** Returns the semantic color value that corresponds to default_icon_color_inverse. */
    public static @ColorInt int getDefaultIconColorInverse(Context context) {
        return resolve(R.attr.colorOnSurfaceInverse, context);
    }

    /** Returns the semantic color value that corresponds to default_icon_color_accent1. */
    public static @ColorInt int getDefaultIconColorAccent1(Context context) {
        return getColorPrimary(context);
    }

    /** Returns the semantic color value that corresponds to default_icon_color_secondary. */
    public static @ColorInt int getDefaultIconColorSecondary(Context context) {
        return resolve(R.attr.colorOnSurfaceVariant, context);
    }

    /** Returns the semantic color value that corresponds to divider_line_bg_color. */
    public static @ColorInt int getDividerLineBgColor(Context context) {
        return resolve(R.attr.colorSurfaceContainerHighest, context);
    }

    /** Returns the semantic color value that corresponds to bottom_system_nav_color. */
    public static @ColorInt int getBottomSystemNavColor(Context context) {
        return getDefaultBgColor(context);
    }

    /** Returns the semantic color value that corresponds to bottom_system_nav_divider_color. */
    public static @ColorInt int getBottomSystemNavDividerColor(Context context) {
        return getDividerLineBgColor(context);
    }

    /** Returns the semantic color value that corresponds to overlay_panel_separator_line_color. */
    public static @ColorInt int getOverlayPanelSeparatorLineColor(Context context) {
        return getDividerLineBgColor(context);
    }

    /** Returns the semantic color value that corresponds to tab_grid_card_divider_tint_color. */
    public static @ColorInt int getTabGridCardDividerTintColor(Context context) {
        return getDividerLineBgColor(context);
    }

    /** Returns the semantic color value that corresponds to default_control_color_active. */
    public static @ColorInt int getDefaultControlColorActive(Context context) {
        return getColorPrimary(context);
    }

    /** Returns the semantic color value that corresponds to progress_bar_foreground. */
    public static @ColorInt int getProgressBarForeground(Context context) {
        return getDefaultControlColorActive(context);
    }

    /** Returns the semantic color value that corresponds to progress_bar_track_color. */
    public static @ColorInt int getProgressBarTrackColor(Context context) {
        return resolve(R.attr.colorSecondaryContainer, context);
    }

    /** Returns the surface color value of the conceptual toolbar_background_primary. */
    public static @ColorInt int getToolbarBackgroundPrimary(Context context) {
        return getDefaultBgColor(context);
    }

    /** Returns the semantic color value that corresponds to navigation_bubble_background_color. */
    public static @ColorInt int getNavigationBubbleBackgroundColor(Context context) {
        return getColorSurfaceContainer(context);
    }

    /** Returns the semantic color value that corresponds to drag_handlebar_color. */
    public static @ColorInt int getDragHandlebarColor(Context context) {
        return getDividerLineBgColor(context);
    }

    /** Returns the surface color value of the conceptual dialog_bg_color. */
    public static @ColorInt int getDialogBgColor(Context context) {
        return ContextCompat.getColor(context, R.color.dialog_bg_color);
    }

    /** Returns the surface color value of the conceptual sheet_bg_color. */
    public static @ColorInt int getSheetBgColor(Context context) {
        return resolve(R.attr.colorSurface, context);
    }

    /** Returns the surface color value of the conceptual snackbar_background_color. */
    public static @ColorInt int getSnackbarBackgroundColor(Context context) {
        return resolve(R.attr.colorSurface, context);
    }

    /** Returns the semantic color value that corresponds to default_text_color_link. */
    public static @ColorInt int getDefaultTextColorLink(Context context) {
        final @ColorInt int fallback = context.getColor(R.color.default_text_color_link_baseline);
        return MaterialColors.getColor(context, R.attr.globalLinkTextColor, fallback);
    }

    /** Returns the semantic color value that corresponds to menu_bg_color. */
    public static @ColorInt int getMenuBgColor(Context context) {
        return ContextCompat.getColor(context, R.color.menu_bg_color);
    }

    /** Returns the background color for a CardView matching default_card_bg_color */
    public static @ColorInt int getCardBackgroundColor(Context context) {
        return getColorSurfaceContainer(context);
    }

    // LINT.ThenChange(//components/browser_ui/styles/android/java/res/values/semantic_colors_dynamic.xml)

    /** Returns the semantic color values that corresponds to colorPrimaryContainer. */
    public static @ColorInt int getColorPrimaryContainer(Context context) {
        return resolve(R.attr.colorPrimaryContainer, context);
    }

    /** Returns the semantic color values that correspond to colorOnPrimary. */
    public static @ColorInt int getColorOnPrimary(Context context) {
        return resolve(R.attr.colorOnPrimary, context);
    }

    /** Returns the semantic color values that correspond to colorOnSurface. */
    public static @ColorInt int getColorOnSurface(Context context) {
        return resolve(R.attr.colorOnSurface, context);
    }

    /** Returns the semantic color values that correspond to colorOnSurfaceInverse. */
    public static @ColorInt int getColorOnSurfaceInverse(Context context) {
        return resolve(R.attr.colorOnSurfaceInverse, context);
    }

    /** Returns the semantic color values that corresponds to chip_bg_selected_color. */
    public static @ColorInt int getChipBgSelectedColor(Context context) {
        return resolve(R.attr.colorSecondaryContainer, context);
    }

    /** Returns the semantic color values that correspond to colorSecondaryContainer. */
    public static @ColorInt int getColorSecondaryContainer(Context context) {
        return resolve(R.attr.colorSecondaryContainer, context);
    }

    /** Returns the semantic color values that correspond to colorOnSecondaryContainer. */
    public static @ColorInt int getColorOnSecondaryContainer(Context context) {
        return resolve(R.attr.colorOnSecondaryContainer, context);
    }

    /** Returns the semantic color values that correspond to colorSurface. */
    public static @ColorInt int getColorSurface(Context context) {
        return resolve(R.attr.colorSurface, context);
    }

    /** Returns the semantic color values that correspond to colorSurfaceBright. */
    public static @ColorInt int getColorSurfaceBright(Context context) {
        return resolve(R.attr.colorSurfaceBright, context);
    }

    /** Returns the semantic color values that correspond to colorPrimary. */
    public static @ColorInt int getColorPrimary(Context context) {
        return resolve(R.attr.colorPrimary, context);
    }

    /** Returns the semantic color values that correspond to colorSurfaceContainerLow. */
    public static @ColorInt int getColorSurfaceContainerLow(Context context) {
        return resolve(R.attr.colorSurfaceContainerLow, context);
    }

    /** Returns the semantic color values that correspond to colorSurfaceContainer. */
    public static @ColorInt int getColorSurfaceContainer(Context context) {
        return resolve(R.attr.colorSurfaceContainer, context);
    }

    /** Returns the semantic color values that correspond to colorSurfaceContainerHigh. */
    public static @ColorInt int getColorSurfaceContainerHigh(Context context) {
        return resolve(R.attr.colorSurfaceContainerHigh, context);
    }

    /** Returns the semantic color values that correspond to colorSurfaceContainerHighest. */
    public static @ColorInt int getColorSurfaceContainerHighest(Context context) {
        return resolve(R.attr.colorSurfaceContainerHighest, context);
    }

    /** Returns the semantic color values that correspond to colorSurfaceDim. */
    public static @ColorInt int getColorSurfaceDim(Context context) {
        return resolve(R.attr.colorSurfaceDim, context);
    }

    /** Returns the surface color value of the conceptual floating snackbar background color. */
    public static @ColorInt int getFloatingSnackbarBackgroundColor(Context context) {
        return resolve(R.attr.colorSurfaceContainerHigh, context);
    }
}
