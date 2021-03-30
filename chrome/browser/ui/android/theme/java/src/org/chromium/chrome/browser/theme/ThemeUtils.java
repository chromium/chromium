// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.util.ColorUtils;

/**
 * Utility methods for theme colors.
 */
public class ThemeUtils {
    private static final float LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA = 0.2f;

    /**
     * The background color to use for a given {@link Tab}. This will either be the color specified
     * by the associated web content or a default color if not specified.
     * @param tab {@link Tab} object to get the background color for.
     * @return The background color of {@link Tab}.
     */
    public static int getBackgroundColor(Tab tab) {
        if (tab.isNativePage()) return tab.getNativePage().getBackgroundColor();

        WebContents tabWebContents = tab.getWebContents();
        RenderWidgetHostView rwhv =
                tabWebContents == null ? null : tabWebContents.getRenderWidgetHostView();
        final int backgroundColor = rwhv != null ? rwhv.getBackgroundColor() : Color.TRANSPARENT;
        if (backgroundColor != Color.TRANSPARENT) return backgroundColor;
        return ChromeColors.getPrimaryBackgroundColor(tab.getContext().getResources(), false);
    }

    /**
     * Determine the text box background color given the current tab.
     * @param res {@link Resources} used to retrieve colors.
     * @param tab The current {@link Tab}
     * @param backgroundColor The color of the toolbar background.
     * @return The base color for the textbox given a toolbar background color.
     */
    public static @ColorInt int getTextBoxColorForToolbarBackground(
            Resources res, @Nullable Tab tab, @ColorInt int backgroundColor) {
        boolean isIncognito = tab != null && tab.isIncognito();
        @ColorInt
        int defaultColor = getTextBoxColorForToolbarBackgroundInNonNativePage(
                res, backgroundColor, isIncognito);
        NativePage nativePage = tab != null ? tab.getNativePage() : null;
        return nativePage != null ? nativePage.getToolbarTextBoxBackgroundColor(defaultColor)
                                  : defaultColor;
    }

    /**
     * Determine the text box background color given a toolbar background color
     * @param res {@link Resources} used to retrieve colors.
     * @param color The color of the toolbar background.
     * @param isIncognito Whether or not the color is used for incognito mode.
     * @return The base color for the textbox given a toolbar background color.
     */
    public static @ColorInt int getTextBoxColorForToolbarBackgroundInNonNativePage(
            Resources res, @ColorInt int color, boolean isIncognito) {
        // Text box color on default toolbar background in incognito mode is a pre-defined
        // color. We calculate the equivalent opaque color from the pre-defined translucent color.
        if (isIncognito) {
            final int overlayColor = ApiCompatibilityUtils.getColor(
                    res, R.color.toolbar_text_box_background_incognito);
            final float overlayColorAlpha = Color.alpha(overlayColor) / 255f;
            final int overlayColorOpaque = overlayColor & 0xFF000000;
            return ColorUtils.getColorWithOverlay(color, overlayColorOpaque, overlayColorAlpha);
        }

        // Text box color on default toolbar background in standard mode is a pre-defined
        // color instead of a calculated color.
        if (ThemeUtils.isUsingDefaultToolbarColor(res, false, color)) {
            return ApiCompatibilityUtils.getColor(res, R.color.toolbar_text_box_background);
        }

        // TODO(mdjones): Clean up shouldUseOpaqueTextboxBackground logic.
        if (ColorUtils.shouldUseOpaqueTextboxBackground(color)) return Color.WHITE;

        return ColorUtils.getColorWithOverlay(
                color, Color.WHITE, LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA);
    }

    /**
     * Returns the icon tint for based on the given parameters. Does not adjust color based on
     * night mode as this may conflict with toolbar theme colors.
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
        return useLight ? R.color.default_icon_color_light_tint_list
                        : R.color.toolbar_icon_tint_dark;
    }

    /**
     * Test if the toolbar is using the default color.
     * @param resources The resources to get the toolbar primary color.
     * @param isIncognito Whether to retrieve the default theme color for incognito mode.
     * @param color The color that the toolbar is using.
     * @return If the color is the default toolbar color.
     */
    public static boolean isUsingDefaultToolbarColor(
            Resources resources, boolean isIncognito, int color) {
        return color == ChromeColors.getDefaultThemeColor(resources, isIncognito);
    }
}
