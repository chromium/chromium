// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.TypedValue;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

/** Provides resources specific to Omnibox. */
public class OmniboxResourceProvider {
    private static final String TAG = "OmniboxResourceProvider";

    /**
     * Returns a drawable for a given attribute depending on a {@link BrandedColorScheme}
     *
     * @param context The {@link Context} used to retrieve resources.
     * @param brandedColorScheme {@link BrandedColorScheme} to use.
     * @param attributeResId A resource ID of an attribute to resolve.
     * @return A background drawable resource ID providing ripple effect.
     */
    public static Drawable resolveAttributeToDrawable(
            Context context, @BrandedColorScheme int brandedColorScheme, int attributeResId) {
        Context wrappedContext = maybeWrapContext(context, brandedColorScheme);
        @DrawableRes
        int resourceId = resolveAttributeToDrawableRes(wrappedContext, attributeResId);
        return ContextCompat.getDrawable(wrappedContext, resourceId);
    }

    /**
     * Returns the ColorScheme based on the incognito state and the background color.
     *
     * @param context The {@link Context}.
     * @param isIncognito Whether incognito mode is enabled.
     * @param primaryBackgroundColor The primary background color of the omnibox.
     * @return The {@link BrandedColorScheme}.
     */
    public static @BrandedColorScheme int getBrandedColorScheme(
            Context context, boolean isIncognito, @ColorInt int primaryBackgroundColor) {
        if (isIncognito) return BrandedColorScheme.INCOGNITO;

        if (ThemeUtils.isUsingDefaultToolbarColor(context, isIncognito, primaryBackgroundColor)) {
            return BrandedColorScheme.APP_DEFAULT;
        }

        return ColorUtils.shouldUseLightForegroundOnBackground(primaryBackgroundColor)
                ? BrandedColorScheme.DARK_BRANDED_THEME
                : BrandedColorScheme.LIGHT_BRANDED_THEME;
    }

    /**
     * Returns the primary text color for the url bar.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return Primary url bar text color.
     */
    public static @ColorInt int getUrlBarPrimaryTextColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        final Resources resources = context.getResources();
        @ColorInt
        int color;
        if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            color = resources.getColor(R.color.branded_url_text_on_light_bg);
        } else if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME) {
            color = resources.getColor(R.color.branded_url_text_on_dark_bg);
        } else if (brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            color = resources.getColor(R.color.url_bar_primary_text_incognito);
        } else {
            color = MaterialColors.getColor(context, R.attr.colorOnSurface, TAG);
        }
        return color;
    }

    /**
     * Returns the secondary text color for the url bar.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return Secondary url bar text color.
     */
    public static @ColorInt int getUrlBarSecondaryTextColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        final Resources resources = context.getResources();
        @ColorInt
        int color;
        if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            color = resources.getColor(R.color.branded_url_text_variant_on_light_bg);
        } else if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME) {
            color = resources.getColor(R.color.branded_url_text_variant_on_dark_bg);
        } else if (brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            color = resources.getColor(R.color.url_bar_secondary_text_incognito);
        } else {
            color = MaterialColors.getColor(context, R.attr.colorOnSurfaceVariant, TAG);
        }
        return color;
    }

    /**
     * Returns the hint text color for the url bar.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return The url bar hint text color.
     */
    public static @ColorInt int getUrlBarHintTextColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        return getUrlBarSecondaryTextColor(context, brandedColorScheme);
    }

    /**
     * Returns the danger semantic color.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return The danger semantic color to be used on the url bar.
     */
    public static @ColorInt int getUrlBarDangerColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        // Danger color has semantic meaning and it doesn't change with dynamic colors.
        @ColorRes
        int colorId = R.color.default_red;
        if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME
                || brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            colorId = R.color.default_red_light;
        } else if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            colorId = R.color.default_red_dark;
        }
        return context.getResources().getColor(colorId);
    }

    /**
     * Returns the secure semantic color.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return The secure semantic color to be used on the url bar.
     */
    public static @ColorInt int getUrlBarSecureColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        // Secure color has semantic meaning and it doesn't change with dynamic colors.
        @ColorRes
        int colorId = R.color.default_green;
        if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME
                || brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            colorId = R.color.default_green_light;
        } else if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            colorId = R.color.default_green_dark;
        }
        return context.getResources().getColor(colorId);
    }

    /**
     * Returns the primary text color for the suggestions.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return Primary suggestion text color.
     */
    public static @ColorInt int getSuggestionPrimaryTextColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        // Suggestions are only shown when the omnibox is focused, hence LIGHT_THEME and DARK_THEME
        // are ignored as they don't change the result.
        return brandedColorScheme == BrandedColorScheme.INCOGNITO
                ? context.getColor(R.color.default_text_color_light)
                : MaterialColors.getColor(context, R.attr.colorOnSurface, TAG);
    }

    /**
     * Returns the secondary text color for the suggestions.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return Secondary suggestion text color.
     */
    public static @ColorInt int getSuggestionSecondaryTextColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        // Suggestions are only shown when the omnibox is focused, hence LIGHT_THEME and DARK_THEME
        // are ignored as they don't change the result.
        return brandedColorScheme == BrandedColorScheme.INCOGNITO
                ? context.getColor(R.color.default_text_color_secondary_light)
                : MaterialColors.getColor(context, R.attr.colorOnSurfaceVariant, TAG);
    }

    /**
     * Returns the URL text color for the suggestions.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return URL suggestion text color.
     */
    public static @ColorInt int getSuggestionUrlTextColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        // Suggestions are only shown when the omnibox is focused, hence LIGHT_THEME and DARK_THEME
        // are ignored as they don't change the result.
        final @ColorInt int color = brandedColorScheme == BrandedColorScheme.INCOGNITO
                ? context.getColor(R.color.suggestion_url_color_incognito)
                : SemanticColorUtils.getDefaultTextColorLink(context);
        return color;
    }

    /**
     * Returns the separator line color for the status view.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return Status view separator color.
     */
    public static @ColorInt int getStatusSeparatorColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            return context.getColor(R.color.locationbar_status_separator_color_dark);
        }
        if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME) {
            return context.getColor(R.color.locationbar_status_separator_color_light);
        }
        if (brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            return context.getColor(R.color.locationbar_status_separator_color_incognito);
        }
        return MaterialColors.getColor(context, R.attr.colorOutline, TAG);
    }

    /**
     * Returns the preview text color for the status view.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return Status view preview text color.
     */
    public static @ColorInt int getStatusPreviewTextColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            return context.getColor(R.color.locationbar_status_preview_color_dark);
        }
        if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME) {
            return context.getColor(R.color.locationbar_status_preview_color_light);
        }
        if (brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            return context.getColor(R.color.locationbar_status_preview_color_incognito);
        }
        return MaterialColors.getColor(context, R.attr.colorPrimary, TAG);
    }

    /**
     * Returns the offline text color for the status view.
     *
     * @param context The context to retrieve the resources from.
     * @param brandedColorScheme The {@link BrandedColorScheme}.
     * @return Status view offline text color.
     */
    public static @ColorInt int getStatusOfflineTextColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            return context.getColor(R.color.locationbar_status_offline_color_dark);
        }
        if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME) {
            return context.getColor(R.color.locationbar_status_offline_color_light);
        }
        if (brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            return context.getColor(R.color.locationbar_status_offline_color_incognito);
        }
        return context.getColor(R.color.default_text_color_secondary_list);
    }

    /**
     * Wraps the context if necessary to force dark resources for incognito.
     *
     * @param context The {@link Context} to be wrapped.
     * @param brandedColorScheme Current color scheme.
     * @return Context with resources appropriate to the {@link BrandedColorScheme}.
     */
    private static Context maybeWrapContext(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        // Only wraps the context in case of incognito.
        if (brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            return NightModeUtils.wrapContextWithNightModeConfig(
                    context, R.style.Theme_Chromium_TabbedMode, /*nightMode=*/true);
        }

        return context;
    }

    /**
     * Resolves the attribute based on the current theme.
     *
     * @param context The {@link Context} used to retrieve resources.
     * @param attributeResId Resource ID of the attribute to resolve.
     * @return Resource ID of the expected drawable.
     */
    private static @DrawableRes int resolveAttributeToDrawableRes(
            Context context, int attributeResId) {
        TypedValue themeRes = new TypedValue();
        context.getTheme().resolveAttribute(attributeResId, themeRes, true);
        return themeRes.resourceId;
    }
}
