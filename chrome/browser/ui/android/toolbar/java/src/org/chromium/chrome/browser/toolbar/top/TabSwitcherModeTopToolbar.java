// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.IncognitoToggleTabLayout;
import org.chromium.chrome.browser.toolbar.NewTabButton;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarAlphaInOverviewObserver;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.widget.OptimizedFrameLayout;

import java.util.function.BooleanSupplier;

/** The tab switcher mode top toolbar */
public class TabSwitcherModeTopToolbar extends OptimizedFrameLayout
        implements View.OnClickListener, IncognitoStateProvider.IncognitoStateObserver {
    private View.OnClickListener mNewTabListener;

    private TabModelSelector mTabModelSelector;
    private IncognitoStateProvider mIncognitoStateProvider;
    private BooleanSupplier mIsIncognitoModeEnabledSupplier;

    private @Nullable IncognitoToggleTabLayout mIncognitoToggleTabLayout;
    // The following view is used as a variation for mNewTabImageButton. When this view is showing
    // as the button for creating new tab, the incognito toggle is hidden.
    private @Nullable View mNewTabViewButton;

    // The following two buttons are not used when Duet is enabled.
    private @Nullable NewTabButton mNewTabImageButton;
    private @Nullable MenuButton mMenuButton;

    private int mPrimaryColor;
    private @BrandedColorScheme int mBrandedColorScheme;

    private boolean mIsIncognito;
    private boolean mShouldShowNewTabVariation;

    private ObjectAnimator mVisiblityAnimator;
    private @Nullable ToolbarAlphaInOverviewObserver mToolbarAlphaInOverviewObserver;

    private boolean mIsFullscreenToolbar;
    private boolean mShowZoomingAnimation;

    public TabSwitcherModeTopToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mNewTabImageButton = findViewById(R.id.new_tab_button);
        mNewTabViewButton = findViewById(R.id.new_tab_view);
        mMenuButton = findViewById(R.id.menu_button_wrapper);

        // TODO(twellington): Try to make NewTabButton responsible for handling its own clicks.
        //                    TabSwitcherBottomToolbarCoordinator also uses NewTabButton and
        //                    sets an onClickListener directly on NewTabButton rather than
        //                    acting as the click listener itself so the behavior between this
        //                    class and the bottom toolbar will need to be unified.
        mNewTabImageButton.setOnClickListener(this);
        mNewTabViewButton.setOnClickListener(this);
    }

    void initialize(
            boolean isFullscreenToolbar,
            boolean isTabToGtsAnimationEnabled,
            BooleanSupplier isIncognitoModeEnabledSupplier,
            ToolbarColorObserverManager toolbarColorObserverManager) {
        mIsFullscreenToolbar = isFullscreenToolbar;
        mShowZoomingAnimation = isTabToGtsAnimationEnabled;
        mIsIncognitoModeEnabledSupplier = isIncognitoModeEnabledSupplier;
        mToolbarAlphaInOverviewObserver = toolbarColorObserverManager;

        mNewTabImageButton.setStartSurfaceEnabled(false);
        setIncognitoToggleVisibility(shouldShowIncognitoToggle());
        updateNewTabButtonVisibility();
    }

    @Override
    public void onClick(View v) {
        if (mNewTabImageButton == v || mNewTabViewButton == v) {
            v.setEnabled(false);
            if (mNewTabListener != null) mNewTabListener.onClick(v);
        }
    }

    /** Cleans up any code and removes observers as necessary. */
    void destroy() {
        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver(this);
            mIncognitoStateProvider = null;
        }
        if (mNewTabImageButton != null) {
            mNewTabImageButton.destroy();
            mNewTabImageButton = null;
        }
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.destroy();
            mIncognitoToggleTabLayout = null;
        }
        if (mToolbarAlphaInOverviewObserver != null) {
            mToolbarAlphaInOverviewObserver = null;
        }
    }

    /**
     * Called when tab switcher mode is entered or exited.
     * @param inTabSwitcherMode Whether or not tab switcher mode should be shown or hidden.
     */
    void setTabSwitcherMode(boolean inTabSwitcherMode) {
        // TODO(https://crbug.com/914868): Use consistent logic here for setting clickable/enabled
        // on mIncognitoToggleTabLayout & mNewTabButton?
        if (!inTabSwitcherMode) {
            if (mIncognitoToggleTabLayout != null) mIncognitoToggleTabLayout.setClickable(false);
        } else {
            if (mNewTabImageButton != null) mNewTabImageButton.setEnabled(true);
            if (mNewTabViewButton != null) mNewTabViewButton.setEnabled(true);
        }

        // Skip the animations and visibility logic when Tablet GTS polish param is enabled, since
        // they will instead be handled by the container view.
        if (mIsFullscreenToolbar) return;

        if (mVisiblityAnimator != null) mVisiblityAnimator.cancel();

        setVisibility(View.VISIBLE);
        // TODO(twellington): Handle interrupted animations to avoid jumps to 1.0 or 0.f.
        setAlpha(inTabSwitcherMode ? 0.0f : 1.0f);

        long duration =
                mShowZoomingAnimation
                        ? TopToolbarCoordinator.TAB_SWITCHER_MODE_GTS_ANIMATION_DURATION_MS
                        : TopToolbarCoordinator.TAB_SWITCHER_MODE_NORMAL_ANIMATION_DURATION_MS;

        mVisiblityAnimator =
                ObjectAnimator.ofFloat(this, View.ALPHA, inTabSwitcherMode ? 1.0f : 0.0f);
        mVisiblityAnimator.setDuration(duration);
        if (mShowZoomingAnimation && inTabSwitcherMode) mVisiblityAnimator.setStartDelay(duration);
        mVisiblityAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        mVisiblityAnimator.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onEnd(Animator animation) {
                        setAlpha(1.0f);

                        if (!inTabSwitcherMode) {
                            setVisibility(View.GONE);
                        }

                        if (mIncognitoToggleTabLayout != null) {
                            mIncognitoToggleTabLayout.setClickable(true);
                        }

                        mVisiblityAnimator = null;
                    }
                });
        // Notify the observer that the toolbar alpha value is changed and pass the rendering
        // toolbar alpha value to the observer.
        if (OmniboxFeatures.shouldMatchToolbarAndStatusBarColor()) {
            mVisiblityAnimator.addUpdateListener(
                    animation -> {
                        Object alphaValue = animation.getAnimatedValue();
                        if (mToolbarAlphaInOverviewObserver != null
                                && alphaValue instanceof Float) {
                            mToolbarAlphaInOverviewObserver.onToolbarAlphaInOverviewChanged(
                                    (Float) alphaValue);
                        }
                    });
        }

        mVisiblityAnimator.start();

        // When animating into the TabSwitcherMode when the GTS supports accessibility then the
        // transition should also be immediate if touch exploration is enabled as the animation
        // causes races in the Android accessibility focus framework.
        if (inTabSwitcherMode && AccessibilityState.isTouchExplorationEnabled()) {
            mVisiblityAnimator.end();
        }
    }

    /**
     * Sets the OnClickListener that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mNewTabListener = listener;
    }

    /** A method to toggle the enabled state of the new tab view button. */
    void setNewTabButtonEnabled(boolean enabled) {
        mNewTabViewButton.setEnabled(enabled);
        mNewTabImageButton.setEnabled(enabled);
    }

    /**
     * Sets the current TabModelSelector so the toolbar can pass it into buttons that need access to
     * it.
     */
    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.setTabModelSelector(selector);
        }
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mIncognitoStateProvider = provider;
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(this);

        if (mNewTabImageButton != null) {
            mNewTabImageButton.setIncognitoStateProvider(mIncognitoStateProvider);
        }
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        mIsIncognito = isIncognito;
        updatePrimaryColorAndTint();
    }

    /** Called when accessibility status changes. */
    void onAccessibilityStatusChanged(boolean enabled) {
        if (mNewTabImageButton != null) mNewTabImageButton.onAccessibilityStatusChanged();

        updatePrimaryColorAndTint();
    }

    /** Called when incognito tab existence changes. */
    void onIncognitoTabsExistenceChanged(boolean doesExist) {
        setIncognitoToggleVisibility(doesExist);
        boolean shouldShowNewTabVariation = shouldShowNewTabVariation(doesExist);
        if (shouldShowNewTabVariation == mShouldShowNewTabVariation) return;

        mShouldShowNewTabVariation = shouldShowNewTabVariation;
        updateNewTabButtonVisibility();
    }

    /**
     * @param highlight If the new tab button should be highlighted.
     */
    void setNewTabButtonHighlight(boolean highlight) {
        if (mNewTabImageButton == null) return;
        if (highlight) {
            ViewHighlighter.turnOnHighlight(
                    mNewTabImageButton, new HighlightParams(HighlightShape.CIRCLE));
        } else {
            ViewHighlighter.turnOffHighlight(mNewTabImageButton);
        }
    }

    private void updateNewTabButtonVisibility() {
        if (mNewTabViewButton != null) {
            mNewTabViewButton.setVisibility(mShouldShowNewTabVariation ? VISIBLE : GONE);
        }
        if (mNewTabImageButton != null) {
            mNewTabImageButton.setVisibility(!mShouldShowNewTabVariation ? VISIBLE : GONE);
        }
    }

    private void updatePrimaryColorAndTint() {
        int primaryColor = getToolbarColorForCurrentState();
        if (mPrimaryColor != primaryColor) {
            mPrimaryColor = primaryColor;
            setBackgroundColor(primaryColor);
        }

        @BrandedColorScheme int brandedColorScheme;
        if (primaryColor == Color.TRANSPARENT) {
            // If the toolbar is transparent, the icon tint will depend on the background color of
            // the tab switcher, which is the standard mode background. Note that horizontal tab
            // switcher is an exception, which uses the correspond background color for standard
            // and incognito mode.
            brandedColorScheme = BrandedColorScheme.APP_DEFAULT;
        } else {
            brandedColorScheme =
                    OmniboxResourceProvider.getBrandedColorScheme(
                            getContext(), mIsIncognito, primaryColor);
        }

        if (mBrandedColorScheme == brandedColorScheme) return;

        mBrandedColorScheme = brandedColorScheme;

        final ColorStateList tint =
                ThemeUtils.getThemedToolbarIconTint(getContext(), brandedColorScheme);
        if (mNewTabViewButton != null) {
            ((ImageView) mNewTabViewButton.findViewById(R.id.new_tab_view_button))
                    .setImageTintList(tint);
            final TextView newTabViewDesc = mNewTabViewButton.findViewById(R.id.new_tab_view_desc);
            newTabViewDesc.setTextColor(tint);
        }

        if (mMenuButton != null) {
            mMenuButton.onTintChanged(tint, brandedColorScheme);
        }
    }

    private int getToolbarColorForCurrentState() {
        // TODO(huayinz): Split tab switcher background color from primary background color.
        return ChromeColors.getPrimaryBackgroundColor(getContext(), mIsIncognito);
    }

    private void inflateIncognitoToggle() {
        ViewStub incognitoToggleTabsStub = findViewById(R.id.incognito_tabs_stub);
        mIncognitoToggleTabLayout = (IncognitoToggleTabLayout) incognitoToggleTabsStub.inflate();

        if (mTabModelSelector != null) {
            mIncognitoToggleTabLayout.setTabModelSelector(mTabModelSelector);
        }
    }

    private void setIncognitoToggleVisibility(boolean showIncognitoToggle) {
        if (!shouldShowIncognitoToggle()) return;
        if (mIncognitoToggleTabLayout == null) {
            if (showIncognitoToggle) inflateIncognitoToggle();
        } else {
            mIncognitoToggleTabLayout.setVisibility(showIncognitoToggle ? View.VISIBLE : View.GONE);
        }
    }

    /**
     * @return Whether or not incognito toggle should be visible based on the enabled features,
     *         incognito status and form-factor.
     */
    private boolean shouldShowIncognitoToggle() {
        // TODO(crbug.com/1434937): Remove top toggle (and update "New Tab" button logic,
        //  accordingly) for the a11y switcher, since that variant has the bottom toggle showing.
        return mIsIncognitoModeEnabledSupplier.getAsBoolean()
                && (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())
                        || mIsFullscreenToolbar);
    }

    /**
     * @return Whether or not new tab variant should be enabled based on if incognito tabs exists
     * and form factor. Always returns true for tablets.
     */
    private boolean shouldShowNewTabVariation(boolean incognitoTabExists) {
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            return true;
        }

        return !incognitoTabExists;
    }
}
