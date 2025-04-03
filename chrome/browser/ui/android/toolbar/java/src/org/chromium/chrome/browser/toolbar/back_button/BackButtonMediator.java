// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import android.content.res.ColorStateList;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
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
    private boolean mIsTabSwitcherMode;

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

        mModel.set(
                BackButtonProperties.CLICK_LISTENER,
                () -> {
                    onBackPressed.run();
                    updateButtonEnabledState();
                });
        mModel.set(
                BackButtonProperties.LONG_CLICK_LISTENER,
                () -> {
                    if (mCurrentTab == null) return;
                    showNavigationPopup.onResult(mCurrentTab);
                });

        updateBackgroundHighlight(mThemeColorProvider.getBrandedColorScheme());
        mThemeColorProvider.addTintObserver(this);

        // From web_contents_impl.cc and browser.cc back button's enabled state is updated based on
        // the InvalidateType.{TAB, LOAD, and URL} flags that are mapped to the callbacks below.
        mTabObserver =
                new TabSupplierObserver(tabSupplier, /* shouldTrigger= */ true) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab) {
                        // ActivityTabProvider returns null for non-interactive tabs, e.g. tab
                        // switcher, and we actually want to keep the most recent tab.
                        // Skipping null to keep recent.
                        if (tab == null) return;
                        mCurrentTab = tab;

                        updateButtonEnabledState();
                    }

                    @Override
                    public void onNavigationEntriesDeleted(Tab tab) {
                        // This callback is invoked when history is deleted, no entries = no back.
                        updateButtonEnabledState();
                    }

                    @Override
                    public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                        updateButtonEnabledState();
                    }

                    @Override
                    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                        updateButtonEnabledState();
                    }

                    @Override
                    public void onUrlUpdated(Tab tab) {
                        // Some updates such as making a navigation entry unskippable can change
                        // canGoBack() result. Such updates are delivered here and we want to handle
                        // them to update our state, see https://crbug.com/1477784.
                        updateButtonEnabledState();
                    }
                };
    }

    private void updateButtonEnabledState() {
        final boolean canGoBack =
                mCurrentTab != null && mCurrentTab.canGoBack() && !mIsTabSwitcherMode;

        mModel.set(BackButtonProperties.IS_ENABLED, canGoBack);
        mModel.set(BackButtonProperties.IS_FOCUSABLE, canGoBack);
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
     * Indicates that parent entered a tab switcher mode.
     *
     * @param isTabSwitcherMode whether tab switcher is showing or not.
     */
    public void setTabSwitcherMode(boolean isTabSwitcherMode) {
        mIsTabSwitcherMode = isTabSwitcherMode;
        updateButtonEnabledState();
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

    @VisibleForTesting
    TabObserver getTabObserver() {
        return mTabObserver;
    }
}
