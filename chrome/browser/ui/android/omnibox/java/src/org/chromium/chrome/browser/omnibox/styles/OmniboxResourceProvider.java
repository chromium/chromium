// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ComponentCallbacks2;
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

import androidx.annotation.AttrRes;
import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.Fusebox.FuseboxLayoutMode;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.IncognitoColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.DrawableUtils;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;
import java.util.function.Function;
import java.util.regex.Pattern;

/**
 * Provides resources specific to Omnibox.
 *
 * <p>This class is actively being migrated to instance methods to remove the requirement that each
 * of its clients caches state information, which can lead to an inconsistent UI and an
 * overabundance of caching.
 *
 * <p><b>NOTE: No new static methods should be added to this class.</b> The component is being
 * migrated to instance methods. Where possible, please use an {@link OmniboxResourceProvider}
 * instance. Existing static methods are set to be retired.
 */
@NullMarked
public class OmniboxResourceProvider implements ComponentCallbacks2 {
    /** Identifies the current layout constraints of the screen/window. */
    @IntDef({
        LayoutSize.UNKNOWN,
        LayoutSize.PHONE,
        LayoutSize.TABLET_NARROW,
        LayoutSize.TABLET_WIDE,
        LayoutSize.DESKTOP
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface LayoutSize {
        int UNKNOWN = 0;
        int PHONE = 1; // Physical phone layout.
        int TABLET_NARROW = 2; // Tablet screen, but in narrow split-screen window (< 600dp).
        int TABLET_WIDE = 3; // Tablet screen, in wide window (>= 600dp).
        int DESKTOP = 4; // Desktop platform.
    }

    private static final String TAG = "OmniboxResourceProvider";

    private static final Pattern GRIT_PLACEHOLDER = Pattern.compile("\\$(\\d+)");
    private static @ColorInt @Nullable Integer sUrlBarPrimaryTextColorForTesting;
    private static @ColorInt @Nullable Integer sUrlBarHintTextColorForTesting;

    // Tracking states for cache invalidation.
    private @LayoutSize int mLayoutSizeState = LayoutSize.UNKNOWN;
    private @Nullable String mLocaleState;
    private boolean mDarkModeState;
    private int mDensityDpiState;
    private float mFontScaleState;

    private final Context mContext;
    private ResourceCache mCache;

    private @BrandedColorScheme int mBrandedColorScheme = BrandedColorScheme.APP_DEFAULT;
    private final @Nullable Function<Tab, @Nullable Bitmap> mTabFaviconFactory;

    public OmniboxResourceProvider(Context context, @BrandedColorScheme int brandedColorScheme) {
        this(context, brandedColorScheme, /* tabFaviconFactory= */ null);
    }

    /**
     * @param context The context to resolve resources with.
     * @param brandedColorScheme The initial {@link BrandedColorScheme}.
     * @param tabFaviconFactory The function returning the favicon for a given tab.
     */
    public OmniboxResourceProvider(
            Context context,
            @BrandedColorScheme int brandedColorScheme,
            @Nullable Function<Tab, @Nullable Bitmap> tabFaviconFactory) {
        mContext = context;
        mBrandedColorScheme = brandedColorScheme;
        mTabFaviconFactory = tabFaviconFactory;
        mContext.registerComponentCallbacks(this);
        updateStateAndCache(context.getResources().getConfiguration());
    }

    /** Unregisters context callbacks. */
    public void destroy() {
        mContext.unregisterComponentCallbacks(this);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        updateStateAndCache(newConfig);
    }

    @Override
    public void onLowMemory() {}

    @Override
    public void onTrimMemory(int level) {}

    /**
     * Set branded color scheme.
     *
     * @see #setBrandedColorScheme(Context, ...)
     */
    public void setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
        if (mBrandedColorScheme != brandedColorScheme) {
            mBrandedColorScheme = brandedColorScheme;
            updateStateAndCache(mContext.getResources().getConfiguration());
        }
    }

    public @BrandedColorScheme int getBrandedColorScheme() {
        return mBrandedColorScheme;
    }

    /**
     * @return Whether the current color scheme is incognito.
     */
    public boolean isIncognito() {
        return mBrandedColorScheme == BrandedColorScheme.INCOGNITO;
    }

    // NullAway cannot recognize mLayoutSizeState == UNKONWN as factor impacting the outcome.
    // We're intentionally starting with values that are not valid to trigger creation of the
    // correct target context. One way around the problem would be to always instantiate mCache
    // in the constructor and let it be recreated below, but this is slightly less wasteful.
    @SuppressWarnings("NullAway")
    @Initializer
    private void updateStateAndCache(Configuration config) {
        @LayoutSize int newLayoutSize = computeLayoutSize(config);
        String newLocale = config.getLocales().toLanguageTags();
        boolean darkModeState =
                (config.uiMode & Configuration.UI_MODE_NIGHT_MASK)
                        == Configuration.UI_MODE_NIGHT_YES;
        // Chrome activities handle density and font scale changes in-process, so cached pixel
        // dimensions must be re-resolved when either changes.
        int densityDpi = config.densityDpi;
        float fontScale = config.fontScale;

        if (newLayoutSize == mLayoutSizeState
                && newLocale.equals(mLocaleState)
                && darkModeState == mDarkModeState
                && densityDpi == mDensityDpiState
                && fontScale == mFontScaleState) {
            return;
        }

        mLayoutSizeState = newLayoutSize;
        mLocaleState = newLocale;
        mDarkModeState = darkModeState;
        mDensityDpiState = densityDpi;
        mFontScaleState = fontScale;

        mCache = new ResourceCache(mContext);
    }

    private @LayoutSize int computeLayoutSize(Configuration config) {
        if (OmniboxCapabilities.isDesktopPlatform()) {
            return LayoutSize.DESKTOP;
        }
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            return config.screenWidthDp < DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP
                    ? LayoutSize.TABLET_NARROW
                    : LayoutSize.TABLET_WIDE;
        }
        return LayoutSize.PHONE;
    }

    /**
     * Resolves a drawable resource using the instance context and cache, automatically casting the
     * result to the target {@link Drawable} subtype.
     *
     * @param <T> Target {@link Drawable} subtype to cast to.
     * @param res Drawable resource ID to retrieve.
     * @return Resolved {@link Drawable} cast to the target type {@code T}.
     */
    @SuppressWarnings("unchecked")
    public <T extends Drawable> T getDrawable(@DrawableRes int res) {
        return (T) mCache.getDrawable(res);
    }

    /** Resolves a string resource and expands positional arguments. */
    public String getString(@StringRes int res, CharSequence... args) {
        return formatStringArgs(mCache.getString(res), args);
    }

    /** Resolves a dimension size and caches the result. */
    public @Px int getDimen(@DimenRes int resId) {
        return mCache.getDimen(resId);
    }

    /** Resolves a color resource and caches the result. */
    public @ColorInt int getColor(@ColorRes int resId) {
        return mCache.getColor(resId);
    }

    /** Resolves a string resource and caches the result. */
    public String getString(@StringRes int resId) {
        return mCache.getString(resId);
    }

    /**
     * Returns a drawable for a given attribute depending on the current {@link BrandedColorScheme}.
     *
     * @param attributeResId A resource ID of an attribute to resolve.
     * @return A background drawable providing ripple effect.
     */
    public Drawable resolveAttributeToDrawable(@AttrRes int attributeResId) {
        Context wrappedContext =
                maybeWrapContextForIncognitoColorScheme(mContext, getBrandedColorScheme());
        @DrawableRes int resourceId = resolveAttributeToDrawableRes(wrappedContext, attributeResId);
        return assumeNonNull(wrappedContext.getDrawable(resourceId));
    }

    /**
     * Get url bar primary text color.
     *
     * @see #getUrlBarPrimaryTextColor(Context, ...)
     */
    public @ColorInt int getUrlBarPrimaryTextColor() {
        return getUrlBarPrimaryTextColor(mContext, getBrandedColorScheme());
    }

    /**
     * Get url bar secondary text color.
     *
     * @see #getUrlBarSecondaryTextColor(Context, ...)
     */
    public @ColorInt int getUrlBarSecondaryTextColor() {
        return getUrlBarSecondaryTextColor(mContext, getBrandedColorScheme());
    }

    /**
     * Get url bar hint text color.
     *
     * @see #getUrlBarHintTextColor(Context, ...)
     */
    public @ColorInt int getUrlBarHintTextColor() {
        return getUrlBarHintTextColor(mContext, getBrandedColorScheme());
    }

    /**
     * Get url bar danger color.
     *
     * @see #getUrlBarDangerColor(Context, ...)
     */
    public @ColorInt int getUrlBarDangerColor() {
        return getUrlBarDangerColor(mContext, getBrandedColorScheme());
    }

    /**
     * Get url bar secure color.
     *
     * @see #getUrlBarSecureColor(Context, ...)
     */
    public @ColorInt int getUrlBarSecureColor() {
        return getUrlBarSecureColor(mContext, getBrandedColorScheme());
    }

    // Suggestions are only shown when the omnibox is focused, hence LIGHT_BRANDED_THEME and
    // DARK_BRANDED_THEME are ignored by the suggestion text colors below.

    /** Returns the primary text color for the suggestions. */
    public @ColorInt int getSuggestionPrimaryTextColor() {
        return isIncognito()
                ? mCache.getColor(R.color.default_text_color_light)
                : mCache.getColorAttr(R.attr.colorOnSurface);
    }

    /** Returns the secondary text color for the suggestions. */
    public @ColorInt int getSuggestionSecondaryTextColor() {
        return isIncognito()
                ? mCache.getColor(R.color.default_text_color_secondary_light)
                : mCache.getColorAttr(R.attr.colorOnSurfaceVariant);
    }

    /** Returns the URL text color for the suggestions. */
    public @ColorInt int getSuggestionUrlTextColor() {
        return isIncognito()
                ? mCache.getColor(R.color.suggestion_url_color_incognito)
                : SemanticColorUtils.getDefaultTextColorLink(mContext);
    }

    /** Get status separator color. */
    public @ColorInt int getStatusSeparatorColor() {
        @ColorRes
        int res =
                switch (mBrandedColorScheme) {
                    case BrandedColorScheme.LIGHT_BRANDED_THEME ->
                            R.color.locationbar_status_separator_color_dark;
                    case BrandedColorScheme.DARK_BRANDED_THEME ->
                            R.color.locationbar_status_separator_color_light;
                    case BrandedColorScheme.INCOGNITO ->
                            R.color.locationbar_status_separator_color_incognito;
                    default -> 0;
                };
        return res != 0 ? mCache.getColor(res) : mCache.getColorAttr(R.attr.colorOutline);
    }

    /** Get status preview text color. */
    public @ColorInt int getStatusPreviewTextColor() {
        @ColorRes
        int res =
                switch (mBrandedColorScheme) {
                    case BrandedColorScheme.LIGHT_BRANDED_THEME ->
                            R.color.locationbar_status_preview_color_dark;
                    case BrandedColorScheme.DARK_BRANDED_THEME ->
                            R.color.locationbar_status_preview_color_light;
                    case BrandedColorScheme.INCOGNITO ->
                            R.color.locationbar_status_preview_color_incognito;
                    default -> 0;
                };
        return res != 0 ? mCache.getColor(res) : mCache.getColorAttr(R.attr.colorPrimary);
    }

    /** Get status offline text color. */
    public @ColorInt int getStatusOfflineTextColor() {
        @ColorRes
        int res =
                switch (mBrandedColorScheme) {
                    case BrandedColorScheme.LIGHT_BRANDED_THEME ->
                            R.color.locationbar_status_offline_color_dark;
                    case BrandedColorScheme.DARK_BRANDED_THEME ->
                            R.color.locationbar_status_offline_color_light;
                    case BrandedColorScheme.INCOGNITO ->
                            R.color.locationbar_status_offline_color_incognito;
                    default -> R.color.default_text_color_secondary_list;
                };
        return mCache.getColor(res);
    }

    /**
     * Get standard suggestion background color.
     *
     * @see #getStandardSuggestionBackgroundColor(Context, ...)
     */
    public @ColorInt int getStandardSuggestionBackgroundColor() {
        return getStandardSuggestionBackgroundColor(mContext, getBrandedColorScheme());
    }

    /** Returns a stateful suggestion background with the select default state. */
    public Drawable getStatefulSuggestionBackground(@ColorInt int defaultColor) {
        var background = new ColorDrawable(defaultColor);
        var hover = new ColorDrawable(getHoverSuggestionBackgroundColor());

        // Ripple effect to use when the user interacts with the suggestion.
        var ripple = resolveAttributeToDrawable(R.attr.selectableItemBackground);

        var statefulBackground = new StateListDrawable();
        statefulBackground.addState(new int[] {android.R.attr.state_selected}, hover);
        statefulBackground.addState(new int[] {android.R.attr.state_hovered}, hover);
        statefulBackground.addState(
                new int[] {android.R.attr.state_selected, android.R.attr.state_hovered}, hover);
        statefulBackground.addState(new int[] {}, background);

        return new LayerDrawable(new Drawable[] {statefulBackground, ripple});
    }

    /** Returns the background hover color for suggestions. */
    private @ColorInt int getHoverSuggestionBackgroundColor() {
        @ColorInt int baseColor = getStandardSuggestionBackgroundColor();
        @ColorInt
        int hoverColor =
                isIncognito()
                        ? mContext.getColor(R.color.baseline_neutral_90)
                        : MaterialColors.getColor(mContext, R.attr.colorOnSurface, TAG);
        float fraction =
                mContext.getResources()
                        .getFraction(R.fraction.omnibox_suggestion_bg_hover_overlay_fraction, 1, 1);

        return ColorUtils.overlayColor(baseColor, hoverColor, fraction);
    }

    /**
     * Get suggestions dropdown background color.
     *
     * @see #getSuggestionsDropdownBackgroundColor(Context, ...)
     */
    public @ColorInt int getSuggestionsDropdownBackgroundColor() {
        return getSuggestionsDropdownBackgroundColor(mContext, getBrandedColorScheme());
    }

    /** Get suggestion background color for the instance context and color scheme. */
    public @ColorInt int getSuggestionBackgroundColor(
            @FuseboxLayoutMode int layoutMode, boolean isDropdownContainer) {
        if (layoutMode == FuseboxLayoutMode.SUGGESTIONS_POPOVER) {
            return mCache.getColor(
                    isIncognito()
                            ? R.color.gm3_baseline_surface_container_dark
                            : R.color.omnibox_popover_suggestion_bg_color);
        }
        return isDropdownContainer
                ? getSuggestionsDropdownBackgroundColor(mContext, getBrandedColorScheme())
                : getStandardSuggestionBackgroundColor(mContext, getBrandedColorScheme());
    }

    /**
     * Returns the background color for the toolbar pill for tablets. Because tablets always ignore
     * any branded theme, can collapse to if incog or not.
     */
    public @ColorInt int getTabletToolbarTextBoxBackgroundColor() {
        return mCache.getColor(
                isIncognito()
                        ? R.color.toolbar_text_box_background_incognito
                        : R.color.toolbar_text_box_bg_color);
    }

    /** Returns the background color for the toolbar pill when the omnibox is in standby. */
    public @ColorInt int getTabletToolbarTextBoxStandbyBackgroundColor() {
        // Not cached: resolved against a night-mode wrapped context in incognito.
        Context wrappedContext =
                maybeWrapContextForIncognitoColorScheme(mContext, getBrandedColorScheme());
        return SemanticColorUtils.getColorSurface(wrappedContext);
    }

    /** Gets the margin, in pixels, on either side of an omnibox suggestion list. */
    public @Px int getDropdownSideSpacing() {
        return getSideSpacing() + mCache.getDimen(R.dimen.omnibox_suggestion_dropdown_side_spacing);
    }

    /** Returns the top padding for the Omnibox suggestions dropdown list. */
    public @Px int getDropdownTopPadding() {
        return mCache.getDimen(R.dimen.omnibox_suggestion_list_padding_top);
    }

    /** Returns the bottom padding for the Omnibox suggestions dropdown list. */
    public @Px int getDropdownBottomPadding() {
        return mCache.getDimen(R.dimen.omnibox_suggestion_list_padding_bottom);
    }

    /** Gets the margin, in pixels, on either side of an omnibox suggestion. */
    public @Px int getSideSpacing() {
        return mCache.getDimen(R.dimen.omnibox_suggestion_side_spacing_smallest);
    }

    /** Gets the edge size of a suggestion icon. */
    public @Px int getEdgeSize() {
        if (OmniboxCapabilities.isDesktopPlatform()) {
            return mCache.getDimen(R.dimen.omnibox_desktop_small_decoration_icon_size);
        }
        return mCache.getDimen(R.dimen.omnibox_suggestion_24dp_icon_size);
    }

    /** Gets the edge size of a large suggestion icon. */
    public @Px int getEdgeSizeLargeIcon() {
        if (OmniboxCapabilities.isDesktopPlatform()) {
            return mCache.getDimen(R.dimen.omnibox_desktop_large_decoration_icon_size);
        }
        return mCache.getDimen(R.dimen.omnibox_suggestion_36dp_icon_size);
    }

    /** Gets the rounding radius of a large suggestion icon. */
    public @Px int getLargeIconRoundingRadius() {
        return mCache.getDimen(R.dimen.omnibox_large_icon_rounding_radius);
    }

    /** Gets the rounding radius of a small suggestion icon. */
    public @Px int getSmallIconRoundingRadius() {
        return mCache.getDimen(R.dimen.omnibox_small_icon_rounding_radius);
    }

    /** Returns the layout size of the status icon in pixels. */
    public @Px int getStatusIconSize() {
        if (OmniboxCapabilities.isDesktopPlatform()) {
            return mCache.getDimen(R.dimen.omnibox_search_engine_logo_composed_size_desktop);
        }
        return mCache.getDimen(R.dimen.omnibox_search_engine_logo_composed_size);
    }

    /**
     * Returns the dimension resource ID for the status icon corner radius.
     *
     * @param focused Whether the omnibox input field currently has focus.
     */
    public @DimenRes int getStatusIconCornerRadiusRes(boolean focused) {
        if (focused && OmniboxFeatures.sPreviewMatchFavicons.isEnabled()) {
            return R.dimen.omnibox_small_icon_rounding_radius;
        }
        return OmniboxCapabilities.isDesktopPlatform()
                ? R.dimen.omnibox_search_engine_logo_composed_half_size_desktop
                : R.dimen.omnibox_search_engine_logo_composed_half_size;
    }

    /** Get most visited carousel top padding. */
    public @Px int getMostVisitedCarouselTopPadding() {
        return mCache.getDimen(R.dimen.omnibox_carousel_suggestion_padding_smaller);
    }

    /** Get most visited carousel bottom padding. */
    public @Px int getMostVisitedCarouselBottomPadding() {
        return mCache.getDimen(R.dimen.omnibox_carousel_suggestion_padding);
    }

    /** Get header start padding. */
    public @Px int getHeaderStartPadding() {
        if (OmniboxCapabilities.isDesktopPlatform()) {
            return mCache.getDimen(R.dimen.omnibox_suggestion_header_padding_start_desktop);
        }
        return mCache.getDimen(R.dimen.omnibox_suggestion_header_padding_start);
    }

    /**
     * Returns the amount of pixels the location bar background should increase in height by when
     * the omnibox is focused.
     */
    public @Px int getLocationBarBackgroundOnFocusHeightIncrease() {
        return mCache.getDimen(R.dimen.location_bar_background_on_focus_height_increase);
    }

    /** Get suggestion decoration icon size width. */
    public @Px int getSuggestionDecorationIconSizeWidth() {
        return mCache.getDimen(R.dimen.omnibox_suggestion_icon_area_size);
    }

    /** Returns the height of the content of an Omnibox Suggestion. */
    public @Px int getSuggestionContentHeight() {
        if (OmniboxCapabilities.isDesktopPlatform()) {
            return mCache.getDimen(R.dimen.omnibox_suggestion_content_height_desktop);
        }
        return mCache.getDimen(R.dimen.omnibox_suggestion_content_height);
    }

    /** Returns the height of a single line Omnibox Suggestion. */
    public @Px int getSuggestionCompactContentHeight() {
        if (OmniboxCapabilities.isDesktopPlatform()) {
            return mCache.getDimen(R.dimen.omnibox_suggestion_content_height_desktop);
        }
        return mCache.getDimen(R.dimen.omnibox_suggestion_compact_content_height);
    }

    /** Returns the min height of an Omnibox Suggestion view. */
    public @Px int getSuggestionMinHeight(int lineCount) {
        if (OmniboxCapabilities.isDesktopPlatform()) {
            return mCache.getDimen(
                    lineCount > 1
                            ? R.dimen.omnibox_suggestion_content_height_desktop_multiline
                            : R.dimen.omnibox_suggestion_content_height_desktop);
        }
        return mCache.getDimen(
                lineCount > 1
                        ? R.dimen.omnibox_suggestion_minimum_content_height_multiline
                        : R.dimen.omnibox_suggestion_minimum_content_height);
    }

    /** Returns the vertical padding for a suggestion. */
    public @Px int getSuggestionContentVerticalPadding() {
        if (OmniboxCapabilities.isDesktopPlatform()) {
            return 0;
        }
        return mCache.getDimen(R.dimen.omnibox_suggestion_content_padding);
    }

    /** Resolves the background color of the chip showing the active tool. */
    public @ColorInt int getRequestTypeButtonColor() {
        return IncognitoColors.getInteractableChipBgColor(mContext, isIncognito());
    }

    /**
     * Resolves the surface color. Used for the contrast background of the close button on
     * attachment chips.
     */
    public @ColorInt int getColorSurface() {
        return IncognitoColors.getColorSurface(mContext, isIncognito());
    }

    /** Resolves the on-surface color. Used for the close src image on attachment chips. */
    public @ColorInt int getColorOnSurface() {
        return IncognitoColors.getColorOnSurface(mContext, isIncognito());
    }

    /** Resolves the surface container high color. Used for the activation chip. */
    public @ColorInt int getColorSurfaceContainerHigh() {
        return IncognitoColors.getColorSurfaceContainerHigh(mContext, isIncognito());
    }

    /** Resolves the surface container highest color. Used for the activation chip. */
    public @ColorInt int getColorSurfaceContainerHighest() {
        return IncognitoColors.getColorSurfaceContainerHighest(mContext, isIncognito());
    }

    /**
     * Resolves the vivid color used for the border of the tool chip when used as a hint to enter AI
     * Mode, as well as the background of the send button.
     */
    public @ColorInt int getColorPrimary() {
        return IncognitoColors.getColorPrimary(mContext, isIncognito());
    }

    /** Resolves the icon tint to be used for secondary image gen icons, not the banana. */
    public @ColorInt int getDefaultIconColor() {
        return IncognitoColors.getDefaultIconColor(mContext, isIncognito());
    }

    /** Resolves the icon tint color for the icons that should be vivid, such as the + button. */
    public ColorStateList getPrimaryIconTintList() {
        return mCache.getColorStateList(ChromeColors.getPrimaryIconTintRes(isIncognito()));
    }

    /** Resolves the secondary icon tint color. */
    public ColorStateList getSecondaryIconTintList() {
        return mCache.getColorStateList(ChromeColors.getSecondaryIconTintRes(isIncognito()));
    }

    /**
     * Resolves the icon tint for Fusebox popup items (primary for bottom sheet, secondary for plus
     * menu).
     *
     * @param isBottomSheet Whether the popup is presented as a bottom sheet.
     * @return Tint list for Fusebox popup icons.
     */
    public ColorStateList getFuseboxPopupIconTintList(boolean isBottomSheet) {
        return isBottomSheet ? getPrimaryIconTintList() : getSecondaryIconTintList();
    }

    /**
     * Resolves the icon background tint for Fusebox popup items (only present for bottom sheet).
     *
     * @param isBottomSheet Whether the popup is presented as a bottom sheet.
     * @return Background tint list for Fusebox popup icons, or null if unstyled.
     */
    public @Nullable ColorStateList getFuseboxPopupIconBackgroundTintList(boolean isBottomSheet) {
        return isBottomSheet ? getPrimaryIconBackgroundTintList() : null;
    }

    /**
     * Resolves the icon dimension for Fusebox popup items (24dp for bottom sheet, 20dp for plus
     * menu). Carousel tiles use {@link #getFuseboxCarouselIconSize()} instead.
     *
     * @param isBottomSheet Whether the popup is presented as a bottom sheet.
     * @return Icon size in pixels.
     */
    public @Px int getFuseboxPopupIconSize(boolean isBottomSheet) {
        if (isBottomSheet) {
            return mCache.getDimen(R.dimen.fusebox_bottom_sheet_attachment_icon_size);
        }
        return mCache.getDimen(
                OmniboxFeatures.sOmniboxFuseboxPopupVariations.isEnabled()
                        ? R.dimen.fusebox_popup_item_large_icon_size
                        : R.dimen.fusebox_popup_item_icon_size);
    }

    /**
     * Resolves the icon dimension for Fusebox popup carousel tiles (30dp when popup variations are
     * enabled, 24dp otherwise).
     *
     * @return Icon size in pixels.
     */
    public @Px int getFuseboxCarouselIconSize() {
        return mCache.getDimen(
                OmniboxFeatures.sOmniboxFuseboxPopupVariations.isEnabled()
                        ? R.dimen.fusebox_carousel_item_large_icon_size
                        : R.dimen.fusebox_bottom_sheet_attachment_icon_size);
    }

    /** Resolves the background tint for primary icons. */
    public ColorStateList getPrimaryIconBackgroundTintList() {
        return IncognitoColors.getColorSurfaceContainerTintList(mContext, isIncognito());
    }

    /** Resolves the divider line color for the popup. */
    public @ColorInt int getPopupDividerLineColor() {
        return IncognitoColors.getDividerLineBgColor(mContext, isIncognito());
    }

    /** Resolves the icon tint for the plus button on top of the vivid send button. */
    public @ColorInt int getSendIconContrastColor() {
        return IncognitoColors.getColorOnPrimary(mContext, isIncognito());
    }

    /** Resolves the text appearance for the active tool button. */
    public @StyleRes int getRequestTypeButtonTextRes() {
        return IncognitoColors.getTextMediumThickPrimary(isIncognito());
    }

    /** Resolves the text appearance for menu items in the popup. */
    public @StyleRes int getPopupButtonTextRes() {
        return OmniboxFeatures.sOmniboxFuseboxPopupVariations.isEnabled()
                ? IncognitoColors.getTextLargePrimary(isIncognito())
                : IncognitoColors.getTextMediumPrimary(isIncognito());
    }

    /**
     * Resolves the text appearance for the label underneath carousel tiles in the popup. Unlike
     * list rows, this stays medium when popup variations enlarge the list text.
     */
    public @StyleRes int getAttachmentButtonTextRes() {
        return IncognitoColors.getTextMediumPrimary(isIncognito());
    }

    /** Resolves the text appearance for header visibility text in the popup. */
    public @StyleRes int getPopupHeaderVisibilityTextRes() {
        return IncognitoColors.getTextSmallSecondary(isIncognito());
    }

    /**
     * Resolves the text appearance for the attachments header in the popup.
     *
     * @param useLarge Whether to use thick headline2 text (carousel) or medium header text.
     */
    public @StyleRes int getPopupAttachmentsHeaderTextRes(boolean useLarge) {
        if (!useLarge) {
            return IncognitoColors.getTextMediumThickSecondary(isIncognito());
        }
        return IncognitoColors.getHeadline2ThickPrimary(isIncognito());
    }

    /** Returns the drawable that normally goes behind the plus button. */
    public Drawable getSearchBoxIconBackground() {
        @Px int size = mCache.getDimen(R.dimen.small_icon_background_size);
        return DrawableUtils.getIconBackground(mContext, isIncognito(), size, size);
    }

    /** Get popover navigate button background. */
    public Drawable getPopoverNavigateButtonBackground() {
        @DrawableRes
        int resId =
                isIncognito()
                        ? R.drawable.fusebox_popover_navigate_button_background_incognito
                        : R.drawable.fusebox_popover_navigate_button_background;
        return getDrawable(resId);
    }

    /** Get popover plus button background. */
    public Drawable getPopoverPlusButtonBackground() {
        @DrawableRes
        int resId =
                isIncognito()
                        ? R.drawable.fusebox_popover_plus_button_background_incognito
                        : R.drawable.fusebox_popover_plus_button_background;
        return getDrawable(resId);
    }

    /** Get popup background drawable. */
    public Drawable getPopupBackgroundDrawable() {
        @DrawableRes
        int resId =
                OmniboxFeatures.shouldShowBottomSheetPopup()
                        ? isIncognito()
                                ? R.drawable.fusebox_popup_bg_tinted_on_dark_bg
                                : R.drawable.fusebox_popup_bg_tinted
                        : isIncognito()
                                ? R.drawable.menu_bg_tinted_on_dark_bg
                                : R.drawable.menu_bg_tinted;
        return getDrawable(resId);
    }

    // ============================================================================================
    // Static methods.
    // NOTE: Do not add new static methods here. The component is actively being migrated to
    // instance methods; any new capabilities must be added as instance methods instead.
    // ============================================================================================

    /**
     * Expands a string using grit-style `$1`, `$2`, ... placeholders, which allow the same string
     * to be shared with other platforms, by substituting the supplied arguments in order.
     *
     * @param template The string containing grit-style placeholders.
     * @param args The values substituted for the placeholders.
     * @return The expanded string, or the template itself if no arguments are supplied.
     */
    public static String expandString(String template, CharSequence... args) {
        return formatStringArgs(convertGritPlaceholders(template), args);
    }

    /** Converts grit-style `$1`, `$2`, ... placeholders to Java string specifiers `%1$s`, .... */
    /* package */ static String convertGritPlaceholders(String template) {
        return GRIT_PLACEHOLDER.matcher(template).replaceAll("%$1\\$s");
    }

    private static String formatStringArgs(String javaTemplate, CharSequence... args) {
        if (args.length == 0) return javaTemplate;
        // Templates only ever contain string specifiers (`%1$s`), which are not affected by the
        // locale; Locale.ROOT is passed to keep the formatting explicit.
        return String.format(Locale.ROOT, javaTemplate, (Object[]) args);
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

    /** Returns the background color for suggestions in the given color scheme and context. */
    public static @ColorInt int getStandardSuggestionBackgroundColor(
            Context context, @BrandedColorScheme int colorScheme) {
        return colorScheme == BrandedColorScheme.INCOGNITO
                ? context.getColor(R.color.search_suggestion_bg_color_incognito)
                : ContextCompat.getColor(context, R.color.search_suggestion_bg_color);
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
     * Returns the favicon for the given tab, or null if unavailable.
     *
     * @param tab The tab to retrieve the favicon for.
     */
    public @Nullable Bitmap getTabFavicon(Tab tab) {
        if (mTabFaviconFactory == null) return null;
        return mTabFaviconFactory.apply(tab);
    }

    public static void setUrlBarPrimaryTextColorForTesting(@ColorInt int value) {
        sUrlBarPrimaryTextColorForTesting = value;
        ResettersForTesting.register(() -> sUrlBarPrimaryTextColorForTesting = null);
    }

    public static void setUrlBarHintTextColorForTesting(@ColorInt int value) {
        sUrlBarHintTextColorForTesting = value;
        ResettersForTesting.register(() -> sUrlBarHintTextColorForTesting = null);
    }

    ResourceCache getCacheForTesting() {
        return mCache;
    }

    SparseArray<ConstantState> getDrawableCacheForTesting() {
        return mCache.getDrawableCacheForTesting();
    }
}
