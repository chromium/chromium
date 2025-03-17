// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import android.animation.ObjectAnimator;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.KeyboardNavigationListener;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A Mediator for reload button. Glues reload button and external events to the model that relays
 * them back to the reload button state.
 */
@NullMarked
class ReloadButtonMediator implements ThemeColorProvider.TintObserver {

    private final PropertyModel mModel;
    private final Resources mResources;
    private final Callback<String> mShowToastCallback;
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
            Callback<String> showToast,
            Resources resources) {
        mModel = model;
        mResources = resources;
        mShowToastCallback = showToast;

        Callback<MotionEvent> onTouchListener =
                (event) ->
                        mIsShiftDownForReload =
                                (event.getMetaState() & KeyEvent.META_SHIFT_ON) != 0;
        mModel.set(ReloadButtonProperties.TOUCH_LISTENER, onTouchListener);
        mModel.set(
                ReloadButtonProperties.CLICK_LISTENER,
                () -> delegate.stopOrReloadCurrentTab(mIsShiftDownForReload));
        mModel.set(ReloadButtonProperties.LONG_CLICK_LISTENER, this::showActionToastOnReloadButton);
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
            ColorStateList tint, ColorStateList activityFocusTint, int brandedColorScheme) {}

    /**
     * Creates a show/hide animator that animates view's alpha.
     *
     * @param isShowing indicated fade in or out animation type
     * @return {@link ObjectAnimator} that animates view's alpha
     */
    // TODO(vkorotkevich): Remove @Nullable
    public @Nullable ObjectAnimator getFadeAnimator(boolean isShowing) {
        return null;
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
    public void setVisibility(boolean isVisible) {}

    /**
     * Sets a listeners that allows parent to intercept keyboard navigation events.
     *
     * @param listener {@link KeyboardNavigationListener}
     */
    public void setKeyboardNavigationListener(KeyboardNavigationListener listener) {}

    public void destroy() {
        mModel.set(ReloadButtonProperties.TOUCH_LISTENER, null);
        mModel.set(ReloadButtonProperties.CLICK_LISTENER, null);
        mModel.set(ReloadButtonProperties.LONG_CLICK_LISTENER, null);
    }
}
