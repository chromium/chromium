// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import android.animation.ObjectAnimator;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.DrawableRes;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
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
    private final Resources mResources;
    private final Callback<String> mShowToastCallback;
    private final ThemeColorProvider mThemeColorProvider;
    private boolean mIsShiftDownForReload;
    private boolean mIsReloading;

    /**
     * Create an instance of {@link ReloadButtonMediator}.
     *
     * @param model a properties model that encapsulates reload button state.
     * @param delegate a callback to stop or reload current tab
     */
    ReloadButtonMediator(
            PropertyModel model,
            ReloadButtonCoordinator.Delegate delegate,
            ThemeColorProvider themeColorProvider,
            Callback<String> showToast,
            Resources resources) {
        mModel = model;
        mResources = resources;
        mShowToastCallback = showToast;
        mThemeColorProvider = themeColorProvider;

        Callback<MotionEvent> onTouchListener =
                (event) ->
                        mIsShiftDownForReload =
                                (event.getMetaState() & KeyEvent.META_SHIFT_ON) != 0;
        mModel.set(ReloadButtonProperties.TOUCH_LISTENER, onTouchListener);
        mModel.set(
                ReloadButtonProperties.CLICK_LISTENER,
                () -> delegate.stopOrReloadCurrentTab(mIsShiftDownForReload));
        mModel.set(ReloadButtonProperties.LONG_CLICK_LISTENER, this::showActionToastOnReloadButton);

        updateBackgroundHighlight(mThemeColorProvider.getBrandedColorScheme());
        mThemeColorProvider.addTintObserver(this);
    }

    private void showActionToastOnReloadButton() {
        if (mIsReloading) {
            mShowToastCallback.onResult(mResources.getString(R.string.menu_stop_refresh));
        } else {
            mShowToastCallback.onResult(mResources.getString(R.string.refresh));
        }
    }

    @Override
    public void onTintChanged(
            ColorStateList tint,
            ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        mModel.set(ReloadButtonProperties.TINT_LIST, activityFocusTint);
        updateBackgroundHighlight(brandedColorScheme);
    }

    private void updateBackgroundHighlight(@BrandedColorScheme int brandedColorScheme) {
        final @DrawableRes int backgroundRes =
                brandedColorScheme == BrandedColorScheme.INCOGNITO
                        ? R.drawable.default_icon_background_baseline
                        : R.drawable.default_icon_background;
        mModel.set(ReloadButtonProperties.BACKGROUND_HIGHLIGHT_RESOURCE, backgroundRes);
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

    /**
     * Changes reload button state to reload or stopped.
     *
     * @param isReloading indicates whether current page is reloading.
     */
    public void setReloading(boolean isReloading) {
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
     * Changes reload button enabled state.
     *
     * @param isEnabled indicates whether the button should be enabled or disabled.
     */
    public void setEnabled(boolean isEnabled) {
        mModel.set(ReloadButtonProperties.IS_ENABLED, isEnabled);
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
     * Sets a listeners that allows parent to intercept key events.
     *
     * @param listener a callback that is invoked when hardware key is pressed.
     */
    public void setOnKeyListener(View.OnKeyListener listener) {
        mModel.set(ReloadButtonProperties.KEY_LISTENER, listener);
    }

    public void destroy() {
        mModel.set(ReloadButtonProperties.TOUCH_LISTENER, null);
        mModel.set(ReloadButtonProperties.CLICK_LISTENER, null);
        mModel.set(ReloadButtonProperties.LONG_CLICK_LISTENER, null);
        mModel.set(ReloadButtonProperties.KEY_LISTENER, null);

        mThemeColorProvider.removeTintObserver(this);
    }
}
