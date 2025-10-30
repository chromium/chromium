// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.InsetDrawable;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.Insets;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarResourceUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelAnimatorFactory;

/**
 * A Mediator for reload button. Glues reload button and external events to the model that relays
 * them back to the reload button state.
 */
@NullMarked
class ReloadButtonMediator implements ThemeColorProvider.TintObserver {

    private final PropertyModel mModel;
    private final Context mContext;
    private final Resources mResources;
    private final Callback<String> mShowToastCallback;
    private final ThemeColorProvider mThemeColorProvider;
    private final TabSupplierObserver mTabObserver;
    private final ObservableSupplier<Boolean> mNtpLoadingSupplier;
    private final Callback<Boolean> mNtpLoadingObserver;
    private final ObservableSupplier<Boolean> mEnabledSupplier;
    private final Callback<Boolean> mEnabledObserver;
    private boolean mIsShiftDownForReload;
    private boolean mIsReloading;
    private @Nullable Tab mCurrentTab;
    private Insets mInsets;
    private final boolean mIsWebApp;

    private @DrawableRes int mBackgroundResForTesting;

    /**
     * Create an instance of {@link ReloadButtonMediator}.
     *
     * @param model a properties model that encapsulates reload button state.
     * @param delegate a callback to stop or reload current tab.
     * @param themeColorProvider a provider that notifies of tint changes.
     * @param tabSupplier a supplier that notifies of tab changes.
     * @param ntpLoadingSupplier a supplier that provides loading state of content inside NTP, e.g.
     *     feed, this is not a reload state of the whole tab.
     * @param showToast a callback that allows to show toast anchored to the current view.
     * @param resources Android resources.
     */
    ReloadButtonMediator(
            PropertyModel model,
            ReloadButtonCoordinator.Delegate delegate,
            ThemeColorProvider themeColorProvider,
            ObservableSupplier<@Nullable Tab> tabSupplier,
            ObservableSupplier<Boolean> ntpLoadingSupplier,
            ObservableSupplier<Boolean> enabledSupplier,
            Callback<String> showToast,
            Resources resources,
            Context context,
            boolean isWebApp) {
        mModel = model;
        mResources = resources;
        mShowToastCallback = showToast;
        mThemeColorProvider = themeColorProvider;
        mNtpLoadingSupplier = ntpLoadingSupplier;
        mEnabledSupplier = enabledSupplier;
        mContext = context;
        mIsWebApp = isWebApp;

        mInsets = Insets.NONE;

        Callback<MotionEvent> onTouchListener =
                (event) ->
                        mIsShiftDownForReload =
                                (event.getMetaState() & KeyEvent.META_SHIFT_ON) != 0;
        mModel.set(ReloadButtonProperties.TOUCH_LISTENER, onTouchListener);
        mModel.set(
                ReloadButtonProperties.CLICK_LISTENER,
                () -> delegate.stopOrReloadCurrentTab(mIsShiftDownForReload));
        mModel.set(ReloadButtonProperties.LONG_CLICK_LISTENER, this::showActionToastOnReloadButton);

        updateBackground(mThemeColorProvider.getBrandedColorScheme());

        mNtpLoadingObserver =
                (isLoading) -> {
                    if (mCurrentTab != null && mCurrentTab.isNativePage()) {
                        setReloading(isLoading);
                    }
                };
        mNtpLoadingSupplier.addObserver(mNtpLoadingObserver);

        mEnabledObserver = (isEnabled) -> mModel.set(ReloadButtonProperties.IS_ENABLED, isEnabled);
        mEnabledSupplier.addObserver(mEnabledObserver);

        mTabObserver =
                new TabSupplierObserver(tabSupplier, /* shouldTrigger= */ true) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        mCurrentTab = tab;
                        updateReloadingState(tab);
                    }

                    @Override
                    public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                        if (!toDifferentDocument) return;
                        updateReloadingState(tab);
                    }

                    @Override
                    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                        if (!toDifferentDocument) return;
                        updateReloadingState(tab);
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        updateReloadingState(tab);
                    }
                };
    }

    private void showActionToastOnReloadButton() {
        if (mIsReloading) {
            mShowToastCallback.onResult(mResources.getString(R.string.menu_stop_refresh));
        } else {
            mShowToastCallback.onResult(mResources.getString(R.string.refresh));
        }
    }

    private void updateReloadingState(@Nullable Tab tab) {
        final boolean isReloading = tab != null && !SadTab.isShowing(tab) && tab.isLoading();
        setReloading(isReloading);
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        mModel.set(ReloadButtonProperties.TINT_LIST, activityFocusTint);
        updateBackground(brandedColorScheme);
    }

    private void updateBackground(@BrandedColorScheme int brandedColorScheme) {
        final int backgroundRes =
                ToolbarResourceUtils.backgroundResForThemeColor(brandedColorScheme, mIsWebApp);
        InsetDrawable drawable =
                new InsetDrawable(
                        ResourcesCompat.getDrawable(mResources, backgroundRes, mContext.getTheme()),
                        mInsets.left,
                        mInsets.top,
                        mInsets.right,
                        mInsets.bottom);
        mBackgroundResForTesting = backgroundRes;
        mModel.set(ReloadButtonProperties.BACKGROUND_HIGHLIGHT, drawable);

        // When setting the background of a view to an `InsetDrawable`, the padding of the view
        // is automatically set to the insets of the `InsetDrawable`. However, a bug prevents the
        // padding from being set if the insets are all 0. The workaround is to set the padding
        // explicitly.
        // https://crbug.com/442688217
        mModel.set(ReloadButtonProperties.PADDING, mInsets);
    }

    public @DrawableRes int getBackgroundResForTesting() {
        return mBackgroundResForTesting;
    }

    /**
     * Prepares the view for fade animation and returns an alpha animator.
     *
     * @param shouldShow indicated fade in or out animation type
     * @return {@link ObjectAnimator} that animates view's alpha
     */
    public ObjectAnimator getFadeAnimator(boolean shouldShow) {
        mModel.set(ReloadButtonProperties.ALPHA, shouldShow ? 0f : 1f);
        return PropertyModelAnimatorFactory.ofFloat(
                mModel, ReloadButtonProperties.ALPHA, shouldShow ? 1f : 0f);
    }

    private void setReloading(boolean isReloading) {
        mIsReloading = isReloading;

        final int level;
        final String contentDescription;
        if (isReloading) {
            level = mResources.getInteger(R.integer.reload_button_level_stop);
            contentDescription = mResources.getString(R.string.accessibility_btn_stop_loading);
        } else {
            level = mResources.getInteger(R.integer.reload_button_level_reload);
            contentDescription = mResources.getString(R.string.accessibility_btn_refresh);
        }

        mModel.set(ReloadButtonProperties.DRAWABLE_LEVEL, level);
        mModel.set(ReloadButtonProperties.CONTENT_DESCRIPTION, contentDescription);
    }

    /**
     * Sets reload button visibility.
     *
     * @param isVisible indicated whether view should be visible or gone.
     */
    public void setVisibility(boolean isVisible) {
        mModel.set(ReloadButtonProperties.IS_VISIBLE, isVisible);
    }

    /**
     * Informs the button on whether there is enough space for it to be shown.
     *
     * @param hasSpaceToShow indicates whether the button view has space to show.
     */
    void setHasSpaceToShow(boolean hasSpaceToShow) {
        mModel.set(ReloadButtonProperties.HAS_SPACE_TO_SHOW, hasSpaceToShow);
    }

    /**
     * Checks whether view is visible or not.
     *
     * @return true - view is visible, false - view is not visible.
     */
    public boolean isVisible() {
        return mModel.get(ReloadButtonProperties.IS_VISIBLE);
    }

    /**
     * Sets a listeners that allows parent to intercept key events.
     *
     * @param listener a callback that is invoked when hardware key is pressed.
     */
    public void setOnKeyListener(View.OnKeyListener listener) {
        mModel.set(ReloadButtonProperties.KEY_LISTENER, listener);
    }

    public void setBackgroundInsets(Insets insets) {
        mInsets = insets;
        updateBackground(mThemeColorProvider.getBrandedColorScheme());
    }

    public void destroy() {
        mModel.set(ReloadButtonProperties.TOUCH_LISTENER, null);
        mModel.set(ReloadButtonProperties.CLICK_LISTENER, null);
        mModel.set(ReloadButtonProperties.LONG_CLICK_LISTENER, null);
        mModel.set(ReloadButtonProperties.KEY_LISTENER, null);

        mNtpLoadingSupplier.removeObserver(mNtpLoadingObserver);
        mEnabledSupplier.removeObserver(mEnabledObserver);
        mTabObserver.destroy();
    }
}
