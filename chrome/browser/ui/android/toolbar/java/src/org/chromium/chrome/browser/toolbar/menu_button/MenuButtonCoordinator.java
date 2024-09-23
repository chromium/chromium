// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import static android.view.View.LAYOUT_DIRECTION_RTL;

import android.animation.Animator;
import android.app.Activity;
import android.graphics.Canvas;
import android.os.Build;
import android.os.Build.VERSION;
import android.view.View;
import android.view.View.OnKeyListener;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.TooltipCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonProperties.ShowBadgeProperty;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonProperties.ThemeProperty;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Root component for the app menu button on the toolbar. Owns the MenuButton view and handles
 * changes to its visual state, e.g. showing/hiding the app update badge.
 */
public class MenuButtonCoordinator {
    public interface SetFocusFunction {
        void setFocus(boolean focus, int reason);
    }

    private final Activity mActivity;
    private final PropertyModel mPropertyModel;
    private MenuButtonMediator mMediator;
    private AppMenuButtonHelper mAppMenuButtonHelper;
    private MenuButton mMenuButton;
    private PropertyModelChangeProcessor mChangeProcessor;

    /**
     * @param appMenuCoordinatorSupplier Supplier for the AppMenuCoordinator, which owns all other
     *     app menu MVC components.
     * @param controlsVisibilityDelegate Delegate for forcing persistent display of browser
     *     controls.
     * @param windowAndroid The WindowAndroid instance.
     * @param setUrlBarFocusFunction Function that allows setting focus on the url bar.
     * @param requestRenderRunnable Runnable that requests a re-rendering of the compositor view
     *     containing the app menu button.
     * @param canShowAppUpdateBadge Whether the app menu update badge can be shown if there is a
     *     pending update.
     * @param isInOverviewModeSupplier Supplier of overview mode state.
     * @param themeColorProvider Provider of theme color changes.
     * @param menuButtonStateSupplier Supplier of the menu button state.
     * @param onMenuButtonClicked Runnable to run on menu button click.
     * @param menuButtonId Resource id that should be used to locate the underlying view.
     */
    public MenuButtonCoordinator(
            OneshotSupplier<AppMenuCoordinator> appMenuCoordinatorSupplier,
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate,
            WindowAndroid windowAndroid,
            SetFocusFunction setUrlBarFocusFunction,
            Runnable requestRenderRunnable,
            boolean canShowAppUpdateBadge,
            Supplier<Boolean> isInOverviewModeSupplier,
            ThemeColorProvider themeColorProvider,
            Supplier<MenuButtonState> menuButtonStateSupplier,
            Runnable onMenuButtonClicked,
            @IdRes int menuButtonId) {
        mActivity = windowAndroid.getActivity().get();
        mMenuButton = mActivity.findViewById(menuButtonId);
        mPropertyModel =
                new PropertyModel.Builder(MenuButtonProperties.ALL_KEYS)
                        .with(
                                MenuButtonProperties.SHOW_UPDATE_BADGE,
                                new ShowBadgeProperty(false, false))
                        .with(
                                MenuButtonProperties.THEME,
                                new ThemeProperty(
                                        themeColorProvider.getTint(),
                                        themeColorProvider.getBrandedColorScheme()))
                        .with(MenuButtonProperties.IS_VISIBLE, true)
                        .with(MenuButtonProperties.STATE_SUPPLIER, menuButtonStateSupplier)
                        .build();
        mMediator =
                new MenuButtonMediator(
                        mPropertyModel,
                        canShowAppUpdateBadge,
                        () -> mActivity.isFinishing() || mActivity.isDestroyed(),
                        requestRenderRunnable,
                        themeColorProvider,
                        isInOverviewModeSupplier,
                        controlsVisibilityDelegate,
                        setUrlBarFocusFunction,
                        appMenuCoordinatorSupplier,
                        windowAndroid,
                        menuButtonStateSupplier,
                        onMenuButtonClicked);
        mMediator
                .getMenuButtonHelperSupplier()
                .addObserver((helper) -> mAppMenuButtonHelper = helper);
        if (mMenuButton != null) {
            mChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mPropertyModel, mMenuButton, new MenuButtonViewBinder());

            // Set tooltip text for menu button.
            if (VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                TooltipCompat.setTooltipText(
                        mMenuButton,
                        mActivity
                                .getResources()
                                .getString(R.string.accessibility_toolbar_btn_menu));
            }
        }
    }

    /**
     * Update the state of AppMenu components that need to know if the current page is loading, e.g.
     * the stop/reload button.
     * @param isLoading Whether the current page is loading.
     */
    public void updateReloadingState(boolean isLoading) {
        if (mMediator == null) return;
        mMediator.updateReloadingState(isLoading);
    }

    /** Disables the menu button, removing it from the view hierarchy and destroying it. */
    public void disableMenuButton() {
        if (mMenuButton != null) {
            UiUtils.removeViewFromParent(mMenuButton);
            destroy();
        }
    }

    /**
     * Set the underlying MenuButton view. Use only if the MenuButton instance isn't available at
     * construction time, e.g. if it's lazily inflated. This should only be called once, unless
     * switching the active toolbar.
     * @param menuButton The underlying MenuButton view.
     */
    public void setMenuButton(MenuButton menuButton) {
        assert menuButton != null;
        mMenuButton = menuButton;

        if (mChangeProcessor != null) {
            mChangeProcessor.destroy();
        }
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, menuButton, new MenuButtonViewBinder());
    }

    /**
     * Handle the key press event on the menu button.
     * @return Whether the app menu was shown as a result of this action.
     */
    public boolean onEnterKeyPress() {
        if (mAppMenuButtonHelper == null || mMenuButton == null) return false;
        return mAppMenuButtonHelper.onEnterKeyPress(mMenuButton.getImageButton());
    }

    /**
     * @return Whether the menu button is present and visible.
     */
    public boolean isVisible() {
        return mMenuButton != null && mMenuButton.getVisibility() == View.VISIBLE;
    }

    /**
     * Get the underlying MenuButton view. Present for legacy reasons only; don't add new usages.
     */
    @Deprecated
    public MenuButton getMenuButton() {
        return mMenuButton;
    }

    /**
     * @param isClickable Whether the underlying MenuButton view should be clickable.
     */
    public void setClickable(boolean isClickable) {
        if (mMediator == null) return;
        mMediator.setClickable(isClickable);
    }

    /**
     * Sets the on key listener for the underlying menu button.
     * @param onKeyListener Listener for key events.
     */
    public void setOnKeyListener(OnKeyListener onKeyListener) {
        if (mMenuButton == null) return;
        mMenuButton.setOnKeyListener(onKeyListener);
    }

    public void destroy() {
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }

        if (mChangeProcessor != null) {
            mChangeProcessor.destroy();
            mChangeProcessor = null;
        }

        mMenuButton = null;
        mAppMenuButtonHelper = null;
    }

    /** @return Observer for menu state change. */
    public @Nullable Runnable getStateObserver() {
        return mMediator != null ? mMediator::updateStateChanged : null;
    }

    @Nullable
    public ObservableSupplier<AppMenuButtonHelper> getMenuButtonHelperSupplier() {
        if (mMediator == null) return null;
        return mMediator.getMenuButtonHelperSupplier();
    }

    /**
     * Set the visibility of the MenuButton controlled by this coordinator.
     *
     * @param visible Visibility state, true for visible and false for hidden.
     */
    public void setVisibility(boolean visible) {
        if (mMediator == null) return;
        mMediator.setVisibility(visible);
    }

    /**
     * Draws the current visual state of this component for the purposes of rendering the tab
     * switcher animation, setting the alpha to fade the view by the appropriate amount.
     * @param root Root view for the menu button; used to position the canvas that's drawn on.
     * @param canvas Canvas to draw to.
     * @param alpha Integer (0-255) alpha level to draw at.
     */
    public void drawTabSwitcherAnimationOverlay(View root, Canvas canvas, int alpha) {
        canvas.save();
        ViewUtils.translateCanvasToView(root, mMenuButton, canvas);
        mMenuButton.drawTabSwitcherAnimationOverlay(canvas, alpha);
        canvas.restore();
    }

    /**
     * Creates an animator for the MenuButton during the process offocusing or unfocusing the
     * UrlBar. The animation translate and fades the button into/out of view.
     * @return The Animator object for the MenuButton.
     * @param isFocusingUrl Whether the animation is for focusing the URL, meaning the button is
     *         fading out of view, or un-focusing, meaning it's fading into view.
     */
    public Animator getUrlFocusingAnimator(boolean isFocusingUrl) {
        return mMediator.getUrlFocusingAnimator(
                isFocusingUrl,
                mMenuButton != null && mMenuButton.getLayoutDirection() == LAYOUT_DIRECTION_RTL);
    }

    /** Returns whether the menu button is currently showing an update badge. */
    public boolean isShowingUpdateBadge() {
        return mPropertyModel.get(MenuButtonProperties.SHOW_UPDATE_BADGE).mShowUpdateBadge;
    }
}
