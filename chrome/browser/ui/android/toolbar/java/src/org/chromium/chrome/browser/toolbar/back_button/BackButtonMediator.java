// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.InsetDrawable;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.Insets;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ToolbarResourceUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelAnimatorFactory;
import org.chromium.ui.util.ClickWithMetaStateCallback;

/**
 * A class responsible for mediating external events like theme changes or visibility changes from
 * external classes and reflects all these changes on back button model.
 */
@NullMarked
class BackButtonMediator implements ThemeColorProvider.TintObserver {

    private final PropertyModel mModel;
    private final Context mContext;
    private final Resources mResources;
    private final ThemeColorProvider mThemeColorProvider;
    private final TabSupplierObserver mTabObserver;
    private @Nullable Tab mCurrentTab;
    private final ObservableSupplier<Boolean> mEnabledSupplier;
    private final Callback<Boolean> mEnabledObserver;
    private Insets mInsets;
    private final boolean mIsWebApp;

    private @DrawableRes int mDrawableResForTesting;

    /**
     * Create an instance of {@link BackButtonMediator}.
     *
     * @param model a model that represents back button state.
     * @param onBackPressed a {@link Callback<Integer>} (taking a parameter of meta key state) that
     *     is invoked on back button click event. Allows parent components to intercept click and
     *     navigate back in the history or hide custom UI components.
     * @param themeColorProvider a provider that notifies about theme changes.
     * @param tabSupplier a supplier that provides current active tab.
     * @param showNavigationPopup a callback that displays history navigation popup.
     */
    public BackButtonMediator(
            PropertyModel model,
            ClickWithMetaStateCallback onBackPressed,
            ThemeColorProvider themeColorProvider,
            ObservableSupplier<@Nullable Tab> tabSupplier,
            ObservableSupplier<Boolean> enabledSupplier,
            Callback<Tab> showNavigationPopup,
            Resources resources,
            Context context,
            boolean isWebApp) {
        mModel = model;
        mThemeColorProvider = themeColorProvider;
        mResources = resources;
        mContext = context;
        mIsWebApp = isWebApp;

        mInsets = Insets.NONE;

        mModel.set(
                BackButtonProperties.CLICK_LISTENER,
                (metaState) -> {
                    onBackPressed.onClickWithMeta(metaState);
                    updateButtonEnabledState();
                });
        mModel.set(
                BackButtonProperties.LONG_CLICK_LISTENER,
                () -> {
                    if (mCurrentTab == null) return;
                    showNavigationPopup.onResult(mCurrentTab);
                });

        updateBackground(mThemeColorProvider.getBrandedColorScheme());
        mThemeColorProvider.addTintObserver(this);

        mEnabledSupplier = enabledSupplier;
        mEnabledObserver = (isEnabled) -> updateButtonEnabledState();
        mEnabledSupplier.addObserver(mEnabledObserver);

        // From web_contents_impl.cc and browser.cc back button's enabled state is updated based on
        // the InvalidateType.{TAB, LOAD, and URL} flags that are mapped to the callbacks below.
        mTabObserver =
                new TabSupplierObserver(tabSupplier, /* shouldTrigger= */ true) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
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
                mCurrentTab != null && mCurrentTab.canGoBack() && mEnabledSupplier.get();

        mModel.set(BackButtonProperties.IS_ENABLED, canGoBack);
        mModel.set(BackButtonProperties.IS_FOCUSABLE, canGoBack);
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        mModel.set(BackButtonProperties.TINT_COLOR_LIST, activityFocusTint);
        updateBackground(brandedColorScheme);
    }

    private void updateBackground(@BrandedColorScheme int brandedThemeColor) {
        final int backgroundRes =
                ToolbarResourceUtils.backgroundResForThemeColor(brandedThemeColor, mIsWebApp);
        mDrawableResForTesting = backgroundRes;
        var drawable =
                new InsetDrawable(
                        ResourcesCompat.getDrawable(mResources, backgroundRes, mContext.getTheme()),
                        mInsets.left,
                        mInsets.top,
                        mInsets.right,
                        mInsets.bottom);
        mModel.set(BackButtonProperties.BACKGROUND_HIGHLIGHT, drawable);
    }

    public @DrawableRes int getBackgroundResForTesting() {
        return mDrawableResForTesting;
    }

    /**
     * Prepares the view for fade animation and returns an alpha animator.
     *
     * @param shouldShow indicated fade in or out animation type
     * @return {@link ObjectAnimator} that animates view's alpha
     */
    public ObjectAnimator getFadeAnimator(boolean shouldShow) {
        mModel.set(BackButtonProperties.ALPHA, shouldShow ? 0f : 1f);
        return PropertyModelAnimatorFactory.ofFloat(
                mModel, BackButtonProperties.ALPHA, shouldShow ? 1f : 0f);
    }

    /**
     * Sets back button visibility.
     *
     * @param isVisible indicated whether view should be visible or gone.
     */
    public void setVisibility(boolean isVisible) {
        mModel.set(BackButtonProperties.IS_VISIBLE, isVisible);
    }

    /**
     * Checks whether view is focusable or not.
     *
     * @return true - view is focusable, false - view is not focusable.
     */
    public boolean isFocusable() {
        return mModel.get(BackButtonProperties.IS_FOCUSABLE);
    }

    /**
     * Checks whether view is visible or not.
     *
     * @return true - view is visible, false - view is not visible.
     */
    public boolean isVisible() {
        return mModel.get(BackButtonProperties.IS_VISIBLE);
    }

    /**
     * Sets a key event listener on the view.
     *
     * @param listener {@link View.OnKeyListener}
     */
    public void setOnKeyListener(View.OnKeyListener listener) {
        mModel.set(BackButtonProperties.KEY_LISTENER, listener);
    }

    public void setBackgroundInsets(Insets insets) {
        mInsets = insets;
        updateBackground(mThemeColorProvider.getBrandedColorScheme());
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
        mEnabledSupplier.removeObserver(mEnabledObserver);
    }

    @VisibleForTesting
    TabObserver getTabObserver() {
        return mTabObserver;
    }
}
