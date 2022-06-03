// Copyright 2020 The Chromium Authors. All rights reserved.
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

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.ui.util.ColorUtils;

/** Provides resources specific to Omnibox. */
public class OmniboxResourceProvider {
    private static final String TAG = "OmniboxResourceProvider";

    /** @return Whether the mode is dark (dark theme or incognito). */
    public static boolean isDarkMode(@OmniboxTheme int omniboxTheme) {
        return omniboxTheme == OmniboxTheme.DARK_THEME || omniboxTheme == OmniboxTheme.INCOGNITO;
    }

    /**
     * Returns a drawable for a given attribute depending on a {@link OmniboxTheme}
     *
     * @param context The {@link Context} used to retrieve resources.
     * @param omniboxTheme {@link OmniboxTheme} to use.
     * @param attributeResId A resource ID of an attribute to resolve.
     * @return A background drawable resource ID providing ripple effect.
     */
    public static Drawable resolveAttributeToDrawable(
            Context context, @OmniboxTheme int omniboxTheme, int attributeResId) {
        Context wrappedContext = maybeWrapContext(context, omniboxTheme);
        @DrawableRes
        int resourceId = resolveAttributeToDrawableRes(wrappedContext, attributeResId);
        return ContextCompat.getDrawable(wrappedContext, resourceId);
    }

    /**
     * Returns the OmniboxTheme based on the incognito state and the background color.
     *
     * @param context The {@link Context}.
     * @param isIncognito Whether incognito mode is enabled.
     * @param primaryBackgroundColor The primary background color of the omnibox.
     * @return The {@link OmniboxTheme}.
     */
    public static @OmniboxTheme int getOmniboxTheme(
            Context context, boolean isIncognito, @ColorInt int primaryBackgroundColor) {
        if (isIncognito) return OmniboxTheme.INCOGNITO;

        if (ThemeUtils.isUsingDefaultToolbarColor(context, isIncognito, primaryBackgroundColor)) {
            return OmniboxTheme.DEFAULT;
        }

        return ColorUtils.shouldUseLightForegroundOnBackground(primaryBackgroundColor)
                ? OmniboxTheme.DARK_THEME
                : OmniboxTheme.LIGHT_THEME;
    }

    /**
     * Returns the primary text color for the url bar.
     *
     * @param context The context to retrieve the resources from.
     * @param omniboxTheme The {@link OmniboxTheme}.
     * @return Primary url bar text color.
     */
    public static @ColorInt int getUrlBarPrimaryTextColor(
            Context context, @OmniboxTheme int omniboxTheme) {
        final Resources resources = context.getResources();
        @ColorInt
        int color;
        if (omniboxTheme == OmniboxTheme.LIGHT_THEME) {
            color = resources.getColor(R.color.branded_url_text_on_light_bg);
        } else if (omniboxTheme == OmniboxTheme.DARK_THEME) {
            color = resources.getColor(R.color.branded_url_text_on_dark_bg);
        } else if (omniboxTheme == OmniboxTheme.INCOGNITO) {
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
     * @param omniboxTheme The {@link OmniboxTheme}.
     * @return Secondary url bar text color.
     */
    public static @ColorInt int getUrlBarSecondaryTextColor(
            Context context, @OmniboxTheme int omniboxTheme) {
        final Resources resources = context.getResources();
        @ColorInt
        int color;
        if (omniboxTheme == OmniboxTheme.LIGHT_THEME) {
            color = resources.getColor(R.color.branded_url_text_variant_on_light_bg);
        } else if (omniboxTheme == OmniboxTheme.DARK_THEME) {
            color = resources.getColor(R.color.branded_url_text_variant_on_dark_bg);
        } else if (omniboxTheme == OmniboxTheme.INCOGNITO) {
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
     * @param omniboxTheme The {@link OmniboxTheme}.
     * @return The url bar hint text color.
     */
    public static @ColorInt int getUrlBarHintTextColor(
            Context context, @OmniboxTheme int omniboxTheme) {
        return getUrlBarSecondaryTextColor(context, omniboxTheme);
    }

    /**
     * Returns the danger semantic color.
     *
     * @param context The context to retrieve the resources from.
     * @param omniboxTheme The {@link OmniboxTheme}.
     * @return The danger semantic color to be used on the url bar.
     */
    public static @ColorInt int getUrlBarDangerColor(
            Context context, @OmniboxTheme int omniboxTheme) {
        // Danger color has semantic meaning and it doesn't change with dynamic colors.
        @ColorRes
        int colorId = R.color.default_red;
        if (omniboxTheme == OmniboxTheme.DARK_THEME || omniboxTheme == OmniboxTheme.INCOGNITO) {
            colorId = R.color.default_red_light;
        } else if (omniboxTheme == OmniboxTheme.LIGHT_THEME) {
            colorId = R.color.default_red_dark;
        }
        return context.getResources().getColor(colorId);
    }

    /**
     * Returns the secure semantic color.
     *
     * @param context The context to retrieve the resources from.
     * @param omniboxTheme The {@link OmniboxTheme}.
     * @return The secure semantic color to be used on the url bar.
     */
    public static @ColorInt int getUrlBarSecureColor(
            Context context, @OmniboxTheme int omniboxTheme) {
        // Secure color has semantic meaning and it doesn't change with dynamic colors.
        @ColorRes
        int colorId = R.color.default_green;
        if (omniboxTheme == OmniboxTheme.DARK_THEME || omniboxTheme == OmniboxTheme.INCOGNITO) {
            colorId = R.color.default_green_light;
        } else if (omniboxTheme == OmniboxTheme.LIGHT_THEME) {
            colorId = R.color.default_green_dark;
        }
        return context.getResources().getColor(colorId);
    }

    /**
     * Returns the primary text color for the suggestions.
     *
     * @param context The context to retrieve the resources from.
     * @param omniboxTheme The {@link OmniboxTheme}.
     * @return Primary suggestion text color.
     */
    public static @ColorInt int getSuggestionPrimaryTextColor(
            Context context, @OmniboxTheme int omniboxTheme) {
        // Suggestions are only shown when the omnibox is focused, hence LIGHT_THEME and DARK_THEME
        // are ignored as they don't change the result.
        return omniboxTheme == OmniboxTheme.INCOGNITO
                ? ApiCompatibilityUtils.getColor(
                        context.getResources(), R.color.default_text_color_light)
                : MaterialColors.getColor(context, R.attr.colorOnSurface, TAG);
    }

    /**
     * Returns the secondary text color for the suggestions.
     *
     * @param context The context to retrieve the resources from.
     * @param omniboxTheme The {@link OmniboxTheme}.
     * @return Secondary suggestion text color.
     */
    public static @ColorInt int getSuggestionSecondaryTextColor(
            Context context, @OmniboxTheme int omniboxTheme) {
        // Suggestions are only shown when the omnibox is focused, hence LIGHT_THEME and DARK_THEME
        // are ignored as they don't change the result.
        return omniboxTheme == OmniboxTheme.INCOGNITO
                ? ApiCompatibilityUtils.getColor(
                        context.getResources(), R.color.default_text_color_secondary_light)
                : MaterialColors.getColor(context, R.attr.colorOnSurfaceVariant, TAG);
    }

    /**
     * Returns the URL text color for the suggestions.
     *
     * @param context The context to retrieve the resources from.
     * @param omniboxTheme The {@link OmniboxTheme}.
     * @return URL suggestion text color.
     */
    public static @ColorInt int getSuggestionUrlTextColor(
            Context context, @OmniboxTheme int omniboxTheme) {
        // Suggestions are only shown when the omnibox is focused, hence LIGHT_THEME and DARK_THEME
        // are ignored as they don't change the result.
        final @ColorRes int colorId = omniboxTheme == OmniboxTheme.INCOGNITO
                ? R.color.suggestion_url_color_incognito
                : R.color.suggestion_url_color;
        return ApiCompatibilityUtils.getColor(context.getResources(), colorId);
    }

    /**
     * Wraps the context if necessary to force dark resources for incognito.
     *
     * @param context The {@link Context} to be wrapped.
     * @param omniboxTheme Current omnibox theme.
     * @return Context with resources appropriate to the {@link OmniboxTheme}.
     */
    private static Context maybeWrapContext(Context context, @OmniboxTheme int omniboxTheme) {
        // Only wraps the context in case of incognito.
        if (omniboxTheme == OmniboxTheme.INCOGNITO) {
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
