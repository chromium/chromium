// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import android.content.res.ColorStateList;

import androidx.annotation.DrawableRes;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A class responsible for mediating external events like theme changes or visibility changes from
 * external classes and reflects all these changes on back button model.
 */
@NullMarked
class BackButtonMediator implements ThemeColorProvider.TintObserver {

    private final PropertyModel mModel;
    private final ThemeColorProvider mThemeColorProvider;
    private final TabSupplierObserver mTabObserver;
    private @Nullable Tab mCurrentTab;

    /**
     * Create an instance of {@link BackButtonMediator}.
     *
     * @param model a model that represents back button state.
     * @param onBackPressed a callback that is invoked on back button click event. Allows parent
     *     components to intercept click and navigate back in the history or hide custom UI
     *     components.
     * @param themeColorProvider a provider that notifies about theme changes.
     * @param tabSupplier a supplier that provides current active tab.
     * @param showNavigationPopup a callback that displays history navigation popup.
     */
    public BackButtonMediator(
            PropertyModel model,
            Runnable onBackPressed,
            ThemeColorProvider themeColorProvider,
            ObservableSupplier<Tab> tabSupplier,
            Callback<Tab> showNavigationPopup) {
        mModel = model;
        mThemeColorProvider = themeColorProvider;

        mModel.set(BackButtonProperties.CLICK_LISTENER, onBackPressed);
        mModel.set(
                BackButtonProperties.LONG_CLICK_LISTENER,
                () -> {
                    if (mCurrentTab == null) return;
                    showNavigationPopup.onResult(mCurrentTab);
                });

        updateBackgroundHighlight(mThemeColorProvider.getBrandedColorScheme());
        mThemeColorProvider.addTintObserver(this);

        mTabObserver =
                new TabSupplierObserver(tabSupplier, /* shouldTrigger= */ true) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab) {
                        mCurrentTab = tab;
                    }
                };
    }

    @Override
    public void onTintChanged(
            ColorStateList tint,
            ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        mModel.set(BackButtonProperties.TINT_COLOR_LIST, activityFocusTint);
        updateBackgroundHighlight(brandedColorScheme);
    }

    private void updateBackgroundHighlight(@BrandedColorScheme int brandedThemeColor) {
        final @DrawableRes int backgroundRes =
                brandedThemeColor == BrandedColorScheme.INCOGNITO
                        ? R.drawable.default_icon_background_baseline
                        : R.drawable.default_icon_background;
        mModel.set(BackButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE, backgroundRes);
    }

    /**
     * Cleans up mediator resources and unsubscribes from external events. An instance can't be used
     * after this method is called.
     */
    public void destroy() {
        mModel.set(BackButtonProperties.CLICK_LISTENER, null);
        mModel.set(BackButtonProperties.LONG_CLICK_LISTENER, null);
        mThemeColorProvider.removeTintObserver(this);
        mTabObserver.destroy();
    }
}
