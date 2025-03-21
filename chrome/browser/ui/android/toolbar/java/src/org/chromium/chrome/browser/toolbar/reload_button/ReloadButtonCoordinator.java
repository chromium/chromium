// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import android.animation.ObjectAnimator;
import android.content.res.ColorStateList;
import android.view.View;
import android.widget.ImageButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.Toast;

/**
 * Root component for the reload button. Exposes public API to change button's state and allows
 * consumers to react to button state changes.
 */
@NullMarked
public class ReloadButtonCoordinator {
    /** An interface that allows parent components to control tab reload logic. */
    public interface Delegate {
        /**
         * Controls how tab is going to be reloaded.
         *
         * @param ignoreCache controls whether should force reload or not
         */
        void stopOrReloadCurrentTab(boolean ignoreCache);
    }

    private final ReloadButtonMediator mMediator;

    /**
     * Creates an instance of {@link ReloadButtonCoordinator}
     *
     * @param view reload button android view.
     * @param delegate that contains reload logic for reload button.
     */
    public ReloadButtonCoordinator(
            ImageButton view,
            ReloadButtonCoordinator.Delegate delegate,
            ThemeColorProvider themeColorProvider) {
        // ThemeColorProvider might not be updated by this time. Keep existing color list.
        final ColorStateList tint =
                themeColorProvider.getActivityFocusTint() == null
                        ? view.getImageTintList()
                        : themeColorProvider.getActivityFocusTint();
        final var model =
                new PropertyModel.Builder(ReloadButtonProperties.ALL_KEYS)
                        .with(ReloadButtonProperties.ALPHA, view.getAlpha())
                        .with(
                                ReloadButtonProperties.IS_VISIBLE,
                                view.getVisibility() == View.VISIBLE)
                        .with(
                                ReloadButtonProperties.CONTENT_DESCRIPTION,
                                view.getContentDescription())
                        .with(ReloadButtonProperties.TINT_LIST, tint)
                        .with(ReloadButtonProperties.DRAWABLE_LEVEL, view.getDrawable().getLevel())
                        .build();
        mMediator =
                new ReloadButtonMediator(
                        model,
                        delegate,
                        themeColorProvider,
                        (text) -> Toast.showAnchoredToast(view.getContext(), view, text),
                        view.getResources());
        PropertyModelChangeProcessor.create(model, view, ReloadButtonViewBinder::bind);
    }

    /**
     * Changes button reloading state.
     *
     * @param isReloading indicated whether current web page is reloading.
     */
    public void setReloading(boolean isReloading) {
        mMediator.setReloading(isReloading);
    }

    /**
     * Changes reload button enabled state.
     *
     * @param isEnabled indicates whether the button should be enabled or disabled.
     */
    public void setEnabled(boolean isEnabled) {
        mMediator.setEnabled(isEnabled);
    }

    /**
     * Sets reload button visibility.
     *
     * @param isVisible indicated whether view should be visible or gone.
     */
    public void setVisibility(boolean isVisible) {
        mMediator.setVisibility(isVisible);
    }

    /**
     * Sets a listeners that allows parent to intercept key events.
     *
     * @param listener a callback that is invoked when hardware key is pressed.
     */
    public void setOnKeyListener(View.OnKeyListener listener) {
        mMediator.setOnKeyListener(listener);
    }

    /**
     * Prepares the view for fade animation and returns an alpha animator.
     *
     * @param shouldShow indicated fade in or out animation type.
     * @return {@link ObjectAnimator} that animates view's alpha.
     */
    public ObjectAnimator getFadeAnimator(boolean shouldShow) {
        return mMediator.getFadeAnimator(shouldShow);
    }

    /** Destroys current object instance. It can't be used after this call. */
    public void destroy() {
        mMediator.destroy();
    }
}
