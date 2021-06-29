// Copyright 2018 The Chromium Authors. All rights reserved.
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

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.BooleanSupplier;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
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
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.widget.OptimizedFrameLayout;

/** The tab switcher mode top toolbar shown on phones. */
public class TabSwitcherModeTTPhone extends OptimizedFrameLayout
        implements View.OnClickListener, IncognitoStateProvider.IncognitoStateObserver {
    private View.OnClickListener mNewTabListener;

    private TabCountProvider mTabCountProvider;
    private TabModelSelector mTabModelSelector;
    private IncognitoStateProvider mIncognitoStateProvider;
    private BooleanSupplier mIsIncognitoModeEnabledSupplier;

    private @Nullable IncognitoToggleTabLayout mIncognitoToggleTabLayout;
    // The following view is used as a variation for mNewTabImageButton. When this view is showing
    // as the button for creating new tab, the incognito toggle is hidden.
    private @Nullable View mNewTabViewButton;

    // The following three buttons are not used when Duet is enabled.
    private @Nullable NewTabButton mNewTabImageButton;
    private @Nullable ToggleTabStackButton mToggleTabStackButton;

    private int mPrimaryColor;
    private boolean mUseLightIcons;
    private ColorStateList mLightIconTint;
    private ColorStateList mDarkIconTint;

    private boolean mIsIncognito;
    private boolean mShouldShowNewTabVariation;

    private ObjectAnimator mVisiblityAnimator;

    private boolean mIsGridTabSwitcherEnabled;
    private boolean mShowZoomingAnimation;

    public TabSwitcherModeTTPhone(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mNewTabImageButton = findViewById(R.id.new_tab_button);
        mNewTabViewButton = findViewById(R.id.new_tab_view);
        mToggleTabStackButton = findViewById(R.id.tab_switcher_mode_tab_switcher_button);

        // TODO(twellington): Try to make NewTabButton responsible for handling its own clicks.
        //                    TabSwitcherBottomToolbarCoordinator also uses NewTabButton and
        //                    sets an onClickListener directly on NewTabButton rather than
        //                    acting as the click listener itself so the behavior between this
        //                    class and the bottom toolbar will need to be unified.
        mNewTabImageButton.setOnClickListener(this);
        mNewTabViewButton.setOnClickListener(this);
    }

    void initialize(boolean isGridTabSwitcherEnabled, boolean isTabToGtsAnimationEnabled,
            boolean isStartSurfaceEnabled, BooleanSupplier isIncognitoModeEnabledSupplier) {
        mIsGridTabSwitcherEnabled = isGridTabSwitcherEnabled;
        mShowZoomingAnimation = isGridTabSwitcherEnabled && isTabToGtsAnimationEnabled;
        mIsIncognitoModeEnabledSupplier = isIncognitoModeEnabledSupplier;

        mNewTabImageButton.setGridTabSwitcherEnabled(isGridTabSwitcherEnabled);
        mNewTabImageButton.setStartSurfaceEnabled(isStartSurfaceEnabled);
        updateTabSwitchingElements(shouldShowIncognitoToggle());
        updateNewTabButtonVisibility();
    }

    @Override
    public void onClick(View v) {
        if (mNewTabImageButton == v || mNewTabViewButton == v) {
            v.setEnabled(false);
            if (mNewTabListener != null) mNewTabListener.onClick(v);
        }
    }

    /**
     * Cleans up any code and removes observers as necessary.
     */
    void destroy() {
        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver(this);
            mIncognitoStateProvider = null;
        }
        if (mNewTabImageButton != null) {
            mNewTabImageButton.destroy();
            mNewTabImageButton = null;
        }
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.destroy();
            mToggleTabStackButton = null;
        }
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.destroy();
            mIncognitoToggleTabLayout = null;
        }
    }

    /**
     * Called when tab switcher mode is entered or exited.
     * @param inTabSwitcherMode Whether or not tab switcher mode should be shown or hidden.
     */
    void setTabSwitcherMode(boolean inTabSwitcherMode) {
        if (mVisiblityAnimator != null) mVisiblityAnimator.cancel();

        setVisibility(View.VISIBLE);
        // TODO(twellington): Handle interrupted animations to avoid jumps to 1.0 or 0.f.
        setAlpha(inTabSwitcherMode ? 0.0f : 1.0f);

        long duration = mShowZoomingAnimation
                ? TopToolbarCoordinator.TAB_SWITCHER_MODE_GTS_ANIMATION_DURATION_MS
                : TopToolbarCoordinator.TAB_SWITCHER_MODE_NORMAL_ANIMATION_DURATION_MS;

        mVisiblityAnimator =
                ObjectAnimator.ofFloat(this, View.ALPHA, inTabSwitcherMode ? 1.0f : 0.0f);
        mVisiblityAnimator.setDuration(duration);
        if (mShowZoomingAnimation && inTabSwitcherMode) mVisiblityAnimator.setStartDelay(duration);
        mVisiblityAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        // TODO(https://crbug.com/914868): Use consistent logic here for setting clickable/enabled
        // on mIncognitoToggleTabLayout & mNewTabButton?
        if (!inTabSwitcherMode) {
            if (mIncognitoToggleTabLayout != null) mIncognitoToggleTabLayout.setClickable(false);
        } else {
            if (mNewTabImageButton != null) mNewTabImageButton.setEnabled(true);
            if (mNewTabViewButton != null) mNewTabViewButton.setEnabled(true);
        }

        mVisiblityAnimator.addListener(new CancelAwareAnimatorListener() {
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

        mVisiblityAnimator.start();

        if (DeviceClassManager.enableAccessibilityLayout()) mVisiblityAnimator.end();
    }

    /**
     * Sets the OnClickListener that will be notified when the TabSwitcher button is pressed.
     * @param listener The callback that will be notified when the TabSwitcher button is pressed.
     */
    void setOnTabSwitcherClickHandler(View.OnClickListener listener) {
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setOnTabSwitcherClickHandler(listener);
        }
    }

    /**
     * Sets the OnClickListener that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mNewTabListener = listener;
    }

    /**
     * @param tabCountProvider The {@link TabCountProvider} used to observe the number of tabs in
     *                         the current model.
     */
    void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setTabCountProvider(tabCountProvider);
        }
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.setTabCountProvider(tabCountProvider);
        }
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
        if (!doesExist == mShouldShowNewTabVariation) return;
        mShouldShowNewTabVariation = !doesExist;

        // TODO(crbug.com/1012014): Address the empty top toolbar issue when adaptive toolbar and
        // new tab variation are both on.
        // Show new tab variation when there are no incognito tabs.
        assert mIncognitoToggleTabLayout != null;
        mIncognitoToggleTabLayout.setVisibility(mShouldShowNewTabVariation ? GONE : VISIBLE);
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

        boolean useLightIcons;
        if (primaryColor == Color.TRANSPARENT) {
            // If the toolbar is transparent, the icon tint will depend on the background color of
            // the tab switcher, which is the standard mode background. Note that horizontal tab
            // switcher is an exception, which uses the correspond background color for standard
            // and incognito mode.
            int backgroundColor = ChromeColors.getPrimaryBackgroundColor(getResources(), false);
            useLightIcons = ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor);
        } else {
            useLightIcons = ColorUtils.shouldUseLightForegroundOnBackground(primaryColor);
        }

        if (mUseLightIcons == useLightIcons) return;

        mUseLightIcons = useLightIcons;

        if (mLightIconTint == null) {
            mLightIconTint = AppCompatResources.getColorStateList(
                    getContext(), R.color.default_icon_color_light_tint_list);
            mDarkIconTint = AppCompatResources.getColorStateList(
                    getContext(), R.color.default_icon_color_tint_list);
        }

        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setUseLightDrawables(useLightIcons);
        }
    }

    private int getToolbarColorForCurrentState() {
        // TODO(huayinz): Split tab switcher background color from primary background color.
        if (DeviceClassManager.enableAccessibilityLayout() || mIsGridTabSwitcherEnabled) {
            return ChromeColors.getPrimaryBackgroundColor(getResources(), mIsIncognito);
        }

        return Color.TRANSPARENT;
    }

    private void inflateIncognitoToggle() {
        ViewStub incognitoToggleTabsStub = findViewById(R.id.incognito_tabs_stub);
        mIncognitoToggleTabLayout = (IncognitoToggleTabLayout) incognitoToggleTabsStub.inflate();

        if (mTabCountProvider != null) {
            mIncognitoToggleTabLayout.setTabCountProvider(mTabCountProvider);
        }

        if (mTabModelSelector != null) {
            mIncognitoToggleTabLayout.setTabModelSelector(mTabModelSelector);
        }
    }

    private void setIncognitoToggleVisibility(boolean showIncognitoToggle) {
        if (mIncognitoToggleTabLayout == null) {
            if (showIncognitoToggle) inflateIncognitoToggle();
        } else {
            mIncognitoToggleTabLayout.setVisibility(showIncognitoToggle ? View.VISIBLE : View.GONE);
        }
    }

    private void setToggleTabStackButtonVisibility(boolean showToggleTabStackButton) {
        if (mToggleTabStackButton == null) return;
        mToggleTabStackButton.setVisibility(showToggleTabStackButton ? View.VISIBLE : View.GONE);
    }

    private void updateTabSwitchingElements(boolean showIncognitoToggle) {
        setIncognitoToggleVisibility(showIncognitoToggle);
        setToggleTabStackButtonVisibility(!showIncognitoToggle);
    }

    /**
     * @return Whether or not incognito toggle should be visible based on the enabled features
     *         and incognito status.
     */
    private boolean shouldShowIncognitoToggle() {
        return mIsGridTabSwitcherEnabled && mIsIncognitoModeEnabledSupplier.getAsBoolean();
    }
}
