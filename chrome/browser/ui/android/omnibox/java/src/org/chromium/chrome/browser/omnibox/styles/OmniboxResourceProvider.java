// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Drawable.ConstantState;
import android.util.SparseArray;
import android.util.TypedValue;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

/** Provides resources specific to Omnibox. */
public class OmniboxResourceProvider {
    private static final String TAG = "OmniboxResourceProvider";

    private static SparseArray<ConstantState> sDrawableCache = new SparseArray<>();
    private static SparseArray<String> sStringCache = new SparseArray<>();

    /**
     * As {@link androidx.appcompat.content.res.AppCompatResources#getDrawable(Context, int)} but
     * potentially augmented with caching. If caching is enabled, there is a single, unbounded cache
     * of ConstantState shared by all contexts.
     */
    public static @NonNull Drawable getDrawable(Context context, @DrawableRes int res) {
        ThreadUtils.assertOnUiThread();
        boolean cacheResources = OmniboxFeatures.shouldCacheSuggestionResources();
        ConstantState constantState = cacheResources ? sDrawableCache.get(res, null) : null;
        if (constantState != null) {
            return constantState.newDrawable(context.getResources());
        }

        Drawable drawable = AppCompatResources.getDrawable(context, res);
        if (cacheResources) {
            sDrawableCache.put(res, drawable.getConstantState());
        }
        return drawable;
    }

    /**
     * As {@link android.content.res.Resources#getString(int, Object...)} but potentially augmented
     * with caching. If caching is enabled, there is a single, unbounded string cache shared by all
     * contexts. When dealing with strings with format params, the raw string is cached and
     * formatted on demand using the default locale.
     */
    public static @NonNull String getString(Context context, @StringRes int res, Object... args) {
        ThreadUtils.assertOnUiThread();
        boolean cacheResources = OmniboxFeatures.shouldCacheSuggestionResources();
        String string = cacheResources ? sStringCache.get(res, null) : null;
        if (string == null) {
            string = context.getResources().getString(res);
            if (cacheResources) {
                sStringCache.put(res, string);
            }
        }

        return args.length == 0
                ? string
                : String.format(context.getResources().getConfiguration().getLocales().get(0),
                        string, args);
    }

    /**
     * Clears the drawable cache to avoid, e.g. caching a now incorrectly colored drawable
     * resource.
     */
    public static void invalidateDrawableCache() {
        sDrawableCache.clear();
    }

    @VisibleForTesting
    public static SparseArray<ConstantState> getDrawableCacheForTesting() {
        return sDrawableCache;
    }

    @VisibleForTesting
    public static SparseArray<String> getStringCacheForTesting() {
        return sStringCache;
    }

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
        return getDrawable(wrappedContext, resourceId);
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
        final @ColorInt int color;
        if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            color = context.getColor(R.color.branded_url_text_on_light_bg);
        } else if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME) {
            color = context.getColor(R.color.branded_url_text_on_dark_bg);
        } else if (brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            color = context.getColor(R.color.url_bar_primary_text_incognito);
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
        final @ColorInt int color;
        if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            color = context.getColor(R.color.branded_url_text_variant_on_light_bg);
        } else if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME) {
            color = context.getColor(R.color.branded_url_text_variant_on_dark_bg);
        } else if (brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            color = context.getColor(R.color.url_bar_secondary_text_incognito);
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
        final @ColorRes int colorId;
        if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME
                || brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            colorId = R.color.default_red_light;
        } else if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            colorId = R.color.default_red_dark;
        } else {
            colorId = R.color.default_red;
        }
        return context.getColor(colorId);
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
        final @ColorRes int colorId;
        if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME
                || brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            colorId = R.color.default_green_light;
        } else if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            colorId = R.color.default_green_dark;
        } else {
            colorId = R.color.default_green;
        }
        return context.getColor(colorId);
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
     * Returns the background color for suggestions in a "standard" (non-incognito) TabModel with
     * the given context.
     */
    public static @ColorInt int getStandardSuggestionBackgroundColor(Context context) {
        return OmniboxFeatures.shouldShowModernizeVisualUpdate(context)
                ? ChromeColors.getSurfaceColor(
                        context, R.dimen.omnibox_suggestion_bg_elevation_modern)
                : ChromeColors.getSurfaceColor(context, R.dimen.omnibox_suggestion_bg_elevation);
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

    /** Gets the margin, in pixels, on either side of an omnibox suggestion. */
    public static @Px int getSideSpacing(@NonNull Context context) {
        return context.getResources().getDimensionPixelSize(
                selectMarginDimen(context, R.dimen.omnibox_suggestion_side_spacing,
                        R.dimen.omnibox_suggestion_side_spacing_smaller,
                        R.dimen.omnibox_suggestion_side_spacing_smallest));
    }

    /** Gets the start padding for an omnibox suggestion's decoration icon. */
    public static @Px int getIconStartPadding(Context context) {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(context)) {
            return context.getResources().getDimensionPixelSize(
                    R.dimen.omnibox_suggestion_24dp_icon_margin_start);
        }
        return context.getResources().getDimensionPixelSize(selectMarginDimen(context,
                R.dimen.omnibox_suggestion_24dp_icon_margin_start_modern_bigger,
                R.dimen.omnibox_suggestion_24dp_icon_margin_start,
                R.dimen.omnibox_suggestion_24dp_icon_margin_start));
    }

    /** Gets the start padding for a large omnibox suggestion decoration icon. */
    public static @Px int getLargeIconStartPadding(Context context) {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(context)) {
            return context.getResources().getDimensionPixelSize(
                    R.dimen.omnibox_suggestion_36dp_icon_margin_start);
        }

        return context.getResources().getDimensionPixelSize(selectMarginDimen(context,
                R.dimen.omnibox_suggestion_36dp_icon_margin_start_smallest,
                R.dimen.omnibox_suggestion_36dp_icon_margin_start,
                R.dimen.omnibox_suggestion_36dp_icon_margin_start));
    }

    /** Gets the end padding for a large omnibox suggestion decoration icon. */
    public static @Px int getLargeIconEndPadding(Context context) {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(context)) {
            return context.getResources().getDimensionPixelSize(
                    R.dimen.omnibox_suggestion_36dp_icon_margin_end);
        }

        return context.getResources().getDimensionPixelSize(
                selectMarginDimen(context, R.dimen.omnibox_suggestion_36dp_icon_margin_end_smallest,
                        R.dimen.omnibox_suggestion_36dp_icon_margin_end,
                        R.dimen.omnibox_suggestion_36dp_icon_margin_end));
    }

    /** Get the top margin for a suggestion that is the beginning of a group. */
    public static int getSuggestionGroupTopMargin(Context context) {
        return context.getResources().getDimensionPixelSize(
                selectMarginDimen(context, R.dimen.omnibox_suggestion_group_vertical_margin,
                        R.dimen.omnibox_suggestion_group_vertical_smaller_margin,
                        R.dimen.omnibox_suggestion_group_vertical_smallest_margin));
    }

    /** Get the top padding for the MV carousel. */
    public static @Px int getCarouselTopPadding(Context context) {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(context)) {
            return context.getResources().getDimensionPixelSize(
                    R.dimen.omnibox_carousel_suggestion_padding);
        }

        return context.getResources().getDimensionPixelSize(
                selectMarginDimen(context, R.dimen.omnibox_carousel_suggestion_padding_smaller,
                        R.dimen.omnibox_carousel_suggestion_padding_smallest,
                        R.dimen.omnibox_carousel_suggestion_padding_smaller));
    }

    /** Get the bottom padding for the MV carousel. */
    public static @Px int getCarouselBottomPadding(Context context) {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(context)) {
            return context.getResources().getDimensionPixelSize(
                    R.dimen.omnibox_carousel_suggestion_padding);
        }

        return context.getResources().getDimensionPixelSize(
                selectMarginDimen(context, R.dimen.omnibox_carousel_suggestion_small_bottom_padding,
                        R.dimen.omnibox_carousel_suggestion_small_bottom_padding,
                        R.dimen.omnibox_carousel_suggestion_padding));
    }

    /** Get the top margin for first suggestion in the omnibox with "active color" enabled. */
    public static @Px int getActiveOmniboxTopSmallMargin(Context context) {
        return context.getResources().getDimensionPixelSize(
                selectMarginDimen(context, R.dimen.omnibox_suggestion_list_active_top_small_margin,
                        R.dimen.omnibox_suggestion_list_active_top_smaller_margin,
                        R.dimen.omnibox_suggestion_list_active_top_small_margin));
    }

    /** Gets the start padding for a header suggestion. */
    public static @Px int getHeaderStartPadding(Context context) {
        return context.getResources().getDimensionPixelSize(
                selectMarginDimen(context, R.dimen.omnibox_suggestion_header_padding_start_modern,
                        R.dimen.omnibox_suggestion_header_padding_start_modern_smaller,
                        R.dimen.omnibox_suggestion_header_padding_start_modern_smallest));
    }

    /** Gets the top padding for a header suggestion. */
    public static int getHeaderTopPadding(Context context) {
        return context.getResources().getDimensionPixelSize(
                selectMarginDimen(context, R.dimen.omnibox_suggestion_header_padding_top,
                        R.dimen.omnibox_suggestion_header_padding_top_smaller,
                        R.dimen.omnibox_suggestion_header_padding_top_smallest));
    }

    /** Returns the min height of the header view. */
    public static int getHeaderMinHeight(Context context) {
        return context.getResources().getDimensionPixelSize(
                selectMarginDimen(context, R.dimen.omnibox_suggestion_header_height_modern_phase2,
                        R.dimen.omnibox_suggestion_header_height_modern_phase2_smaller,
                        R.dimen.omnibox_suggestion_header_height_modern_phase2_smallest));
    }

    /**
     * Returns the size of the spacer on the left side of the status view when the omnibox is
     * focused.
     */
    public static @Px int getFocusedStatusViewLeftSpacing(Context context) {
        return context.getResources().getDimensionPixelSize(
                selectMarginDimen(context, R.dimen.location_bar_status_view_left_space_width,
                        R.dimen.location_bar_status_view_left_space_width_bigger,
                        R.dimen.location_bar_status_view_left_space_width_bigger));
    }

    /**
     * Returns the amount of pixels the toolbar should increased its height by when the omnibox is
     * focused.
     */
    public static @Px int getToolbarOnFocusHeightIncrease(Context context) {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(context)) {
            return 0;
        }

        return context.getResources().getDimensionPixelSize(
                OmniboxFeatures.shouldShowActiveColorOnOmnibox()
                        ? R.dimen.toolbar_url_focus_height_increase_active_color
                        : R.dimen.toolbar_url_focus_height_increase_no_active_color);
    }

    /** Returns the amount of pixels for the toolbar's side padding when the omnibox is focused. */
    public static @Px int getToolbarSidePadding(Context context) {
        return context.getResources().getDimensionPixelSize(
                OmniboxFeatures.shouldShowModernizeVisualUpdate(context)
                        ? OmniboxResourceProvider.selectMarginDimen(context,
                                R.dimen.toolbar_edge_padding_modern,
                                R.dimen.toolbar_edge_padding_modern_smaller,
                                R.dimen.toolbar_edge_padding)
                        : R.dimen.toolbar_edge_padding);
    }

    /** */
    public static @DimenRes int selectMarginDimen(
            Context context, @DimenRes int regular, @DimenRes int smaller, @DimenRes int smallest) {
        if (OmniboxFeatures.shouldShowSmallestMargins(context)) {
            return smallest;
        } else if (OmniboxFeatures.shouldShowSmallerMargins(context)) {
            return smaller;
        }
        return regular;
    }
}
