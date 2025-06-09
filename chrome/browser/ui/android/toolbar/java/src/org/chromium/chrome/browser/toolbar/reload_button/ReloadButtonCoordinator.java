// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import android.animation.ObjectAnimator;
import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.view.View;
import android.widget.ImageButton;

import androidx.core.graphics.Insets;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.Toast;

/**
 * Root component for the reload button that maintains UI representations like stop/loading states.
 * Exposes public API to change button's state and allows consumers to react to button state
 * changes.
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
    private final ImageButton mView;

    /**
     * Creates an instance of {@link ReloadButtonCoordinator}
     *
     * @param view reload button android view.
     * @param delegate that contains reload logic for reload button.
     * @param tabSupplier a supplier that provides current active tab.
     * @param ntpLoadingSupplier a supplier that provides loading state of content inside NTP, e.g
     *     feed, this is not a reload state of the whole tab.
     * @param themeColorProvider a provider that notifies about theme changes and focus tint.
     */
    public ReloadButtonCoordinator(
            ImageButton view,
            ReloadButtonCoordinator.Delegate delegate,
            ObservableSupplier<@Nullable Tab> tabSupplier,
            ObservableSupplier<Boolean> ntpLoadingSupplier,
            ObservableSupplier<Boolean> enabledSupplier,
            ThemeColorProvider themeColorProvider,
            boolean isWebApp) {
        mView = view;

        // ThemeColorProvider might not be updated by this time. Keep existing color list.
        final ColorStateList tint =
                themeColorProvider.getActivityFocusTint() == null
                        ? mView.getImageTintList()
                        : themeColorProvider.getActivityFocusTint();
        final var model =
                new PropertyModel.Builder(ReloadButtonProperties.ALL_KEYS)
                        .with(ReloadButtonProperties.ALPHA, mView.getAlpha())
                        .with(
                                ReloadButtonProperties.IS_VISIBLE,
                                mView.getVisibility() == View.VISIBLE)
                        .with(
                                ReloadButtonProperties.CONTENT_DESCRIPTION,
                                mView.getContentDescription())
                        .with(ReloadButtonProperties.TINT_LIST, tint)
                        .with(ReloadButtonProperties.DRAWABLE_LEVEL, mView.getDrawable().getLevel())
                        .build();
        mMediator =
                new ReloadButtonMediator(
                        model,
                        delegate,
                        themeColorProvider,
                        tabSupplier,
                        ntpLoadingSupplier,
                        enabledSupplier,
                        (text) -> Toast.showAnchoredToast(mView.getContext(), mView, text),
                        mView.getResources(),
                        mView.getContext(),
                        isWebApp);
        PropertyModelChangeProcessor.create(model, mView, ReloadButtonViewBinder::bind);
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

    public void setBackgroundInsets(Insets insets) {
        mMediator.setBackgroundInsets(insets);
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

    /**
     * Gets an area of the button that are touchable/clickable.
     *
     * @return a {@link Rect} that contains touchable/clickable area.
     */
    public Rect getHitRect() {
        final var rect = new Rect();
        mView.getHitRect(rect);
        return rect;
    }

    /**
     * Gets visibility.
     *
     * @return a Boolean indicating whether view is visible or not.
     */
    public boolean isVisibile() {
        return mMediator.isVisible();
    }

    /** Destroys current object instance. It can't be used after this call. */
    public void destroy() {
        mMediator.destroy();
    }
}
