// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

/** Utility class that provides color values based on feature flags enabled. */
@NullMarked
public class SurfaceColorUpdateUtils {
    /**
     * Returns the background color for the Omnibox based on the enabled flag.
     *
     * @param context {@link Context} used to retrieve colors.
     * @return The background color.
     */
    public static @ColorInt int getOmniboxBackgroundColor(Context context, boolean isIncognito) {
        if (isIncognito) {
            return ContextCompat.getColor(context, R.color.toolbar_text_box_background_incognito);
        }
        return ChromeFeatureList.sAndroidSurfaceColorUpdate.isEnabled()
                ? SemanticColorUtils.getColorSurface(context)
                : ContextCompat.getColor(context, R.color.toolbar_text_box_bg_color);
    }

    /**
     * Returns the background color for the toolbar based on the enabled flag and other parameters.
     *
     * @param context {@link Context} used to retrieve colors.
     * @return The background color.
     */
    public static @ColorInt int getDefaultThemeColor(Context context, boolean isIncognito) {
        if (ChromeFeatureList.sAndroidSurfaceColorUpdate.isEnabled() && !isIncognito) {
            return SemanticColorUtils.getColorSurfaceContainerHigh(context);
        }
        return ChromeColors.getDefaultThemeColor(context, isIncognito);
    }

    /**
     * Determine the background color for tab strip based on surface color update flags.
     *
     * @see TabUiThemeUtil#getTabStripBackgroundColor
     * @param context {@link Context} used to retrieve colors.
     * @return The background color.
     */
    public static @ColorInt int getTabStripBackgroundColorDefault(Context context) {
        if (ChromeFeatureList.sAndroidSurfaceColorUpdate.isEnabled()) {
            return SemanticColorUtils.getColorSurfaceDim(context);
        }
        @ColorInt int darkThemeColor = SemanticColorUtils.getColorSurfaceContainer(context);
        @ColorInt int lightThemeColor = SemanticColorUtils.getColorSurfaceContainerHigh(context);
        return ColorUtils.inNightMode(context) ? darkThemeColor : lightThemeColor;
    }

    /**
     * Determine the background color for tab strip when unfocused based on surface color update
     * flags.
     *
     * @see TabUiThemeUtil#getTabStripBackgroundColor
     * @param context {@link Context} used to retrieve colors.
     * @return The background color.
     */
    public static @ColorInt int getTabStripBackgroundColorUnfocused(Context context) {
        if (ChromeFeatureList.sAndroidSurfaceColorUpdate.isEnabled()) {
            @ColorInt int baseColor = SemanticColorUtils.getColorSurfaceDim(context);
            @ColorInt int overlayColor = SemanticColorUtils.getColorOnSurfaceInverse(context);
            float fraction =
                    context.getResources()
                            .getFraction(R.fraction.tab_strip_background_unfocused_fraction, 1, 1);
            return ColorUtils.overlayColor(baseColor, overlayColor, fraction);
        }

        @ColorInt int darkThemeColor = SemanticColorUtils.getColorSurfaceContainerLow(context);
        @ColorInt int lightThemeColor = SemanticColorUtils.getColorSurfaceContainer(context);
        return ColorUtils.inNightMode(context) ? darkThemeColor : lightThemeColor;
    }

    /**
     * Returns the background color for the grid tab switcher based on the enabled flag and
     * incognito.
     *
     * @param context {@link Context} used to retrieve colors.
     * @return The background color.
     */
    public static @ColorInt int getGridTabSwitcherBackgroundColor(
            Context context, boolean isIncognito) {
        // TODO(crbug.com/414404094): Add semantic color for incognito.
        if (ChromeFeatureList.sGridTabSwitcherSurfaceColorUpdate.isEnabled()) {
            return isIncognito
                    ? ContextCompat.getColor(
                            context, R.color.gm3_baseline_surface_container_high_dark)
                    : SemanticColorUtils.getColorSurfaceContainerHigh(context);
        }
        return isIncognito
                ? ContextCompat.getColor(context, R.color.default_bg_color_dark)
                : SemanticColorUtils.getDefaultBgColor(context);
    }

    /**
     * Returns the background color for the card view in grid tab switcher on the enabled flag and
     * incognito.
     *
     * @param context {@link Context} used to retrieve colors.
     * @return The background color.
     */
    public static @ColorInt int getCardViewBackgroundColor(Context context, boolean isIncognito) {
        if (ChromeFeatureList.sGridTabSwitcherSurfaceColorUpdate.isEnabled()) {
            // TODO(crbug.com/414404094): Add semantic color for incognito tab card view.
            return isIncognito
                    ? ContextCompat.getColor(
                            context, R.color.gm3_baseline_surface_container_high_dark)
                    : SemanticColorUtils.getColorSurfaceDim(context);
        }
        // Checking night mode to keep color behaviour the same as in tab_card_view_bg_color.
        @ColorInt
        int defaultBackground =
                ColorUtils.inNightMode(context)
                        ? SemanticColorUtils.getColorSurfaceContainerHighest(context)
                        : SemanticColorUtils.getColorSurfaceContainerHigh(context);
        return isIncognito
                ? ContextCompat.getColor(
                        context, R.color.gm3_baseline_surface_container_highest_dark)
                : defaultBackground;
    }
}
