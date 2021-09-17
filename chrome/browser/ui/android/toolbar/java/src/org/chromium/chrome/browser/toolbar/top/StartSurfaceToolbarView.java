// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewStub;
import android.view.animation.BaseInterpolator;
import android.view.animation.Interpolator;
import android.widget.ImageButton;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.IncognitoToggleTabLayout;
import org.chromium.chrome.browser.toolbar.NewTabButton;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;

import java.util.ArrayList;
import java.util.List;

/** View of the StartSurfaceToolbar */
class StartSurfaceToolbarView extends RelativeLayout {
    private static final boolean ARE_INTERPOLATORS_ENABLED =
            Build.VERSION.SDK_INT > VERSION_CODES.LOLLIPOP;
    private static final long SUPER_FAST_DURATION_MS = 50;
    private static final long FAST_DURATION_MS = 100;
    private static final long MEDIUM_DURATION_MS = 150;
    private static final long SLOW_DURATION_MS = 250;
    private static final long SHORT_DELAY_MS = 50;
    private static final long MEDIUM_DELAY_MS = 100;

    private final List<Animator> mAnimators = new ArrayList();

    private NewTabButton mNewTabButton;
    private HomeButton mHomeButton;
    private View mLogo;
    private View mTabSwitcherButtonView;

    @Nullable
    private IncognitoToggleTabLayout mIncognitoToggleTabLayout;
    @Nullable
    private ImageButton mIdentityDiscButton;
    private ColorStateList mLightIconTint;
    private ColorStateList mDarkIconTint;

    private Rect mLogoRect = new Rect();
    private Rect mViewRect = new Rect();

    private boolean mShouldShow;
    private boolean mInStartSurfaceMode;
    private boolean mShowTransitionAnimations;
    private AnimatorSet mLayoutChangeAnimatorSet;

    private ObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private ObservableSupplier<Boolean> mHomepageManagedByPolicySupplier;
    private boolean mIsHomeButtonInitialized;
    private boolean mIsShowing;

    private boolean mIsLogoVisible;
    private boolean mIsNewTabButtonVisible;
    private boolean mIsHomeButtonVisible;
    private boolean mIsIncognitoToggleVisible;
    private boolean mIsTabSwitcherButtonVisible;
    private boolean mIsIdentityDiscButtonVisible;
    private float mTabSwitcherButtonX;
    private int mToolbarButtonWidthPx;
    private float mIncognitoToggleX;
    private Rect mIncognitoToggleIndicatorBounds;

    public StartSurfaceToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mNewTabButton = findViewById(R.id.new_tab_button);
        mHomeButton = findViewById(R.id.home_button_on_tab_switcher);
        ViewStub incognitoToggleTabsStub = findViewById(R.id.incognito_tabs_stub);
        mIncognitoToggleTabLayout = (IncognitoToggleTabLayout) incognitoToggleTabsStub.inflate();
        mLogo = findViewById(R.id.logo);
        mIdentityDiscButton = findViewById(R.id.identity_disc_button);
        mTabSwitcherButtonView = findViewById(R.id.start_tab_switcher_button);
        updatePrimaryColorAndTint(false);
        mNewTabButton.setStartSurfaceEnabled(true);
        setIncognitoToggleTabSwitcherButtonXs();
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        // TODO(https://crbug.com/1040526)

        super.onLayout(changed, l, t, r, b);

        if (mLogo.getVisibility() == View.GONE) return;

        mLogoRect.set(mLogo.getLeft(), mLogo.getTop(), mLogo.getRight(), mLogo.getBottom());
        for (int viewIndex = 0; viewIndex < getChildCount(); viewIndex++) {
            View view = getChildAt(viewIndex);

            // If the view is incognito toggle, it is fading out and will finally disappear.
            if (view == mLogo || view.getVisibility() == View.GONE
                    || view == mIncognitoToggleTabLayout) {
                continue;
            }

            assert view.getVisibility() == View.VISIBLE;
            mViewRect.set(view.getLeft(), view.getTop(), view.getRight(), view.getBottom());
            if (Rect.intersects(mLogoRect, mViewRect)) {
                mLogo.setVisibility(View.GONE);
                break;
            }
        }
    }

    void setGridTabSwitcherEnabled(boolean isGridTabSwitcherEnabled) {
        mNewTabButton.setGridTabSwitcherEnabled(isGridTabSwitcherEnabled);
    }

    /**
     * Sets the {@link OnClickListener} that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mNewTabButton.setOnClickListener(listener);
    }

    /**
     * If transition animations shouldn't show, visibility is updated; Otherwise animations of logo
     * are added in startToolbarVisibilityAnimations() and visibility is set when animations end.
     * Logo will not show if screen is not wide enough.
     * @param isVisible Whether the Logo should be visible.
     */
    void setLogoVisibility(boolean isVisible) {
        mIsLogoVisible = isVisible;
        if (!mShowTransitionAnimations) mLogo.setVisibility(getVisibility(isVisible));
    }

    /**
     * @param isVisible Whether the menu button is visible.
     */
    void setMenuButtonVisibility(boolean isVisible) {
        final int buttonPaddingLeft = getContext().getResources().getDimensionPixelOffset(
                R.dimen.start_surface_toolbar_button_padding_to_button);
        final int buttonPaddingRight =
                (isVisible ? buttonPaddingLeft
                           : getContext().getResources().getDimensionPixelOffset(
                                   R.dimen.start_surface_toolbar_button_padding_to_edge));
        mIdentityDiscButton.setPadding(buttonPaddingLeft, 0, buttonPaddingRight, 0);
        mNewTabButton.setPadding(buttonPaddingLeft, 0, buttonPaddingRight, 0);
    }

    /**
     * If transition animations shouldn't show, visibility is updated; Otherwise animations of new
     * tab button are added in startToolbarVisibilityAnimations() and visibility is set when
     * animations end.
     * @param isVisible Whether the new tab button is visible.
     */
    void setNewTabButtonVisibility(boolean isVisible) {
        mIsNewTabButtonVisible = isVisible;
        if (isVisible && Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            // This is a workaround for the issue that the UrlBar is given the default focus on
            // Android versions before Pie when showing the start surface toolbar with the new tab
            // button (UrlBar is invisible to users). Check crbug.com/1081538 for more details.
            mNewTabButton.getParent().requestChildFocus(mNewTabButton, mNewTabButton);
        }
        if (!mShowTransitionAnimations) mNewTabButton.setVisibility(getVisibility(isVisible));
    }

    /**
     * If transition animations shouldn't show, visibility is updated; Otherwise animations of home
     * button are added in startToolbarVisibilityAnimations() and visibility is set when animations
     * end.
     * @param isVisible Whether the home button is visible.
     */
    void setHomeButtonVisibility(boolean isVisible) {
        mayInitializeHomeButton();
        mIsHomeButtonVisible = isVisible;
        if (!mShowTransitionAnimations) mHomeButton.setVisibility(getVisibility(isVisible));
    }

    /**
     * @param homepageEnabledSupplier Supplier of whether homepage is enabled.
     */
    void setHomepageEnabledSupplier(ObservableSupplier<Boolean> homepageEnabledSupplier) {
        assert mHomepageEnabledSupplier == null;
        mHomepageEnabledSupplier = homepageEnabledSupplier;
    }

    /**
     * @param homepageManagedByPolicySupplier Supplier of whether the homepage is managed by policy.
     */
    void setHomepageManagedByPolicySupplier(
            ObservableSupplier<Boolean> homepageManagedByPolicySupplier) {
        assert mHomepageManagedByPolicySupplier == null;
        mHomepageManagedByPolicySupplier = homepageManagedByPolicySupplier;
    }

    /**
     * Initializes the home button if not yet.
     */
    private void mayInitializeHomeButton() {
        if (mIsHomeButtonInitialized || mHomepageEnabledSupplier == null
                || mHomepageManagedByPolicySupplier == null) {
            return;
        }

        // The long click which shows the change homepage settings is disabled when the Start
        // surface is enabled.
        mHomeButton.init(mHomepageEnabledSupplier, null, mHomepageManagedByPolicySupplier);
        mIsHomeButtonInitialized = true;
    }

    /**
     * @param homeButtonClickHandler The callback that will be notified when the home button is
     *                               pressed.
     */
    void setHomeButtonClickHandler(OnClickListener homeButtonClickHandler) {
        mHomeButton.setOnClickListener(homeButtonClickHandler);
    }

    /**
     * @param isClickable Whether the buttons are clickable.
     */
    void setButtonClickableState(boolean isClickable) {
        mNewTabButton.setClickable(isClickable);
        mIncognitoToggleTabLayout.setClickable(isClickable);
    }

    /**
     * If transition animations shouldn't show, visibility is updated; Otherwise animations of
     * incognito toggle tab layout are added in startToolbarVisibilityAnimations() and visibility is
     * set when animations end.
     * @param isVisible Whether the Incognito toggle tab layout is visible.
     */
    void setIncognitoToggleTabVisibility(boolean isVisible) {
        mIsIncognitoToggleVisible = isVisible;
        if (!mShowTransitionAnimations) {
            mIncognitoToggleTabLayout.setVisibility(getVisibility(isVisible));
        }
    }

    /**
     * @param highlight If the new tab button should be highlighted.
     */
    void setNewTabButtonHighlight(boolean highlight) {
        if (mNewTabButton == null) return;
        if (highlight) {
            ViewHighlighter.turnOnHighlight(
                    mNewTabButton, new HighlightParams(HighlightShape.CIRCLE));
        } else {
            ViewHighlighter.turnOffHighlight(mNewTabButton);
        }
    }

    /** Called when incognito mode changes. */
    void updateIncognito(boolean isIncognito) {
        updatePrimaryColorAndTint(isIncognito);
    }

    /**
     * @param provider The {@link IncognitoStateProvider} passed to buttons that need access to it.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mNewTabButton.setIncognitoStateProvider(provider);
    }

    /** Called when accessibility status changes. */
    void onAccessibilityStatusChanged(boolean enabled) {
        mNewTabButton.onAccessibilityStatusChanged();
    }

    /** @return The View for the identity disc. */
    View getIdentityDiscView() {
        return mIdentityDiscButton;
    }

    /**
     * @param isAtStart Whether the identity disc is at start.
     */
    void setIdentityDiscAtStart(boolean isAtStart) {
        ((LayoutParams) mIdentityDiscButton.getLayoutParams()).removeRule(RelativeLayout.START_OF);
    }

    /**
     * If transition animations shouldn't show, visibility is updated; Otherwise animations of
     * identity disc are added in startToolbarVisibilityAnimations() and visibility is set when
     * animations end.
     * @param isVisible Whether the identity disc is visible.
     */
    void setIdentityDiscVisibility(boolean isVisible) {
        mIsIdentityDiscButtonVisible = isVisible;
        if (!mShowTransitionAnimations) mIdentityDiscButton.setVisibility(getVisibility(isVisible));
    }

    /**
     * Sets the {@link OnClickListener} that will be notified when the identity disc button is
     * pressed.
     * @param listener The callback that will be notified when the identity disc  is pressed.
     */
    void setIdentityDiscClickHandler(View.OnClickListener listener) {
        mIdentityDiscButton.setOnClickListener(listener);
    }

    /**
     * Updates the image displayed on the identity disc button.
     * @param image The new image for the button.
     */
    void setIdentityDiscImage(Drawable image) {
        mIdentityDiscButton.setImageDrawable(image);
    }

    /**
     * Updates idnetity disc content description.
     * @param contentDescriptionResId The new description for the button.
     */
    void setIdentityDiscContentDescription(@StringRes int contentDescriptionResId) {
        mIdentityDiscButton.setContentDescription(
                getContext().getResources().getString(contentDescriptionResId));
    }

    /**
     * Show or hide toolbar from tab.
     * @param inStartSurfaceMode Whether or not toolbar should be shown or hidden.
     * */
    void setStartSurfaceMode(boolean inStartSurfaceMode) {
        mInStartSurfaceMode = inStartSurfaceMode;
        startToolbarVisibilityAnimations();
    }

    /**
     * Show or hide toolbar.
     * @param shouldShowStartSurfaceToolbar Whether or not toolbar should be shown or hidden.
     * */
    void setToolbarVisibility(boolean shouldShowStartSurfaceToolbar) {
        mShouldShow = shouldShowStartSurfaceToolbar;
        startToolbarVisibilityAnimations();
    }

    /**
     * If transition animations shouldn't show, visibility is updated; Otherwise animations of tab
     * switcher button are added in startToolbarVisibilityAnimations() and visibility is set when
     * animations end.
     * @param isVisible Whether the tab switcher button is visible.
     */
    void setTabSwitcherButtonVisibility(boolean isVisible) {
        mIsTabSwitcherButtonVisible = isVisible;
        if (!mShowTransitionAnimations) {
            mTabSwitcherButtonView.setVisibility(getVisibility(isVisible));
        }
    }

    /**
     * Set TabCountProvider for incognito toggle view.
     * @param tabCountProvider The {@link TabCountProvider} to update the incognito toggle view.
     */
    void setTabCountProvider(TabCountProvider tabCountProvider) {
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.setTabCountProvider(tabCountProvider);
        }
    }

    /**
     * Set TabModelSelector for incognito toggle view.
     * @param selector  A {@link TabModelSelector} to provide information about open tabs.
     */
    void setTabModelSelector(TabModelSelector selector) {
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.setTabModelSelector(selector);
        }
    }

    void setShowAnimation(boolean showAnimation) {
        mShowTransitionAnimations = showAnimation;
    }

    /**
     * If transition animations shouldn't show, start animation to show or hide toolbar; Otherwise
     * show transition animations.
     */
    private void startToolbarVisibilityAnimations() {
        boolean shouldShowStartSurfaceToolbar = mInStartSurfaceMode && mShouldShow;

        if (mLayoutChangeAnimatorSet != null) mLayoutChangeAnimatorSet.cancel();

        if (!mShowTransitionAnimations) {
            if (shouldShowStartSurfaceToolbar == mIsShowing) return;
            mIsShowing = shouldShowStartSurfaceToolbar;

            // If transition animations of sub components shouldn't show, the fade animator of
            // toolbar view should show by default.
            addFadeAnimator(this, shouldShowStartSurfaceToolbar, MEDIUM_DURATION_MS,
                    /* delay = */ 0, Interpolators.LINEAR_INTERPOLATOR);
        } else if (shouldShowStartSurfaceToolbar) {
            addTransitionAnimators();
        }

        assert mLayoutChangeAnimatorSet == null;
        mLayoutChangeAnimatorSet = new AnimatorSet();
        mLayoutChangeAnimatorSet.playTogether(mAnimators);
        mLayoutChangeAnimatorSet.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onCancel(Animator animator) {
                mAnimators.clear();
                mLayoutChangeAnimatorSet = null;
            }
            @Override
            public void onEnd(Animator animator) {
                mAnimators.clear();
                mLayoutChangeAnimatorSet = null;
                mShowTransitionAnimations = false;
            }
        });
        mLayoutChangeAnimatorSet.start();
    }

    private void updatePrimaryColorAndTint(boolean isIncognito) {
        int primaryColor = ChromeColors.getPrimaryBackgroundColor(getContext(), isIncognito);
        setBackgroundColor(primaryColor);

        if (mLightIconTint == null) {
            mLightIconTint = AppCompatResources.getColorStateList(
                    getContext(), R.color.default_icon_color_light_tint_list);
            mDarkIconTint = AppCompatResources.getColorStateList(
                    getContext(), R.color.default_icon_color_tint_list);
        }
    }

    private void addTransitionAnimators() {
        addFadeAnimator(mLogo, mIsLogoVisible,
                mIsLogoVisible ? SLOW_DURATION_MS : MEDIUM_DURATION_MS,
                mIsLogoVisible ? SHORT_DELAY_MS : 0, Interpolators.LINEAR_INTERPOLATOR);

        addScaleAnimators(mNewTabButton, mIsNewTabButtonVisible, MEDIUM_DURATION_MS,
                mIsNewTabButtonVisible ? MEDIUM_DELAY_MS : 0,
                mIsNewTabButtonVisible ? Interpolators.DECELERATE_INTERPOLATOR
                                       : Interpolators.ACCELERATE_INTERPOLATOR);
        addFadeAnimator(mNewTabButton, mIsNewTabButtonVisible, MEDIUM_DURATION_MS,
                mIsNewTabButtonVisible ? MEDIUM_DELAY_MS : 0, Interpolators.LINEAR_INTERPOLATOR);

        addScaleAnimators(mHomeButton, mIsHomeButtonVisible, MEDIUM_DURATION_MS,
                mIsHomeButtonVisible ? MEDIUM_DELAY_MS : 0,
                mIsHomeButtonVisible ? Interpolators.DECELERATE_INTERPOLATOR
                                     : Interpolators.ACCELERATE_INTERPOLATOR);
        addFadeAnimator(mHomeButton, mIsHomeButtonVisible, MEDIUM_DURATION_MS,
                mIsHomeButtonVisible ? MEDIUM_DELAY_MS : 0, Interpolators.LINEAR_INTERPOLATOR);

        addScaleAnimators(mIdentityDiscButton, mIsIdentityDiscButtonVisible,
                mIsIdentityDiscButtonVisible ? MEDIUM_DURATION_MS : FAST_DURATION_MS,
                /* delay = */ 0,
                mIsIdentityDiscButtonVisible ? Interpolators.DECELERATE_INTERPOLATOR
                                             : Interpolators.ACCELERATE_INTERPOLATOR);
        addFadeAnimator(mIdentityDiscButton, mIsIdentityDiscButtonVisible, MEDIUM_DURATION_MS,
                /* delay = */ 0, Interpolators.LINEAR_INTERPOLATOR);

        if (mIncognitoToggleTabLayout.getVisibility() == getVisibility(mIsIncognitoToggleVisible)) {
            // If the visibility of incognito toggle layout isn't changed, we need to animate
            // the tab switcher button view itself.
            addScaleAnimators(mTabSwitcherButtonView, mIsTabSwitcherButtonVisible,
                    MEDIUM_DURATION_MS, mIsTabSwitcherButtonVisible ? SHORT_DELAY_MS : 0,
                    mIsTabSwitcherButtonVisible ? Interpolators.DECELERATE_INTERPOLATOR
                                                : Interpolators.ACCELERATE_INTERPOLATOR);
            addFadeAnimator(mTabSwitcherButtonView, mIsTabSwitcherButtonVisible, MEDIUM_DURATION_MS,
                    mIsTabSwitcherButtonVisible ? MEDIUM_DELAY_MS : 0,
                    Interpolators.LINEAR_INTERPOLATOR);
        } else {
            // If the visibility of incognito toggle layout is changed, we show its fade animations
            // and moving animations and update the visibility of the tab switcher button in {@link
            // finishMoveIncognitoToggleAnimation}.
            addFadeAnimator(mIncognitoToggleTabLayout.getIncognitoButtonIcon(),
                    mIsIncognitoToggleVisible,
                    mIsIncognitoToggleVisible ? FAST_DURATION_MS : SUPER_FAST_DURATION_MS,
                    mIsIncognitoToggleVisible ? SHORT_DELAY_MS : 0,
                    Interpolators.LINEAR_INTERPOLATOR);
            addMoveIncognitoToggleAnimator(mIsIncognitoToggleVisible);
        }
    }

    private void addScaleAnimators(View targetView, boolean showTargetView, long duration,
            long delay, BaseInterpolator interpolator) {
        if (targetView.getVisibility() == getVisibility(showTargetView)) return;

        // Set shrinking and expanding animations for the targetView. Set visibility as VISIBLE here
        // to show the animation. The visibility will be finally set in finishScaleAnimators().
        targetView.setVisibility(View.VISIBLE);
        targetView.setScaleX(showTargetView ? 0.0f : 1.0f);
        targetView.setScaleY(showTargetView ? 0.0f : 1.0f);

        Animator scaleAnimator = ObjectAnimator
                                         .ofFloat(targetView, SCALE_X, showTargetView ? 0.0f : 1.0f,
                                                 showTargetView ? 1.0f : 0.0f)
                                         .setDuration(duration);
        scaleAnimator.setStartDelay(delay);
        if (ARE_INTERPOLATORS_ENABLED) scaleAnimator.setInterpolator(interpolator);
        mAnimators.add(scaleAnimator);

        scaleAnimator = ObjectAnimator
                                .ofFloat(targetView, SCALE_Y, showTargetView ? 0.0f : 1.0f,
                                        showTargetView ? 1.0f : 0.0f)
                                .setDuration(duration);
        scaleAnimator.setStartDelay(delay);
        if (ARE_INTERPOLATORS_ENABLED) scaleAnimator.setInterpolator(interpolator);
        scaleAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onCancel(Animator animator) {
                finishScaleAnimators(targetView, showTargetView);
            }
            @Override
            public void onEnd(Animator animator) {
                finishScaleAnimators(targetView, showTargetView);
            }
        });
        mAnimators.add(scaleAnimator);
    }

    private void finishScaleAnimators(View targetView, boolean showTargetView) {
        targetView.setVisibility(getVisibility(showTargetView));
        targetView.setScaleX(1.0f);
        targetView.setScaleY(1.0f);
    }

    private void addFadeAnimator(View targetView, boolean showTargetView, long duration, long delay,
            Interpolator interpolator) {
        if (targetView.getVisibility() == getVisibility(showTargetView)
                && targetView != mIncognitoToggleTabLayout.getIncognitoButtonIcon()) {
            return;
        }
        // Show the fade-in and fade-out animation. Set visibility as VISIBLE here to show the
        // animation. The visibility will be finally set in finishFadeAnimator().
        targetView.setVisibility(View.VISIBLE);
        targetView.setAlpha(showTargetView ? 0.0f : 1.0f);
        Animator opacityAnimator =
                ObjectAnimator.ofFloat(targetView, ALPHA, showTargetView ? 1.0f : 0.0f)
                        .setDuration(duration);
        opacityAnimator.setStartDelay(delay);
        opacityAnimator.setInterpolator(interpolator);
        opacityAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onCancel(Animator animator) {
                finishFadeAnimator(targetView, showTargetView);
            }
            @Override
            public void onEnd(Animator animator) {
                finishFadeAnimator(targetView, showTargetView);
            }
        });
        mAnimators.add(opacityAnimator);
    }

    private void finishFadeAnimator(View targetView, boolean showTargetView) {
        // If targetView is the incognito button icon in incognito toggle layout, we cannot set it
        // gone because it will cause the incognito toggle distorted. The alpha will be reset in
        // finishMoveIncognitoToggleAnimator() after incognito toggle layout visibility is set.
        if (targetView == mIncognitoToggleTabLayout.getIncognitoButtonIcon()) return;
        targetView.setVisibility(getVisibility(showTargetView));
        targetView.setAlpha(1.0f);
    }

    private void addMoveIncognitoToggleAnimator(boolean showIncognitoToggle) {
        if (mIncognitoToggleTabLayout.getVisibility() == getVisibility(showIncognitoToggle)) return;
        mIncognitoToggleTabLayout.setVisibility(View.VISIBLE);
        mTabSwitcherButtonView.setVisibility(View.GONE);

        // Add the animation for toggle indicator width change.
        ValueAnimator valueAnimator =
                ValueAnimator.ofInt(showIncognitoToggle ? 0 : mToolbarButtonWidthPx / 2,
                        showIncognitoToggle ? mToolbarButtonWidthPx / 2 : 0);
        valueAnimator.addUpdateListener(animator -> {
            mIncognitoToggleTabLayout.setTabIndicatorFullWidth(false);
            mIncognitoToggleTabLayout.getTabSelectedIndicator().setBounds(
                    mToolbarButtonWidthPx / 2 - (int) valueAnimator.getAnimatedValue(),
                    mIncognitoToggleIndicatorBounds.top,
                    mToolbarButtonWidthPx / 2 + (int) valueAnimator.getAnimatedValue(),
                    mIncognitoToggleIndicatorBounds.bottom);
        });
        valueAnimator.setDuration(showIncognitoToggle ? SLOW_DURATION_MS : SUPER_FAST_DURATION_MS);
        if (ARE_INTERPOLATORS_ENABLED) {
            valueAnimator.setInterpolator(showIncognitoToggle
                            ? Interpolators.DECELERATE_INTERPOLATOR
                            : Interpolators.ACCELERATE_INTERPOLATOR);
        }
        mAnimators.add(valueAnimator);

        // Add the animation for incognito toggle moving.
        mIncognitoToggleTabLayout.setX(
                showIncognitoToggle ? mTabSwitcherButtonX : mIncognitoToggleX);
        Animator xAnimator =
                ObjectAnimator
                        .ofFloat(mIncognitoToggleTabLayout, X,
                                showIncognitoToggle ? mIncognitoToggleX : mTabSwitcherButtonX)
                        .setDuration(StartSurfaceToolbarView.SLOW_DURATION_MS);
        if (ARE_INTERPOLATORS_ENABLED) {
            xAnimator.setInterpolator(Interpolators.ACCELERATE_DECELERATE_INTERPOLATOR);
        }
        xAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onCancel(Animator animator) {
                finishMoveIncognitoToggleAnimator(showIncognitoToggle);
            }
            @Override
            public void onEnd(Animator animator) {
                finishMoveIncognitoToggleAnimator(showIncognitoToggle);
            }
        });
        mAnimators.add(xAnimator);
    }

    private void finishMoveIncognitoToggleAnimator(boolean showIncognitoToggle) {
        mTabSwitcherButtonView.setVisibility(getVisibility(mIsTabSwitcherButtonVisible));
        mIncognitoToggleTabLayout.setVisibility(getVisibility(showIncognitoToggle));
        mIncognitoToggleTabLayout.getIncognitoButtonIcon().setAlpha(1.0f);
        mIncognitoToggleTabLayout.setX(mIncognitoToggleX);
        mIncognitoToggleTabLayout.getTabSelectedIndicator().setBounds(
                showIncognitoToggle ? 0 : mToolbarButtonWidthPx / 2,
                mIncognitoToggleIndicatorBounds.top,
                showIncognitoToggle ? mToolbarButtonWidthPx : mToolbarButtonWidthPx / 2,
                mIncognitoToggleIndicatorBounds.bottom);
        mIncognitoToggleTabLayout.setTabIndicatorFullWidth(true);
    }

    private void setIncognitoToggleTabSwitcherButtonXs() {
        int screenWidthPx = dpToPx(getResources().getConfiguration().screenWidthDp);
        mToolbarButtonWidthPx =
                getResources().getDimensionPixelOffset(R.dimen.toolbar_button_width);
        mIncognitoToggleX = (float) screenWidthPx / 2 - mToolbarButtonWidthPx;
        mTabSwitcherButtonX = screenWidthPx - mToolbarButtonWidthPx * 2;
        if (mIncognitoToggleIndicatorBounds == null
                && mIncognitoToggleTabLayout.getTabSelectedIndicator() != null) {
            mIncognitoToggleIndicatorBounds =
                    mIncognitoToggleTabLayout.getTabSelectedIndicator().getBounds();
        }
    }

    private int getVisibility(boolean isVisible) {
        return isVisible ? View.VISIBLE : View.GONE;
    }

    private int dpToPx(int dp) {
        return (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, dp, getResources().getDisplayMetrics());
    }
}
