// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.highlight.PulseDrawable;
import org.chromium.ui.interpolators.Interpolators;

/** The overflow menu button. */
public class MenuButton extends FrameLayout implements TintObserver {
    /** The {@link ImageButton} for the menu button. */
    private ImageButton mMenuImageButton;

    /** The view for the update badge. */
    private ImageView mUpdateBadgeView;

    private @BrandedColorScheme int mBrandedColorScheme;

    private AppMenuButtonHelper mAppMenuButtonHelper;

    private boolean mHighlightingMenu;
    private PulseDrawable mHighlightDrawable;
    private Drawable mOriginalBackground;

    private AnimatorSet mMenuBadgeAnimatorSet;
    private boolean mIsMenuBadgeAnimationRunning;

    /** A provider that notifies components when the theme color changes.*/
    private BitmapDrawable mMenuImageButtonAnimationDrawable;

    private BitmapDrawable mUpdateBadgeAnimationDrawable;

    private Supplier<MenuButtonState> mStateSupplier;

    public MenuButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMenuImageButton = findViewById(R.id.menu_button);
        mUpdateBadgeView = findViewById(R.id.menu_badge);
        mOriginalBackground = getBackground();
    }

    public void setAppMenuButtonHelper(AppMenuButtonHelper appMenuButtonHelper) {
        mAppMenuButtonHelper = appMenuButtonHelper;
        mMenuImageButton.setOnTouchListener(mAppMenuButtonHelper);
        mMenuImageButton.setAccessibilityDelegate(mAppMenuButtonHelper.getAccessibilityDelegate());
    }

    public ImageButton getImageButton() {
        return mMenuImageButton;
    }

    @Override
    public void setOnKeyListener(OnKeyListener onKeyListener) {
        if (mMenuImageButton == null) return;
        mMenuImageButton.setOnKeyListener(onKeyListener);
    }

    /**
     * Sets the update badge to visible.
     *
     * @param visible Whether the update badge should be visible. Always sets visibility to GONE if
     *     the update type does not require a badge. TODO(crbug.com/40585866): Clean this up when
     *     MenuButton and UpdateMenuItemHelper is MVCed.
     */
    private void setUpdateBadgeVisibility(boolean visible) {
        if (mUpdateBadgeView == null) return;
        mUpdateBadgeView.setVisibility(visible ? View.VISIBLE : View.GONE);
        if (visible) updateImageResources();
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (changed) {
            updateImageResources();
        }
    }

    private void updateImageResources() {
        mMenuImageButtonAnimationDrawable =
                (BitmapDrawable)
                        mMenuImageButton.getDrawable().getConstantState().newDrawable().mutate();

        mMenuImageButtonAnimationDrawable.setBounds(
                mMenuImageButton.getPaddingLeft(),
                mMenuImageButton.getPaddingTop(),
                mMenuImageButton.getWidth() - mMenuImageButton.getPaddingRight(),
                mMenuImageButton.getHeight() - mMenuImageButton.getPaddingBottom());
        mMenuImageButtonAnimationDrawable.setGravity(Gravity.CENTER);
        int color =
                ThemeUtils.getThemedToolbarIconTint(getContext(), mBrandedColorScheme)
                        .getDefaultColor();
        mMenuImageButtonAnimationDrawable.setColorFilter(color, PorterDuff.Mode.SRC_IN);

        // Not reliably set in tests.
        if (mStateSupplier == null) return;

        // As an optimization, don't re-calculate drawable state for the update badge unless we
        // intend to actually show it.
        MenuButtonState buttonState = mStateSupplier.get();
        if (buttonState == null || mUpdateBadgeView == null) return;
        @DrawableRes int drawable = getUpdateBadgeIcon(buttonState, mBrandedColorScheme);
        mUpdateBadgeView.setImageDrawable(
                ApiCompatibilityUtils.getDrawable(getResources(), drawable));
        mUpdateBadgeAnimationDrawable =
                (BitmapDrawable)
                        mUpdateBadgeView.getDrawable().getConstantState().newDrawable().mutate();
        mUpdateBadgeAnimationDrawable.setBounds(
                mUpdateBadgeView.getPaddingLeft(),
                mUpdateBadgeView.getPaddingTop(),
                mUpdateBadgeView.getWidth() - mUpdateBadgeView.getPaddingRight(),
                mUpdateBadgeView.getHeight() - mUpdateBadgeView.getPaddingBottom());
        mUpdateBadgeAnimationDrawable.setGravity(Gravity.CENTER);
    }

    private @DrawableRes int getUpdateBadgeIcon(
            MenuButtonState buttonState, @BrandedColorScheme int brandedColorScheme) {
        @DrawableRes int drawable = buttonState.adaptiveBadgeIcon;
        if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME
                || brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            drawable = buttonState.lightBadgeIcon;
        } else if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            drawable = buttonState.darkBadgeIcon;
        }
        return drawable;
    }

    /**
     * Set the supplier of menu button state.
     * @param supplier Supplier of menu button state.
     */
    void setStateSupplier(Supplier<MenuButtonState> supplier) {
        mStateSupplier = supplier;
    }

    /**
     * Show the update badge on the app menu button.
     * @param animate Whether to animate the showing of the update badge.
     */
    void showAppMenuUpdateBadge(boolean animate) {
        if (mUpdateBadgeView == null || mMenuImageButton == null) {
            return;
        }

        updateImageResources();
        if (!animate || mIsMenuBadgeAnimationRunning) {
            setUpdateBadgeVisibility(true);
            return;
        }

        // Set initial states.
        mUpdateBadgeView.setAlpha(0.f);
        mUpdateBadgeView.setVisibility(View.VISIBLE);

        mMenuBadgeAnimatorSet = createShowUpdateBadgeAnimation(mMenuImageButton, mUpdateBadgeView);

        mMenuBadgeAnimatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        mIsMenuBadgeAnimationRunning = true;
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mIsMenuBadgeAnimationRunning = false;
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                        mIsMenuBadgeAnimationRunning = false;
                    }
                });

        mMenuBadgeAnimatorSet.start();
    }

    /**
     * Remove the update badge on the app menu button.
     * @param animate Whether to animate the hiding of the update badge.
     */
    void removeAppMenuUpdateBadge(boolean animate) {
        if (mUpdateBadgeView == null || !isShowingAppMenuUpdateBadge()) return;

        if (!animate) {
            setUpdateBadgeVisibility(false);
            return;
        }

        if (mIsMenuBadgeAnimationRunning && mMenuBadgeAnimatorSet != null) {
            mMenuBadgeAnimatorSet.cancel();
        }

        // Set initial states.
        mMenuImageButton.setAlpha(0.f);

        mMenuBadgeAnimatorSet = createHideUpdateBadgeAnimation(mMenuImageButton, mUpdateBadgeView);

        mMenuBadgeAnimatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        mIsMenuBadgeAnimationRunning = true;
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mIsMenuBadgeAnimationRunning = false;
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                        mIsMenuBadgeAnimationRunning = false;
                    }
                });

        mMenuBadgeAnimatorSet.start();
    }

    /**
     * @return Whether the update badge is showing.
     */
    boolean isShowingAppMenuUpdateBadge() {
        return mUpdateBadgeView != null && mUpdateBadgeView.getVisibility() == View.VISIBLE;
    }

    void updateContentDescription(String description) {
        mMenuImageButton.setContentDescription(description);
    }

    /**
     * Sets the menu button's background depending on whether or not we are highlighting and whether
     * or not we are using light or dark assets.
     */
    private void updateMenuButtonHighlightDrawable() {
        // Return if onFinishInflate didn't finish
        if (mMenuImageButton == null) return;

        if (mHighlightingMenu) {
            if (mHighlightDrawable == null) {
                mHighlightDrawable = PulseDrawable.createCircle(getContext());
                mHighlightDrawable.setInset(
                        ViewCompat.getPaddingStart(mMenuImageButton),
                        mMenuImageButton.getPaddingTop(),
                        ViewCompat.getPaddingEnd(mMenuImageButton),
                        mMenuImageButton.getPaddingBottom());
            }
            // TODO(crbug.com/40191664) This doesn't work well with website themes.
            boolean isLightPulseColor =
                    mBrandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME
                            || mBrandedColorScheme == BrandedColorScheme.INCOGNITO;
            mHighlightDrawable.setUseLightPulseColor(getContext(), isLightPulseColor);
            setBackground(mHighlightDrawable);
            mHighlightDrawable.start();
        } else {
            setBackground(mOriginalBackground);
        }
    }

    void setMenuButtonHighlight(boolean highlight) {
        mHighlightingMenu = highlight;
        updateMenuButtonHighlightDrawable();
    }

    /**
     * Draws the current visual state of this component for the purposes of rendering the tab
     * switcher animation, setting the alpha to fade the view by the appropriate amount.
     * @param canvas Canvas to draw to.
     * @param alpha Integer (0-255) alpha level to draw at.
     */
    public void drawTabSwitcherAnimationOverlay(Canvas canvas, int alpha) {
        Drawable drawable = getTabSwitcherAnimationDrawable();
        drawable.setAlpha(alpha);
        drawable.draw(canvas);
    }

    public @BrandedColorScheme int getBrandedColorSchemeForTesting() {
        return mBrandedColorScheme;
    }

    @Override
    public void onTintChanged(
            ColorStateList tintList,
            ColorStateList activityFocusTintList,
            @BrandedColorScheme int brandedColorScheme) {
        ImageViewCompat.setImageTintList(mMenuImageButton, tintList);
        mBrandedColorScheme = brandedColorScheme;
        updateImageResources();
        updateMenuButtonHighlightDrawable();
    }

    @VisibleForTesting
    Drawable getTabSwitcherAnimationDrawable() {
        if (mUpdateBadgeAnimationDrawable == null && mMenuImageButtonAnimationDrawable == null) {
            updateImageResources();
        }

        return isShowingAppMenuUpdateBadge()
                ? mUpdateBadgeAnimationDrawable
                : mMenuImageButtonAnimationDrawable;
    }

    /**
     * Creates an {@link AnimatorSet} for showing the update badge that is displayed on top
     * of the app menu button.
     *
     * @param menuButton The {@link View} containing the app menu button.
     * @param menuBadge The {@link View} containing the update badge.
     * @return An {@link AnimatorSet} to run when showing the update badge.
     */
    private static AnimatorSet createShowUpdateBadgeAnimation(
            final View menuButton, final View menuBadge) {
        // Create badge ObjectAnimators.
        ObjectAnimator badgeFadeAnimator = ObjectAnimator.ofFloat(menuBadge, View.ALPHA, 1.f);
        badgeFadeAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);

        int pixelTranslation =
                menuBadge.getResources().getDimensionPixelSize(R.dimen.menu_badge_translation_y);
        ObjectAnimator badgeTranslateYAnimator =
                ObjectAnimator.ofFloat(menuBadge, View.TRANSLATION_Y, pixelTranslation, 0.f);
        badgeTranslateYAnimator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        // Create menu button ObjectAnimator.
        ObjectAnimator menuButtonFadeAnimator = ObjectAnimator.ofFloat(menuButton, View.ALPHA, 0.f);
        menuButtonFadeAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        // Create AnimatorSet and listeners.
        AnimatorSet set = new AnimatorSet();
        set.playTogether(badgeFadeAnimator, badgeTranslateYAnimator, menuButtonFadeAnimator);
        set.setDuration(350);
        set.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Make sure the menu button is visible again.
                        menuButton.setAlpha(1.f);
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                        // Jump to the end state if the animation is canceled.
                        menuBadge.setAlpha(1.f);
                        menuBadge.setTranslationY(0.f);
                        menuButton.setAlpha(1.f);
                    }
                });

        return set;
    }

    /**
     * Creates an {@link AnimatorSet} for hiding the update badge that is displayed on top
     * of the app menu button.
     *
     * @param menuButton The {@link View} containing the app menu button.
     * @param menuBadge The {@link View} containing the update badge.
     * @return An {@link AnimatorSet} to run when hiding the update badge.
     */
    private static AnimatorSet createHideUpdateBadgeAnimation(
            final View menuButton, final View menuBadge) {
        // Create badge ObjectAnimator.
        ObjectAnimator badgeFadeAnimator = ObjectAnimator.ofFloat(menuBadge, View.ALPHA, 0.f);
        badgeFadeAnimator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);

        // Create menu button ObjectAnimator.
        ObjectAnimator menuButtonFadeAnimator = ObjectAnimator.ofFloat(menuButton, View.ALPHA, 1.f);
        menuButtonFadeAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);

        // Create AnimatorSet and listeners.
        AnimatorSet set = new AnimatorSet();
        set.playTogether(badgeFadeAnimator, menuButtonFadeAnimator);
        set.setDuration(200);
        set.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        menuBadge.setVisibility(View.GONE);
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                        // Jump to the end state if the animation is canceled.
                        menuButton.setAlpha(1.f);
                        menuBadge.setVisibility(View.GONE);
                    }
                });

        return set;
    }

    void setOriginalBackgroundForTesting(Drawable background) {
        mOriginalBackground = background;
        setBackground(mOriginalBackground);
    }
}
