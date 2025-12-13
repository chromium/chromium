// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Drawable.ConstantState;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.StateListDrawable;
import android.util.SparseArray;
import android.util.TypedValue;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.IncognitoColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.util.ColorUtils;

import java.util.function.Function;

/** Provides resources specific to Omnibox. */
@NullMarked
public class OmniboxResourceProvider {
    private static final String TAG = "OmniboxResourceProvider";

    private static SparseArray<ConstantState> sDrawableCache = new SparseArray<>();
    private static SparseArray<String> sStringCache = new SparseArray<>();
    private static @Nullable Function<Tab, @Nullable Bitmap> sTabFaviconFactory;
    private static @ColorInt @Nullable Integer sUrlBarPrimaryTextColorForTesting;
    private static @ColorInt @Nullable Integer sUrlBarHintTextColorForTesting;

    /**
     * As {@link androidx.appcompat.content.res.AppCompatResources#getDrawable(Context, int)} but
     * potentially augmented with caching. If caching is enabled, there is a single, unbounded cache
     * of ConstantState shared by all contexts.
     */
    public static Drawable getDrawable(Context context, @DrawableRes int res) {
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
     *
     * <p>This function converts cross-platform grit string expansion placeholders to Java
     * placeholders allowing any single string to be used both from C++ and Java. This requires all
     * the arguments to be of String type.
     *
     * @param context current context used to resolve string res
     * @param res string res to retrieve, cache, and expand
     * @param args positional arguments expanded when `res` includes expansion placeholders
     * @return expanded and formatted string representing
     */
    public static String getString(Context context, @StringRes int res, CharSequence... args) {
        ThreadUtils.assertOnUiThread();
        String string = sStringCache.get(res, null);
        if (string == null) {
            string = context.getString(res);

            // Translate `$1`, `$2`, ... strings (found typically on other platforms)
            // to `%1$s`, `%2$s` etc, which are appropriate for Chrome.
            string = string.replaceAll("\\$(\\d+)", "%$1\\$s");

            sStringCache.put(res, string);
        }

        return args.length == 0
                ? string
                : String.format(
                        context.getResources().getConfiguration().getLocales().get(0),
                        string,
                        (Object[]) args);
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
                    public @Nullable ConstantState get(int key) {
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
                    public @Nullable String get(int key) {
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
        if (sUrlBarPrimaryTextColorForTesting != null) {
            return sUrlBarPrimaryTextColorForTesting;
        }
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

    public static void setUrlBarPrimaryTextColorForTesting(@ColorInt int value) {
        sUrlBarPrimaryTextColorForTesting = value;
        ResettersForTesting.register(() -> sUrlBarPrimaryTextColorForTesting = null);
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
        if (sUrlBarHintTextColorForTesting != null) {
            return sUrlBarHintTextColorForTesting;
        }
        return getUrlBarSecondaryTextColor(context, brandedColorScheme);
    }

    public static void setUrlBarHintTextColorForTesting(@ColorInt int value) {
        sUrlBarHintTextColorForTesting = value;
        ResettersForTesting.register(() -> sUrlBarHintTextColorForTesting = null);
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
    public static @ColorInt int getStandardSuggestionBackgroundColor(
            Context context, @BrandedColorScheme int colorScheme) {
        return colorScheme == BrandedColorScheme.INCOGNITO
                ? context.getColor(R.color.omnibox_suggestion_bg_incognito)
                : ContextCompat.getColor(context, R.color.omnibox_suggestion_bg);
    }

    /** Returns the background hover color for suggestions in a model with the given context. */
    private static @ColorInt int getHoverSuggestionBackgroundColor(
            Context context, @BrandedColorScheme int colorScheme) {

        if (colorScheme == BrandedColorScheme.INCOGNITO) {
            return context.getColor(R.color.omnibox_suggestion_bg_hover_incognito);
        }

        // omnibox_suggestion_bg + 8% colorOnSurface
        @ColorInt int baseColor = ContextCompat.getColor(context, R.color.omnibox_suggestion_bg);
        @ColorInt int hoverColor = MaterialColors.getColor(context, R.attr.colorOnSurface, TAG);
        float fraction =
                context.getResources()
                        .getFraction(R.fraction.omnibox_suggestion_bg_hover_overlay_fraction, 1, 1);

        return ColorUtils.overlayColor(baseColor, hoverColor, fraction);
    }

    /** Returns a stateful suggestion background with the select default state. */
    public static Drawable getStatefulSuggestionBackground(
            Context context, @ColorInt int defaultColor, @BrandedColorScheme int colorScheme) {
        var background = new ColorDrawable(defaultColor);
        var hover = new ColorDrawable(getHoverSuggestionBackgroundColor(context, colorScheme));

        // Ripple effect to use when the user interacts with the suggestion.
        var ripple =
                resolveAttributeToDrawable(context, colorScheme, R.attr.selectableItemBackground);

        var statefulBackground = new StateListDrawable();
        statefulBackground.addState(new int[] {android.R.attr.state_selected}, hover);
        statefulBackground.addState(new int[] {android.R.attr.state_hovered}, hover);
        statefulBackground.addState(
                new int[] {android.R.attr.state_selected, android.R.attr.state_hovered}, hover);
        statefulBackground.addState(new int[] {}, background);

        return new LayerDrawable(new Drawable[] {statefulBackground, ripple});
    }

    /**
     * Returns the background color for the suggestions dropdown for the given {@link
     * BrandedColorScheme} with the given context.
     */
    public static @ColorInt int getSuggestionsDropdownBackgroundColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        return brandedColorScheme == BrandedColorScheme.INCOGNITO
                ? context.getColor(R.color.omnibox_dropdown_bg_incognito)
                : ContextCompat.getColor(context, R.color.omnibox_suggestion_dropdown_bg);
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
    public static @Px int getDropdownSideSpacing(Context context) {
        context = maybeReplaceContextForSmallTabletWindow(context);
        return getSideSpacing(context)
                + context.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_dropdown_side_spacing);
    }

    /** Gets the margin, in pixels, on either side of an omnibox suggestion. */
    public static @Px int getSideSpacing(Context context) {
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
     * Returns the amount of pixels the location bar background should increase in height by when
     * the omnibox is focused.
     */
    public static @Px int getLocationBarBackgroundOnFocusHeightIncrease(Context context) {
        return context.getResources()
                .getDimensionPixelSize(R.dimen.location_bar_background_on_focus_height_increase);
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

    public static @Nullable Bitmap getFaviconBitmapForTab(Tab tab) {
        if (sTabFaviconFactory == null) return null;
        return sTabFaviconFactory.apply(tab);
    }

    /**
     * Converts a branded color scheme to a boolean, whether to use incognito colors or day night
     * adaptive colors.
     *
     * @param brandedColorScheme The theme to check.
     * @return A boolean, true for incognito, false for day night adaptive.
     */
    public static boolean convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(
            @BrandedColorScheme int brandedColorScheme) {
        // It is assumed that anywhere that calls this method doesn't actually need to support
        // real branded colors schemes. Instead we just need to determine if it is incognito or not.
        return brandedColorScheme == BrandedColorScheme.INCOGNITO;
    }

    /** Resolves the background color of the chip showing the AI Mode tool active. */
    public static @ColorInt int getAiModeButtonColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getColorSurfaceContainerHigh(context, isIncognito);
    }

    /** Resolves the background color of the chip showing image gen the tool active. */
    public static @ColorInt int getImageGenButtonColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getColorSurfaceContainerLow(context, isIncognito);
    }

    /** Resolves the border color of the active tool chip. */
    public static @ColorInt int getRequestTypeButtonBorderColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getColorPrimaryContainer(context, isIncognito);
    }

    /**
     * A color scheme version of {@link IncognitoColors#getColorSurface(Context, boolean)}. Used for
     * the contrast background of the close button on attachment chips.
     */
    public static @ColorInt int getColorSurface(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getColorSurface(context, isIncognito);
    }

    /**
     * A color scheme version of {@link IncognitoColors#getColorOnSurface(Context, boolean)}. Used
     * for the close src image on attachment chips.
     */
    public static @ColorInt int getColorOnSurface(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getColorOnSurface(context, isIncognito);
    }

    /**
     * Resolves the vivid color used for the border of the tool chip when used as a hint to enter AI
     * Mode, as well as the background of the send button.
     */
    public static @ColorInt int getColorPrimary(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getColorPrimary(context, isIncognito);
    }

    /** Resolves the icon tint to be used for secondary image gen icons, not the banana. */
    public static @ColorInt int getDefaultIconColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getDefaultIconColor(context, isIncognito);
    }

    /** Resolves the icon tint to be used for all the ai mode icons. This is a vivid color. */
    public static @ColorInt int getAiModeHintIconTintColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getDefaultIconColorSecondary(context, isIncognito);
    }

    /** Resolves the icon tint to be used for all the ai mode icons. */
    public static @ColorInt int getAiModeHintBorderColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getColorOnSurfaceWithAlpha16(context, isIncognito);
    }

    /** Resolves the icon tint color for the icons that should be vivid, such as the + button. */
    public static ColorStateList getPrimaryIconTintList(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return ChromeColors.getPrimaryIconTint(context, isIncognito);
    }

    /** Resolves the icon tint color for the icons that should be vivid, such as the + button. */
    public static @ColorInt int getPopupDividerLineColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getDividerLineBgColor(context, isIncognito);
    }

    /** Resolves the icon tint for the plus button on top of the vivid send button. */
    public static @ColorInt int getSendIconContrastColor(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getColorOnPrimary(context, isIncognito);
    }

    /** Resolves the text appearance for the image gen chip. */
    public static @StyleRes int getImageGenButtonTextRes(
            @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getTextMediumThickPrimary(isIncognito);
    }

    /** Resolves the text appearance for the AI Mode chip. This includes a vivid color. */
    public static @StyleRes int getAiModeButtonTextRes(@BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getTextMediumThickAccent1(isIncognito);
    }

    /** Resolves the text appearance for the hint chip, somewhat faded out. */
    public static @StyleRes int getAiModeHintTextRes(@BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getTextMediumThickSecondary(isIncognito);
    }

    /** Resolves the text appearance for menu items in the popup. */
    public static @StyleRes int getPopupButtonTextRes(@BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        return IncognitoColors.getTextMediumPrimary(isIncognito);
    }

    /** Returns the drawable that is to go behind the + button in the search box. */
    public static Drawable getSearchBoxIconBackground(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        @DrawableRes
        int resId =
                isIncognito
                        ? R.drawable.search_box_icon_background_opaque_incognito
                        : R.drawable.search_box_icon_background_opaque;
        return getDrawable(context, resId);
    }

    /** Returns the drawable for the popup menu that shows menu items for context and tools. */
    public static Drawable getPopupBackgroundDrawable(
            Context context, @BrandedColorScheme int brandedColorScheme) {
        boolean isIncognito =
                convertBrandedColorSchemeToIncognitoOrDayNightAdaptive(brandedColorScheme);
        @DrawableRes
        int resId = isIncognito ? R.drawable.menu_bg_tinted_on_dark_bg : R.drawable.menu_bg_tinted;
        return getDrawable(context, resId);
    }

    public static void setTabFaviconFactory(Function<Tab, @Nullable Bitmap> tabFaviconFactory) {
        sTabFaviconFactory = tabFaviconFactory;
    }
}
