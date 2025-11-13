// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import static android.view.View.LAYOUT_DIRECTION_RTL;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonProperties.ShowBadgeProperty;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonProperties.ThemeProperty;
import org.chromium.chrome.browser.toolbar.top.ToolbarChildButton;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.KeyboardNavigationListener;

import java.util.function.Supplier;

/**
 * Root component for the app menu button on the toolbar. Owns the MenuButton view and handles
 * changes to its visual state, e.g. showing/hiding the app update badge.
 */
@NullMarked
public class MenuButtonCoordinator extends ToolbarChildButton {
    public interface SetFocusFunction {
        void setFocus(boolean focus, int reason);
    }

    /** Delegate for handling the visibility of the menu button. */
    public interface VisibilityDelegate {
        /**
         * Sets the menu button visibility.
         *
         * @param visible Whether the menu button should be on the toolbar and visible.
         */
        void setMenuButtonVisible(boolean visible);

        /** Whether the menu button is visible. */
        boolean isMenuButtonVisible();
    }

    private final Activity mActivity;
    private final PropertyModel mPropertyModel;
    private final @Nullable VisibilityDelegate mVisibilityDelegate;
    private MenuButtonMediator mMediator;
    private @Nullable AppMenuButtonHelper mAppMenuButtonHelper;
    private @Nullable MenuButton mMenuButton;
    private @Nullable PropertyModelChangeProcessor mChangeProcessor;

    /**
     * @param activity The Activity containing the menu button.
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
     * @param visibilityDelegate Delegate for handling the visibility of the menu button.
     * @param isWebApp Whether the app is a webApp.
     */
    public MenuButtonCoordinator(
            Activity activity,
            OneshotSupplier<AppMenuCoordinator> appMenuCoordinatorSupplier,
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate,
            WindowAndroid windowAndroid,
            SetFocusFunction setUrlBarFocusFunction,
            Runnable requestRenderRunnable,
            boolean canShowAppUpdateBadge,
            Supplier<Boolean> isInOverviewModeSupplier,
            ThemeColorProvider themeColorProvider,
            IncognitoStateProvider incognitoStateProvider,
            Supplier<@Nullable MenuButtonState> menuButtonStateSupplier,
            Runnable onMenuButtonClicked,
            @IdRes int menuButtonId,
            @Nullable VisibilityDelegate visibilityDelegate,
            boolean isWebApp) {
        super(activity, themeColorProvider, incognitoStateProvider);
        mActivity = activity;
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
                        .with(MenuButtonProperties.HAS_SPACE_TO_SHOW, true)
                        .with(MenuButtonProperties.STATE_SUPPLIER, menuButtonStateSupplier)
                        .with(
                                MenuButtonProperties.ON_KEY_LISTENER,
                                new KeyboardNavigationListener() {
                                    @Override
                                    protected boolean handleEnterKeyPress() {
                                        return onEnterKeyPress();
                                    }
                                })
                        .build();
        mMediator =
                new MenuButtonMediator(
                        mPropertyModel,
                        canShowAppUpdateBadge,
                        () -> mActivity.isFinishing() || mActivity.isDestroyed(),
                        requestRenderRunnable,
                        isInOverviewModeSupplier,
                        controlsVisibilityDelegate,
                        setUrlBarFocusFunction,
                        appMenuCoordinatorSupplier,
                        windowAndroid,
                        menuButtonStateSupplier,
                        onMenuButtonClicked,
                        visibilityDelegate,
                        themeColorProvider,
                        isWebApp);
        mMediator
                .getMenuButtonHelperSupplier()
                .addObserver((helper) -> mAppMenuButtonHelper = helper);
        if (mMenuButton != null) {
            mChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mPropertyModel, mMenuButton, new MenuButtonViewBinder());
        }
        mVisibilityDelegate = visibilityDelegate;
    }

    /**
     * Update the state of AppMenu components that need to know if the current page is loading, e.g.
     * the stop/reload button.
     *
     * @param isLoading Whether the current page is loading.
     */
    public void updateReloadingState(boolean isLoading) {
        if (mMediator == null) return;
        mMediator.updateReloadingState(isLoading);
    }

    /** Disables the menu button, removing it from the view hierarchy and destroying it. */
    public void disableMenuButton() {
        if (mVisibilityDelegate != null) {
            mVisibilityDelegate.setMenuButtonVisible(false);
        }
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
     * Highlights a menu item the next time the menu is opened.
     *
     * @param menuItemId The ID of the menu item to be highlighted.
     */
    @SuppressWarnings("NullAway")
    public void highlightMenuItemOnShow(@IdRes int menuItemId) {
        mAppMenuButtonHelper.highlightMenuItemOnShow(menuItemId);
    }

    /**
     * @return Whether the menu button is present and visible.
     */
    @Override
    public boolean isVisible() {
        if (mVisibilityDelegate != null) {
            return mVisibilityDelegate.isMenuButtonVisible();
        }
        return mMenuButton != null && mMenuButton.getVisibility() == View.VISIBLE;
    }

    /**
     * Get the underlying MenuButton view. Present for legacy reasons only; don't add new usages.
     */
    @Deprecated
    public @Nullable MenuButton getMenuButton() {
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
     * @param insets The insets to apply to the background.
     */
    public void setBackgroundInsets(androidx.core.graphics.Insets insets) {
        if (mMediator == null) return;
        mMediator.setBackgroundInsets(insets);
    }

    @SuppressWarnings("NullAway")
    @Override
    public void destroy() {
        super.destroy();
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

    public @Nullable ObservableSupplier<AppMenuButtonHelper> getMenuButtonHelperSupplier() {
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
     * Sets whether the MenuButton has space to show.
     *
     * @param hasSpaceToShow Whether the button has space to show.
     */
    @Override
    public void setHasSpaceToShow(boolean hasSpaceToShow) {
        if (mMediator == null) return;
        mMediator.setHasSpaceToShow(hasSpaceToShow);
    }

    /**
     * Hides menu button persistently until all tokens are released.
     *
     * @param token previously acquired token.
     * @return a new token that keeps menu button hidden.
     */
    public int hideWithOldTokenRelease(int token) {
        return mMediator.hideWithOldTokenRelease(token);
    }

    /**
     * Releases menu button hide token that might cause menu button to become visible if no more
     * tokens are held.
     *
     * @param token previously acquired token.
     */
    public void releaseHideToken(int token) {
        mMediator.releaseHideToken(token);
    }

    /**
     * Draws the current visual state of this component for the purposes of rendering the tab
     * switcher animation, setting the alpha to fade the view by the appropriate amount.
     *
     * @param root Root view for the menu button; used to position the canvas that's drawn on.
     * @param canvas Canvas to draw to.
     * @param alpha Integer (0-255) alpha level to draw at.
     */
    public void drawTabSwitcherAnimationOverlay(View root, Canvas canvas, int alpha) {
        assumeNonNull(mMenuButton);
        canvas.save();
        ViewUtils.translateCanvasToView(root, mMenuButton, canvas);
        mMenuButton.drawTabSwitcherAnimationOverlay(canvas, alpha);
        canvas.restore();
    }

    /**
     * Creates an animator for the MenuButton during the process offocusing or unfocusing the
     * UrlBar. The animation translate and fades the button into/out of view.
     *
     * @return The Animator object for the MenuButton.
     * @param isFocusingUrl Whether the animation is for focusing the URL, meaning the button is
     *     fading out of view, or un-focusing, meaning it's fading into view.
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

    /**
     * Updates the menu button background.
     *
     * @param backgroundResId The button background resource.
     */
    public void updateButtonBackground(@DrawableRes int backgroundResId) {
        assumeNonNull(mMenuButton);
        mMenuButton.getImageButton().setBackgroundResource(backgroundResId);
    }

    /**
     * Gets an area of the button that are touchable/clickable.
     *
     * @return a {@link Rect} that contains touchable/clickable area.
     */
    public Rect getHitRect() {
        assumeNonNull(mMenuButton);
        final var rect = new Rect();
        mMenuButton.getHitRect(rect);
        return rect;
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        super.onTintChanged(tint, activityFocusTint, brandedColorScheme);
        if (mMediator == null) return;
        mMediator.onTintChanged(tint, activityFocusTint, brandedColorScheme);
    }
}
