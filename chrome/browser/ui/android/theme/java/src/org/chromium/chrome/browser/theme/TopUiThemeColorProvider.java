// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;

import java.util.function.Supplier;

/**
 * Manages the theme color used on the top part of the UI based on Tab's theme color and other
 * conditions such as dark mode settings, incognito mode, security state, etc.
 *
 * <p>The theme color is only updated when the supplied tab is non-null.
 */
@NullMarked
public class TopUiThemeColorProvider extends ThemeColorProvider {
    protected final Context mContext;
    private final Supplier<Integer> mActivityThemeColorSupplier;
    private final boolean mIsTablet;

    /** Whether the theme should apply while in dark mode. */
    private final boolean mAllowThemingInNightMode;

    /** Whether bright theme colors are allowed. */
    private final boolean mAllowBrightThemeColors;

    /** Whether tab theming is allowed on large screens */
    private final boolean mAllowThemingOnTablets;

    protected CurrentTabObserver mTabObserver;

    /** Whether or not the default color is used. */
    private boolean mIsDefaultColorUsed;

    /**
     * @param context {@link Context} to access the theme and the resources.
     * @param tabSupplier Supplier of the current tab.
     * @param activityThemeColorSupplier Supplier of activity theme color.
     * @param isTablet Whether the current activity is being run on a tablet.
     * @param allowThemingInNightMode Whether the tab theme should be used when the device is in
     *     night mode.
     * @param allowBrightThemeColors Whether the tab allows bright theme colors.
     * @param allowThemingOnTablets Whether the tab them should be used on large form-factors.
     */
    public TopUiThemeColorProvider(
            Context context,
            NullableObservableSupplier<Tab> tabSupplier,
            Supplier<Integer> activityThemeColorSupplier,
            boolean isTablet,
            boolean allowThemingInNightMode,
            boolean allowBrightThemeColors,
            boolean allowThemingOnTablets) {
        super(context);
        mContext = context;
        mTabObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onDidChangeThemeColor(Tab tab, @ColorInt int themeColor) {
                                updateColor(tab, themeColor, true);
                            }

                            @Override
                            public void onContentChanged(Tab tab) {
                                if (tab != null) {
                                    updateColor(tab, tab.getThemeColor(), false);
                                }
                            }
                        },
                        (tab) -> {
                            if (tab != null) updateColor(tab, tab.getThemeColor(), false);
                        });
        mActivityThemeColorSupplier = activityThemeColorSupplier;
        mIsTablet = isTablet;
        mAllowThemingInNightMode = allowThemingInNightMode;
        mAllowBrightThemeColors = allowBrightThemeColors;
        mAllowThemingOnTablets = allowThemingOnTablets;
    }

    @Override
    public void destroy() {
        super.destroy();
        mTabObserver.destroy();
    }

    /**
     * @param tab The {@link Tab} on which the theme color is used.
     * @param fallbackColor The fallback color to use if the default color is used or there is no
     *     current tab.
     * @return Theme color or the given fallback color if the default color is used or there is no
     *     current tab.
     */
    public @ColorInt int getThemeColorOrFallback(@Nullable Tab tab, @ColorInt int fallbackColor) {
        return (tab == null || mIsDefaultColorUsed) ? fallbackColor : getThemeColor();
    }

    /**
     * @param tab The {@link Tab} on which the toolbar background color is used.
     * @return Returns the toolbar background color.
     */
    public @ColorInt int getToolbarBackgroundColor(Tab tab) {
        NativePage nativePage = tab.getNativePage();
        @ColorInt int defaultColor = calculateColor(tab, tab.getThemeColor());
        return nativePage != null
                ? nativePage.getToolbarSceneLayerBackground(defaultColor)
                : defaultColor;
    }

    /**
     * Calculate theme color to be used for a given tab.
     *
     * @param tab Tab to get the theme color for.
     * @param themeColor Initial color to calculate the theme color with.
     * @return Final theme color for a given tab, with other signals taken into account.
     */
    protected @ColorInt int calculateColor(Tab tab, @ColorInt int themeColor) {
        // This method is used not only for the current tab but also for
        // any given tab. Therefore it should not alter any class state.
        if (!isUsingTabThemeColor(tab, themeColor)) {
            themeColor = ChromeColors.getDefaultThemeColor(mContext, tab.isIncognito());
            if (isThemingAllowed(tab)) {
                @ColorInt int customThemeColor = mActivityThemeColorSupplier.get();
                if (customThemeColor != TabState.UNSPECIFIED_THEME_COLOR) {
                    themeColor = customThemeColor;
                }
            }
        }

        // Ensure there is no alpha component to the theme color as that is not supported in the
        // dependent UI.
        return ColorUtils.getOpaqueColor(themeColor);
    }

    protected void updateColor(Tab tab, @ColorInt int themeColor, boolean shouldAnimate) {
        updatePrimaryColor(calculateColor(tab, themeColor), shouldAnimate);
        mIsDefaultColorUsed = isUsingDefaultColor(tab, themeColor);
        final @BrandedColorScheme int brandedColorScheme =
                calculateBrandedColorScheme(tab.isIncognito(), mIsDefaultColorUsed);
        final ColorStateList iconTint =
                ThemeUtils.getThemedToolbarIconTint(mContext, brandedColorScheme);
        updateTint(iconTint, iconTint, brandedColorScheme);
    }

    private boolean isUsingDefaultColor(Tab tab, @ColorInt int themeColor) {
        // This method is used not only for the current tab but also for
        // any given tab. Therefore it should not alter any class state.
        return !(isUsingTabThemeColor(tab, themeColor)
                || (isThemingAllowed(tab)
                        && mActivityThemeColorSupplier.get() != TabState.UNSPECIFIED_THEME_COLOR));
    }

    /**
     * @param tab Tab to get the theme color for.
     * @param themeColor Initial color to calculate the theme color with.
     * @return Whether the given tab is using the tab theme color.
     */
    private boolean isUsingTabThemeColor(Tab tab, @ColorInt int themeColor) {
        return isThemingAllowed(tab)
                && themeColor != TabState.UNSPECIFIED_THEME_COLOR
                && (mAllowBrightThemeColors || !ColorUtils.isThemeColorTooBright(themeColor));
    }

    /**
     * Returns whether theming the activity is allowed (either by the web contents or by the
     * activity).
     */
    private boolean isThemingAllowed(Tab tab) {
        boolean disallowDueToNightMode =
                !mAllowThemingInNightMode && ColorUtils.inNightMode(tab.getContext());
        final boolean isEligibleFormFactor = mAllowThemingOnTablets || !mIsTablet;

        return tab.isThemingAllowed()
                && isEligibleFormFactor
                && !disallowDueToNightMode
                && !tab.isNativePage()
                && !tab.isIncognito();
    }

    private @BrandedColorScheme int calculateBrandedColorScheme(
            boolean isIncognito, boolean isDefaultColor) {
        if (isIncognito) return BrandedColorScheme.INCOGNITO;
        if (isDefaultColor) return BrandedColorScheme.APP_DEFAULT;

        final boolean isDarkTheme =
                ColorUtils.shouldUseLightForegroundOnBackground(getThemeColor());
        return isDarkTheme
                ? BrandedColorScheme.DARK_BRANDED_THEME
                : BrandedColorScheme.LIGHT_BRANDED_THEME;
    }
}
