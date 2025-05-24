// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.app.Activity;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator.SetFocusFunction;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonProperties.ShowBadgeProperty;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonProperties.ThemeProperty;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelAnimatorFactory;
import org.chromium.ui.util.TokenHolder;

/**
 * Mediator for the MenuButton. Listens for MenuButton state changes and drives corresponding
 * changes to the property model that backs the MenuButton view.
 */
@NullMarked
class MenuButtonMediator implements AppMenuObserver {
    private final Callback<AppMenuCoordinator> mAppMenuCoordinatorSupplierObserver;
    private @Nullable AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private @Nullable AppMenuButtonHelper mAppMenuButtonHelper;
    private final ObservableSupplierImpl<AppMenuButtonHelper> mAppMenuButtonHelperSupplier;
    private @Nullable AppMenuHandler mAppMenuHandler;
    private final BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    private final SetFocusFunction mSetUrlBarFocusFunction;
    private final PropertyModel mPropertyModel;
    private final Runnable mRequestRenderRunnable;
    private final ThemeColorProvider mThemeColorProvider;
    private final Activity mActivity;
    private final KeyboardVisibilityDelegate mKeyboardDelegate;
    private final boolean mCanShowAppUpdateBadge;
    private final Supplier<Boolean> mIsActivityFinishingSupplier;
    private int mFullscreenMenuToken = TokenHolder.INVALID_TOKEN;
    private int mFullscreenHighlightToken = TokenHolder.INVALID_TOKEN;
    private final Supplier<Boolean> mIsInOverviewModeSupplier;
    private final Resources mResources;
    private final OneshotSupplier<AppMenuCoordinator> mAppMenuCoordinatorSupplier;
    private final Supplier<MenuButtonState> mMenuButtonStateSupplier;
    private final Runnable mOnMenuButtonClicked;
    private final TokenHolder mHideTokenHolder;

    private final int mUrlFocusTranslationX;

    /**
     * @param propertyModel Model to write property changes to.
     * @param canShowAppUpdateBadge Whether the "update available" badge can be shown.
     * @param isActivityFinishingSupplier Supplier for knowing if the embedding activity is in the
     *     process of finishing or has already been destroyed.
     * @param requestRenderRunnable Runnable that requests a re-rendering of the compositor view
     *     containing the app menu button.
     * @param themeColorProvider Provider of theme color changes.
     * @param isInOverviewModeSupplier Supplier of overview mode state.
     * @param controlsVisibilityDelegate Delegate for forcing persistent display of browser
     *     controls.
     * @param setUrlBarFocusFunction Function that allows setting focus on the url bar.
     * @param appMenuCoordinatorSupplier Supplier for the AppMenuCoordinator, which owns all other
     *     app menu MVC components.
     * @param windowAndroid The WindowAndroid instance.
     * @param menuButtonStateSupplier Suplier of {@link MenuButtonState}.
     * @param onMenuButtonClicked Runnable to execute when menu button is clicked.
     */
    MenuButtonMediator(
            PropertyModel propertyModel,
            boolean canShowAppUpdateBadge,
            Supplier<Boolean> isActivityFinishingSupplier,
            Runnable requestRenderRunnable,
            ThemeColorProvider themeColorProvider,
            Supplier<Boolean> isInOverviewModeSupplier,
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate,
            SetFocusFunction setUrlBarFocusFunction,
            OneshotSupplier<AppMenuCoordinator> appMenuCoordinatorSupplier,
            WindowAndroid windowAndroid,
            Supplier<MenuButtonState> menuButtonStateSupplier,
            Runnable onMenuButtonClicked) {
        mPropertyModel = propertyModel;
        mCanShowAppUpdateBadge = canShowAppUpdateBadge;
        mIsActivityFinishingSupplier = isActivityFinishingSupplier;
        mRequestRenderRunnable = requestRenderRunnable;
        mThemeColorProvider = themeColorProvider;
        mIsInOverviewModeSupplier = isInOverviewModeSupplier;
        mControlsVisibilityDelegate = controlsVisibilityDelegate;
        mSetUrlBarFocusFunction = setUrlBarFocusFunction;
        mThemeColorProvider.addTintObserver(this::onTintChanged);
        mAppMenuCoordinatorSupplierObserver = this::onAppMenuInitialized;
        mAppMenuCoordinatorSupplier = appMenuCoordinatorSupplier;
        mAppMenuCoordinatorSupplier.onAvailable(mAppMenuCoordinatorSupplierObserver);
        mActivity = assertNonNull(windowAndroid.getActivity().get());
        mResources = mActivity.getResources();
        mAppMenuButtonHelperSupplier = new ObservableSupplierImpl<>();
        mKeyboardDelegate = windowAndroid.getKeyboardDelegate();
        mMenuButtonStateSupplier = menuButtonStateSupplier;
        mOnMenuButtonClicked = onMenuButtonClicked;
        mHideTokenHolder = new TokenHolder(this::updateMenuButtonVisibility);

        mUrlFocusTranslationX =
                mResources.getDimensionPixelSize(R.dimen.toolbar_url_focus_translation_x);
    }

    private void updateMenuButtonVisibility() {
        mPropertyModel.set(MenuButtonProperties.IS_VISIBLE, !mHideTokenHolder.hasTokens());
    }

    @Override
    public void onMenuVisibilityChanged(boolean isVisible) {
        if (isVisible) {
            // Defocus here to avoid handling focus in multiple places, e.g., when the
            // forward button is pressed. (see crbug.com/414219)
            mSetUrlBarFocusFunction.setFocus(false, OmniboxFocusReason.UNFOCUS);

            View view = mActivity.getCurrentFocus();
            if (view != null) {
                // Dismiss keyboard in case the user was interacting with an input field on a
                // website.
                mKeyboardDelegate.hideKeyboard(view);
            }

            if (!mIsInOverviewModeSupplier.get() && isShowingAppMenuUpdateBadge()) {
                // The app menu badge should be removed the first time the menu is opened.
                removeAppMenuUpdateBadge(true);
                mRequestRenderRunnable.run();
            }

            mFullscreenMenuToken =
                    mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mFullscreenMenuToken);
        } else {
            mControlsVisibilityDelegate.releasePersistentShowingToken(mFullscreenMenuToken);
        }

        if (isVisible) mOnMenuButtonClicked.run();
    }

    @Override
    public void onMenuHighlightChanged(boolean isHighlighting) {
        setMenuButtonHighlight(isHighlighting);

        if (isHighlighting) {
            mFullscreenHighlightToken =
                    mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mFullscreenHighlightToken);
        } else {
            mControlsVisibilityDelegate.releasePersistentShowingToken(mFullscreenHighlightToken);
        }
    }

    void setClickable(boolean isClickable) {
        mPropertyModel.set(MenuButtonProperties.IS_CLICKABLE, isClickable);
    }

    void setMenuButtonHighlight(boolean isHighlighting) {
        mPropertyModel.set(MenuButtonProperties.IS_HIGHLIGHTING, isHighlighting);
    }

    void setVisibility(boolean visible) {
        if (mHideTokenHolder.hasTokens()) return;
        mPropertyModel.set(MenuButtonProperties.IS_VISIBLE, visible);
    }

    /**
     * Hides menu button persistently until all tokens are released.
     *
     * @param token previously acquired token.
     * @return a new token that keeps menu button hidden.
     */
    int hideWithOldTokenRelease(int token) {
        int newToken = mHideTokenHolder.acquireToken();
        releaseHideToken(token);
        return newToken;
    }

    /**
     * Releases menu button hide token that might cause menu button to become visible if no more
     * tokens are held.
     *
     * @param token previously acquired token.
     */
    void releaseHideToken(int token) {
        mHideTokenHolder.releaseToken(token);
    }

    void updateReloadingState(boolean isLoading) {
        if (mAppMenuPropertiesDelegate == null || mAppMenuHandler == null) {
            return;
        }
        mAppMenuPropertiesDelegate.loadingStateChanged(isLoading);
        mAppMenuHandler.menuItemContentChanged(R.id.icon_row_menu_id);
    }

    ObservableSupplier<AppMenuButtonHelper> getMenuButtonHelperSupplier() {
        return mAppMenuButtonHelperSupplier;
    }

    void destroy() {
        if (mAppMenuButtonHelper != null) {
            assumeNonNull(mAppMenuHandler);
            mAppMenuHandler.removeObserver(this);
            mAppMenuButtonHelper = null;
        }
    }

    void updateStateChanged() {
        if (mIsActivityFinishingSupplier.get() || !mCanShowAppUpdateBadge) {
            return;
        }

        if (isUpdateAvailable()) {
            showAppMenuUpdateBadge(true);
            mRequestRenderRunnable.run();
        } else if (isShowingAppMenuUpdateBadge()) {
            removeAppMenuUpdateBadge(true);
        }
    }

    private void showAppMenuUpdateBadge(boolean animate) {
        MenuButtonState buttonState = mMenuButtonStateSupplier.get();
        assert buttonState != null : "No button state when trying to show the badge.";
        updateContentDescription(true, buttonState.menuContentDescription);
        mPropertyModel.set(
                MenuButtonProperties.SHOW_UPDATE_BADGE, new ShowBadgeProperty(true, animate));
    }

    private void removeAppMenuUpdateBadge(boolean animate) {
        mPropertyModel.set(
                MenuButtonProperties.SHOW_UPDATE_BADGE, new ShowBadgeProperty(false, animate));

        updateContentDescription(false, 0);
    }

    private void onTintChanged(
            @Nullable ColorStateList tintList,
            @Nullable ColorStateList activityFocusTintList,
            @BrandedColorScheme int brandedColorScheme) {
        mPropertyModel.set(
                MenuButtonProperties.THEME,
                new ThemeProperty(activityFocusTintList, brandedColorScheme));
    }

    /**
     * Sets the content description for the menu button.
     *
     * @param isUpdateBadgeVisible Whether the update menu badge is visible.
     * @param badgeContentDescription Resource id of the string to show if the update badge is
     *     visible.
     */
    private void updateContentDescription(
            boolean isUpdateBadgeVisible, int badgeContentDescription) {
        if (isUpdateBadgeVisible) {
            mPropertyModel.set(
                    MenuButtonProperties.CONTENT_DESCRIPTION,
                    mResources.getString(badgeContentDescription));
        } else {
            mPropertyModel.set(
                    MenuButtonProperties.CONTENT_DESCRIPTION,
                    mResources.getString(R.string.accessibility_toolbar_btn_menu));
        }
    }

    /**
     * Called when the app menu and related properties delegate are available.
     *
     * @param appMenuCoordinator The coordinator for interacting with the menu.
     */
    private void onAppMenuInitialized(AppMenuCoordinator appMenuCoordinator) {
        assert mAppMenuHandler == null;
        AppMenuHandler appMenuHandler = appMenuCoordinator.getAppMenuHandler();

        mAppMenuHandler = appMenuHandler;
        mAppMenuHandler.addObserver(this);
        mAppMenuButtonHelper = mAppMenuHandler.createAppMenuButtonHelper();
        mAppMenuButtonHelper.setOnAppMenuShownListener(
                () -> {
                    RecordUserAction.record("MobileToolbarShowMenu");
                });
        mPropertyModel.set(MenuButtonProperties.APP_MENU_BUTTON_HELPER, mAppMenuButtonHelper);

        mAppMenuButtonHelperSupplier.set(mAppMenuButtonHelper);
        mAppMenuPropertiesDelegate = appMenuCoordinator.getAppMenuPropertiesDelegate();
    }

    /**
     * @return Whether the badge is showing (either in the toolbar).
     */
    private boolean isShowingAppMenuUpdateBadge() {
        return mPropertyModel.get(MenuButtonProperties.SHOW_UPDATE_BADGE).mShowUpdateBadge;
    }

    private boolean isUpdateAvailable() {
        return mMenuButtonStateSupplier.get() != null;
    }

    public Animator getUrlFocusingAnimator(boolean isFocusingUrl, boolean isRtl) {
        float translationX;
        float alpha;
        if (isFocusingUrl) {
            float density = mResources.getDisplayMetrics().density;
            translationX = MathUtils.flipSignIf(mUrlFocusTranslationX, isRtl) * density;
            alpha = 0.0f;
        } else {
            translationX = 0.0f;
            alpha = 1.0f;
        }

        AnimatorSet animatorSet = new AnimatorSet();
        Animator translationAnimator =
                PropertyModelAnimatorFactory.ofFloat(
                        mPropertyModel, MenuButtonProperties.TRANSLATION_X, translationX);
        Animator alphaAnimator =
                PropertyModelAnimatorFactory.ofFloat(
                        mPropertyModel, MenuButtonProperties.ALPHA, alpha);
        animatorSet.playTogether(translationAnimator, alphaAnimator);
        return animatorSet;
    }
}
