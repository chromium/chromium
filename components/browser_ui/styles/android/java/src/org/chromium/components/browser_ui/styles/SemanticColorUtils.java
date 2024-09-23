// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import android.content.Context;

import androidx.annotation.AttrRes;
import androidx.annotation.ColorInt;
import androidx.annotation.DimenRes;

import com.google.android.material.color.MaterialColors;

/**
 * Provides semantic color values, typically in place of <macro>s which currently cannot be used in
 * Java code, or for surface colors that must be calculated to Java code.
 */
public class SemanticColorUtils {
    private static final String TAG = "SemanticColorUtils";

    private static @ColorInt int resolve(@AttrRes int attrRes, Context context) {
        return MaterialColors.getColor(context, attrRes, TAG);
    }

    private static @ColorInt int resolveSurfaceColorElev(
            @DimenRes int elevationDimen, Context context) {
        return ChromeColors.getSurfaceColor(context, elevationDimen);
    }

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
        return resolve(R.attr.colorPrimary, context);
    }

    /** Returns the semantic color value that corresponds to default_icon_color_on_accent1. */
    public static @ColorInt int getDefaultIconColorOnAccent1(Context context) {
        return resolve(R.attr.colorOnPrimary, context);
    }

    /**
     * Returns the semantic color value that corresponds to default_icon_color_on_accent1_container.
     */
    public static @ColorInt int getDefaultIconColorOnAccent1Container(Context context) {
        return resolve(R.attr.colorOnPrimaryContainer, context);
    }

    /** Returns the semantic color value that corresponds to default_text_color_on_accent1. */
    public static @ColorInt int getDefaultTextColorOnAccent1(Context context) {
        return resolve(R.attr.colorOnPrimary, context);
    }

    /** Returns the semantic color value that corresponds to default_text_color_secondary. */
    public static @ColorInt int getDefaultTextColorSecondary(Context context) {
        return resolve(R.attr.colorOnSurfaceVariant, context);
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
        return resolve(R.attr.colorPrimary, context);
    }

    /** Returns the semantic color value that corresponds to default_icon_color_secondary. */
    public static @ColorInt int getDefaultIconColorSecondary(Context context) {
        return resolve(R.attr.colorOnSurfaceVariant, context);
    }

    /** Returns the semantic color value that corresponds to divider_line_bg_color. */
    public static @ColorInt int getDividerLineBgColor(Context context) {
        return resolve(R.attr.colorSurfaceVariant, context);
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
        return resolve(R.attr.colorPrimary, context);
    }

    /** Returns the semantic color value that corresponds to progress_bar_foreground. */
    public static @ColorInt int getProgressBarForeground(Context context) {
        return getDefaultControlColorActive(context);
    }

    /** Returns the surface color value of the conceptual toolbar_background_primary. */
    public static @ColorInt int getToolbarBackgroundPrimary(Context context) {
        return getDefaultBgColor(context);
    }

    /** Returns the semantic color value that corresponds to default_bg_color_elev_1. */
    public static @ColorInt int getDefaultBgColorElev1(Context context) {
        return resolveSurfaceColorElev(R.dimen.default_elevation_1, context);
    }

    /** Returns the semantic color value that corresponds to default_bg_color_elev_2. */
    public static @ColorInt int getDefaultBgColorElev2(Context context) {
        return resolveSurfaceColorElev(R.dimen.default_elevation_2, context);
    }

    /** Returns the semantic color value that corresponds to navigation_bubble_background_color. */
    public static @ColorInt int getNavigationBubbleBackgroundColor(Context context) {
        return getDefaultBgColorElev2(context);
    }

    /** Returns the semantic color value that corresponds to drag_handlebar_color. */
    public static @ColorInt int getDragHandlebarColor(Context context) {
        return getDividerLineBgColor(context);
    }

    /** Returns the surface color value of the conceptual dialog_bg_color. */
    public static @ColorInt int getDialogBgColor(Context context) {
        return resolveSurfaceColorElev(R.dimen.dialog_bg_color_elev, context);
    }

    /** Returns the surface color value of the conceptual sheet_bg_color. */
    public static @ColorInt int getSheetBgColor(Context context) {
        return resolveSurfaceColorElev(R.dimen.sheet_bg_color_elev, context);
    }

    /** Returns the surface color value of the conceptual snackbar_background_color_baseline. */
    public static @ColorInt int getSnackbarBackgroundColor(Context context) {
        return resolveSurfaceColorElev(R.dimen.snackbar_background_color_elev, context);
    }

    /** Returns the semantic color value that corresponds to default_text_color_link. */
    public static @ColorInt int getDefaultTextColorLink(Context context) {
        final @ColorInt int fallback = context.getColor(R.color.default_text_color_link_baseline);
        return MaterialColors.getColor(context, R.attr.globalLinkTextColor, fallback);
    }

    /** Returns the semantic color values that corresponds to colorPrimaryContainer. */
    public static @ColorInt int getColorPrimaryContainer(Context context) {
        return resolve(R.attr.colorPrimaryContainer, context);
    }

    /** Returns the semantic color values that corresponds to colorOnSurfaceInverse. */
    public static @ColorInt int getColorOnSurfaceInverse(Context context) {
        return resolve(R.attr.colorOnSurfaceInverse, context);
    }

    /** Returns the semantic color values that corresponds to chip_bg_selected_color. */
    public static @ColorInt int getChipBgSelectedColor(Context context) {
        return resolve(R.attr.colorSecondaryContainer, context);
    }

    public static @ColorInt int getColorOnSecondaryContainer(Context context) {
        return resolve(R.attr.colorOnSecondaryContainer, context);
    }
}
