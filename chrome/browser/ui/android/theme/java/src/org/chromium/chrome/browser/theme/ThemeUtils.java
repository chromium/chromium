// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.util.ColorUtils;

/** Utility methods for theme colors. */
public class ThemeUtils {
    /**
     * Alpha used when TextBox color is computed by brightening the Toolbar color using Color.WHITE.
     */
    @VisibleForTesting static final float LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA = 0.2f;

    /**
     * Alpha used when TextBox color is computed by darkening the Toolbar color using Color.BLACK.
     */
    @VisibleForTesting static final float LOCATION_BAR_TRANSPARENT_BACKGROUND_DARKEN_ALPHA = 0.1f;

    /**
     * The background color to use for a given {@link Tab}. This will either be the color specified
     * by the associated web content or a default color if not specified.
     *
     * @param tab {@link Tab} object to get the background color for.
     * @return The background color of {@link Tab}.
     */
    public static @ColorInt int getBackgroundColor(Tab tab) {
        if (tab.isNativePage()) return tab.getNativePage().getBackgroundColor();

        WebContents tabWebContents = tab.getWebContents();
        RenderWidgetHostView rwhv =
                tabWebContents == null ? null : tabWebContents.getRenderWidgetHostView();
        @ColorInt
        int backgroundColor = rwhv != null ? rwhv.getBackgroundColor() : Color.TRANSPARENT;
        if (backgroundColor != Color.TRANSPARENT) return backgroundColor;
        return ChromeColors.getPrimaryBackgroundColor(tab.getContext(), false);
    }

    /**
     * Determine the text box background color given the current tab.
     *
     * @param context {@link Context} used to retrieve colors.
     * @param tab The current {@link Tab}
     * @param backgroundColor The color of the toolbar background.
     * @return The base color for the textbox given a toolbar background color.
     */
    public static @ColorInt int getTextBoxColorForToolbarBackground(
            Context context, @Nullable Tab tab, @ColorInt int backgroundColor) {
        boolean isIncognito = tab != null && tab.isIncognito();
        boolean isCustomTab = tab != null && tab.isCustomTab();
        @ColorInt
        int defaultColor =
                getTextBoxColorForToolbarBackgroundInNonNativePage(
                        context, backgroundColor, isIncognito, isCustomTab);
        NativePage nativePage = tab != null ? tab.getNativePage() : null;
        return nativePage != null
                ? nativePage.getToolbarTextBoxBackgroundColor(defaultColor)
                : defaultColor;
    }

    /**
     * Determine the text box background color given a toolbar background color
     *
     * @param context {@link Context} used to retrieve colors.
     * @param color The color of the toolbar background.
     * @param isIncognito Whether or not the color is used for incognito mode.
     * @param isCustomTab Whether TextBox color is requested for Custom Tab.
     * @return The base color for the textbox given a toolbar background color.
     */
    public static @ColorInt int getTextBoxColorForToolbarBackgroundInNonNativePage(
            Context context, @ColorInt int color, boolean isIncognito, boolean isCustomTab) {
        // Text box color on default toolbar background in incognito mode is a pre-defined color.
        if (isIncognito) {
            return context.getColor(R.color.toolbar_text_box_background_incognito);
        }

        // Text box color on default toolbar background in standard mode is a pre-defined
        // color instead of a calculated color.
        if (ThemeUtils.isUsingDefaultToolbarColor(context, false, color)) {
            float tabElevation = context.getResources().getDimension(R.dimen.default_elevation_4);
            return ChromeColors.getSurfaceColor(context, tabElevation);
        }

        if (ColorUtils.shouldUseOpaqueTextboxBackground(color)) {
            if (isCustomTab) {
                return ColorUtils.getColorWithOverlay(
                        color, Color.BLACK, LOCATION_BAR_TRANSPARENT_BACKGROUND_DARKEN_ALPHA);
            }
            // TODO(mdjones): Clean up shouldUseOpaqueTextboxBackground logic.
            return Color.WHITE;
        } else {
            return ColorUtils.getColorWithOverlay(
                    color, Color.WHITE, LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA);
        }
    }

    /**
     * Returns the icon tint for based on the given parameters. Does not adjust color based on night
     * mode as this may conflict with toolbar theme colors.
     *
     * @param context The {@link Context} used to retrieve colors.
     * @param useLight Whether or not the icon tint should be light.
     * @return The {@link ColorStateList} for the icon tint of themed toolbar.
     */
    public static ColorStateList getThemedToolbarIconTint(Context context, boolean useLight) {
        return AppCompatResources.getColorStateList(context, getThemedToolbarIconTintRes(useLight));
    }

    /**
     * Returns the icon tint for based on the given parameters. Does not adjust color based on
     * night mode as this may conflict with toolbar theme colors.
     * @param useLight Whether or not the icon tint should be light.
     * @return The {@link ColorRes} for the icon tint of themed toolbar.
     */
    public static @ColorRes int getThemedToolbarIconTintRes(boolean useLight) {
        // Light toolbar theme colors may be used in night mode, so use toolbar_icon_tint_dark which
        // is not overridden in night- resources.
        return useLight
                ? R.color.default_icon_color_light_tint_list
                : R.color.default_icon_color_dark_tint_list;
    }

    /**
     * Returns the themed toolbar icon tint list.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return Primary icon tint list.
     */
    public static ColorStateList getThemedToolbarIconTint(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        // A focused activity uses the default (primary) icon tint.
        return getThemedToolbarIconTintForActivityState(
                context, brandedColorScheme, /* isActivityFocused= */ true);
    }

    /**
     * Returns the themed toolbar icon tint resource.
     *
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return Primary icon tint resource.
     */
    public static @ColorRes int getThemedToolbarIconTintRes(
            @BrandedColorScheme int brandedColorScheme) {
        // A focused activity uses the default (primary) icon tint.
        return getThemedToolbarIconTintResForActivityState(
                brandedColorScheme, /* isActivityFocused= */ true);
    }

    /**
     * Returns the themed toolbar icon tint list, taking the activity focus state into account. The
     * activity focus state is relevant only when the desktop windowing mode is active, where a
     * different tint is used for an unfocused activity.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @param isActivityFocused Whether the activity containing the toolbar is focused, {@code true}
     *     if focused, {@code false} otherwise.
     * @return Icon tint list.
     */
    public static ColorStateList getThemedToolbarIconTintForActivityState(
            Context context,
            @BrandedColorScheme int brandedColorScheme,
            boolean isActivityFocused) {
        return AppCompatResources.getColorStateList(
                context,
                getThemedToolbarIconTintResForActivityState(brandedColorScheme, isActivityFocused));
    }

    /**
     * Returns the themed toolbar icon tint resource, taking the activity focus state into account.
     * The activity focus state is relevant only when the desktop windowing mode is active, where a
     * different tint is used for an unfocused activity.
     *
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @param isActivityFocused Whether the activity containing the toolbar is focused, {@code true}
     *     if focused, {@code false} otherwise.
     * @return Icon tint resource.
     */
    public static @ColorRes int getThemedToolbarIconTintResForActivityState(
            @BrandedColorScheme int brandedColorScheme, boolean isActivityFocused) {
        @ColorRes
        int colorId =
                isActivityFocused
                        ? R.color.default_icon_color_tint_list
                        : R.color.toolbar_icon_unfocused_activity_tint_list;
        if (brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            colorId =
                    isActivityFocused
                            ? R.color.default_icon_color_light_tint_list
                            : R.color.toolbar_icon_unfocused_activity_incognito_color;
        } else if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            colorId = R.color.default_icon_color_dark_tint_list;
        } else if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME) {
            colorId = R.color.default_icon_color_white_tint_list;
        }
        return colorId;
    }

    /**
     * Test if the toolbar is using the default color.
     *
     * @param context The context to get the toolbar surface color.
     * @param isIncognito Whether to retrieve the default theme color for incognito mode.
     * @param color The color that the toolbar is using.
     * @return If the color is the default toolbar color.
     */
    public static boolean isUsingDefaultToolbarColor(
            Context context, boolean isIncognito, @ColorInt int color) {
        return color == ChromeColors.getDefaultThemeColor(context, isIncognito);
    }

    /**
     * Returns the opaque toolbar hairline color based on the given parameters.
     * @param context The {@link Context} to access the theme and resources.
     * @param toolbarColor The toolbar color to base the calculation on.
     * @param isIncognito Whether the color is for incognito mode.
     * @return The color that will be used to tint the hairline.
     */
    public static @ColorInt int getToolbarHairlineColor(
            Context context, @ColorInt int toolbarColor, boolean isIncognito) {
        // Hairline is not shown when the toolbar is in an expansion animation, which should be the
        // primary time when there's transparency in the toolbar color. Our color here doesn't
        // really matter, but we need to guard against calling #overlayColor as it does not accept
        // transparent colors. Similarly, the check in #isUsingDefaultToolbarColor does not work
        // when there's any transparency.
        if (Color.alpha(toolbarColor) < 255) {
            return Color.TRANSPARENT;
        }

        if (isUsingDefaultToolbarColor(context, isIncognito, toolbarColor)) {
            return isIncognito
                    ? ContextCompat.getColor(context, R.color.divider_line_bg_color_light)
                    : SemanticColorUtils.getDividerLineBgColor(context);
        }

        @ColorInt
        int hairlineColor = ContextCompat.getColor(context, R.color.toolbar_hairline_overlay);
        return ColorUtils.overlayColor(toolbarColor, hairlineColor);
    }
}
