// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

import java.util.function.Supplier;

/** A subclass of TopUiThemeColorProvider which allows for adjustments to the tint color. */
@NullMarked
public class AdjustedTopUiThemeColorProvider extends TopUiThemeColorProvider {
    private final NullableObservableSupplier<Tab> mTabSupplier;
    private final NtpThemeStateProvider.Observer mThemeStateObserver;

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
    public AdjustedTopUiThemeColorProvider(
            Context context,
            NullableObservableSupplier<Tab> tabSupplier,
            Supplier<Integer> activityThemeColorSupplier,
            boolean isTablet,
            boolean allowThemingInNightMode,
            boolean allowBrightThemeColors,
            boolean allowThemingOnTablets) {
        super(
                context,
                tabSupplier,
                activityThemeColorSupplier,
                isTablet,
                allowThemingInNightMode,
                allowBrightThemeColors,
                allowThemingOnTablets);
        mTabSupplier = tabSupplier;

        mThemeStateObserver =
                new NtpThemeStateProvider.Observer() {
                    @Override
                    public void applyThemeChanges() {}

                    @Override
                    public void onCustomBackgroundChanged() {
                        onBackgroundChangedImpl();
                    }
                };
        NtpThemeStateProvider.getInstance().addObserver(mThemeStateObserver);
    }

    /** Updates tint colors of the given tab. */
    @VisibleForTesting
    @Override
    protected void updateColor(Tab tab, @ColorInt int themeColor, boolean shouldAnimate) {
        if (!tab.isNativePage() || !assumeNonNull(tab.getNativePage()).useLightIconTint()) {
            super.updateColor(tab, themeColor, shouldAnimate);
            return;
        }

        // Only light tint color is used if adjustTintColor is true.
        ColorStateList iconTint =
                AppCompatResources.getColorStateList(
                        mContext, R.color.default_icon_color_white_tint_list);
        super.updateTint(iconTint, iconTint, BrandedColorScheme.DARK_BRANDED_THEME);
    }

    @Override
    public void destroy() {
        super.destroy();
        NtpThemeStateProvider.getInstance().removeObserver(mThemeStateObserver);
    }

    private void onBackgroundChangedImpl() {
        Tab tab = mTabSupplier.get();
        if (tab == null) {
            return;
        }

        updateColor(tab, tab.getThemeColor(), /* shouldAnimate= */ false);
    }
}
