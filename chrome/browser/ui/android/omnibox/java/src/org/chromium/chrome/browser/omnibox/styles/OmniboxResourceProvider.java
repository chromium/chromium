// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Drawable.ConstantState;
import android.util.SparseArray;
import android.util.TypedValue;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.DeviceFormFactor;
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
        ConstantState constantState = sDrawableCache.get(res, null);
        if (constantState != null) {
            return constantState.newDrawable(context.getResources());
        }

        Drawable drawable = AppCompatResources.getDrawable(context, res);
        sDrawableCache.put(res, drawable.getConstantState());
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
        String string = sStringCache.get(res, null);
        if (string == null) {
            string = context.getResources().getString(res);
            sStringCache.put(res, string);
        }

        return args.length == 0
                ? string
                : String.format(
                        context.getResources().getConfiguration().getLocales().get(0),
                        string,
                        args);
    }

    /**
     * Clears the drawable cache to avoid, e.g. caching a now incorrectly colored drawable resource.
     */
    public static void invalidateDrawableCache() {
        sDrawableCache.clear();
    }

    public static SparseArray<ConstantState> getDrawableCacheForTesting() {
        return sDrawableCache;
    }

    public static void disableCachesForTesting() {
        sDrawableCache =
                new SparseArray<>() {
                    @Override
                    public ConstantState get(int key) {
                        return null;
                    }

                    @Override
                    public ConstantState get(int key, ConstantState valueIfKeyNotFound) {
                        return valueIfKeyNotFound;
                    }
                };
        sStringCache =
                new SparseArray<>() {
                    @Override
                    public String get(int key) {
                        return null;
                    }

                    @Override
                    public String get(int key, String valueIfKeyNotFound) {
                        return valueIfKeyNotFound;
                    }
                };
    }

    public static void reenableCachesForTesting() {
        sDrawableCache = new SparseArray<>();
        sStringCache = new SparseArray<>();
    }

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
        Context wrappedContext =
                maybeWrapContextForIncognitoColorScheme(context, brandedColorScheme);
        @DrawableRes int resourceId = resolveAttributeToDrawableRes(wrappedContext, attributeResId);
        return getDrawable(wrappedContext, resourceId);
    }

    /**
     * Returns the ColorScheme based on the incognito state and the background color.
     *
     * @param context The {@link Context}.
     * @param isIncognitoBranded Whether incognito mode is enabled.
     * @param primaryBackgroundColor The primary background color of the omnibox.
     * @return The {@link BrandedColorScheme}.
     */
    public static @BrandedColorScheme int getBrandedColorScheme(
            Context context, boolean isIncognitoBranded, @ColorInt int primaryBackgroundColor) {
        if (isIncognitoBranded) return BrandedColorScheme.INCOGNITO;

        if (ThemeUtils.isUsingDefaultToolbarColor(
                context, isIncognitoBranded, primaryBackgroundColor)) {
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
        final @ColorInt int color =
                brandedColorScheme == BrandedColorScheme.INCOGNITO
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
        return ChromeColors.getSurfaceColor(context, R.dimen.omnibox_suggestion_bg_elevation);
    }

    /**
     * Returns the background color for the suggestions dropdown in a "standard" (non-incognito)
     * TabModel with the given context.
     */
    public static @ColorInt int getSuggestionsDropdownStandardBackgroundColor(Context context) {
        return ChromeColors.getSurfaceColor(
                context, R.dimen.omnibox_suggestion_dropdown_bg_elevation);
    }

    /**
     * Returns the background color for the suggestions dropdown in an incognito TabModel with the
     * given context.
     */
    public static @ColorInt int getSuggestionsDropdownIncognitoBackgroundColor(Context context) {
        return context.getColor(R.color.omnibox_dropdown_bg_incognito);
    }

    /**
     * Returns the background color for the suggestions dropdown for the given {@link
     * BrandedColorScheme} with the given context.
     */
    public static @ColorInt int getSuggestionsDropdownBackgroundColorForColorScheme(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        return brandedColorScheme == BrandedColorScheme.INCOGNITO
                ? getSuggestionsDropdownIncognitoBackgroundColor(context)
                : getSuggestionsDropdownStandardBackgroundColor(context);
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

    /** Gets the margin, in pixels, on either side of an omnibox suggestion list. */
    public static @Px int getDropdownSideSpacing(@NonNull Context context) {
        context = maybeReplaceContextForSmallTabletWindow(context);
        return getSideSpacing(context)
                + context.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_dropdown_side_spacing);
    }

    /** Gets the margin, in pixels, on either side of an omnibox suggestion. */
    public static @Px int getSideSpacing(@NonNull Context context) {
        context = maybeReplaceContextForSmallTabletWindow(context);
        return context.getResources()
                .getDimensionPixelSize(R.dimen.omnibox_suggestion_side_spacing_smallest);
    }

    /** Get the top padding for the MV carousel. */
    public static @Px int getMostVisitedCarouselTopPadding(Context context) {
        context = maybeReplaceContextForSmallTabletWindow(context);
        return context.getResources()
                .getDimensionPixelSize(R.dimen.omnibox_carousel_suggestion_padding_smaller);
    }

    /** Get the bottom padding for the MV carousel. */
    public static @Px int getMostVisitedCarouselBottomPadding(Context context) {
        context = maybeReplaceContextForSmallTabletWindow(context);
        return context.getResources()
                .getDimensionPixelSize(R.dimen.omnibox_carousel_suggestion_padding);
    }

    /** Get the top margin for first suggestion in the omnibox with "active color" enabled. */
    public static @Px int getActiveOmniboxTopSmallMargin(Context context) {
        return context.getResources()
                .getDimensionPixelSize(R.dimen.omnibox_suggestion_list_active_top_small_margin);
    }

    /** Gets the start padding for a header suggestion. */
    public static @Px int getHeaderStartPadding(Context context) {
        context = maybeReplaceContextForSmallTabletWindow(context);
        return context.getResources()
                .getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_start);
    }

    /**
     * Returns the size of the spacer on the left side of the status view when the omnibox is
     * focused.
     */
    public static @Px int getFocusedStatusViewLeftSpacing(Context context) {
        return context.getResources()
                .getDimensionPixelSize(R.dimen.location_bar_status_view_left_space_width_bigger);
    }

    /**
     * Returns the amount of pixels the toolbar should increased its height by when the omnibox is
     * focused.
     */
    public static @Px int getToolbarOnFocusHeightIncrease(Context context) {
        return context.getResources()
                .getDimensionPixelSize(R.dimen.toolbar_url_focus_height_increase);
    }

    /** Returns the amount of pixels for the toolbar's side padding when the omnibox is focused. */
    public static @Px int getToolbarSidePadding(Context context) {
        return context.getResources().getDimensionPixelSize(R.dimen.toolbar_edge_padding);
    }

    /**
     * Returns the amount of pixels for the toolbar's side padding when the omnibox is pinned on the
     * top of the screen in NTP.
     */
    public static @Px int getToolbarSidePaddingForNtp(Context context) {
        return context.getResources().getDimensionPixelSize(R.dimen.toolbar_edge_padding_ntp);
    }

    /** Return the width of the Omnibox Suggestion decoration icon. */
    public static @Px int getSuggestionDecorationIconSizeWidth(Context context) {
        Context wrappedContext = maybeReplaceContextForSmallTabletWindow(context);
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && wrappedContext == context) {
            return context.getResources()
                    .getDimensionPixelSize(R.dimen.omnibox_suggestion_icon_area_size_modern);
        }

        return context.getResources()
                .getDimensionPixelSize(R.dimen.omnibox_suggestion_icon_area_size);
    }

    /**
     * Wraps the context if necessary to force dark resources for incognito.
     *
     * @param context The {@link Context} to be wrapped.
     * @param brandedColorScheme Current color scheme.
     * @return Context with resources appropriate to the {@link BrandedColorScheme}.
     */
    private static Context maybeWrapContextForIncognitoColorScheme(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        // Only wraps the context in case of incognito.
        if (brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            return NightModeUtils.wrapContextWithNightModeConfig(
                    context, R.style.Theme_Chromium_TabbedMode, /* nightMode= */ true);
        }

        return context;
    }

    /**
     * Replace the given context with a new one where smallestScreenWidthDp is set to the current
     * screen width, if: 1. The tablet revamp is enabled and the current device is a tablet 2. The
     * current window width is narrower than 600dp. The returned context can be used to retrieve
     * resources appropriate for a smaller minimum screen size. If 1 and 2 aren't true, the original
     * context is returned.
     *
     * @param context The context to replace.
     */
    @VisibleForTesting
    static Context maybeReplaceContextForSmallTabletWindow(Context context) {
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            return context;
        }

        Configuration existingConfig = context.getResources().getConfiguration();
        if (existingConfig.screenWidthDp >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP) {
            return context;
        }

        Configuration newConfig = new Configuration(existingConfig);
        newConfig.smallestScreenWidthDp = existingConfig.screenWidthDp;

        return context.createConfigurationContext(newConfig);
    }

    /**
     * @param context The context to retrieve the resources from.
     * @return the color for the additional text.
     */
    @ColorInt
    public static int getAdditionalTextColor(Context context) {
        return SemanticColorUtils.getDefaultTextColorSecondary(context);
    }
}
