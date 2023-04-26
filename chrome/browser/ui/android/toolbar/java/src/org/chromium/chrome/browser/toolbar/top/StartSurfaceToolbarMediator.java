// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.ACCESSIBILITY_ENABLED;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.ALPHA;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BUTTONS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_AT_START;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IMAGE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_STATE_PROVIDER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_TAB_COUNT_PROVIDER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_TAB_MODEL_SELECTOR;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_NEW_TAB_ENABLED;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_HIGHLIGHT;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_VIEW_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_VIEW_TEXT_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.TAB_SWITCHER_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.TRANSLATION_Y;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.logo.LogoCoordinator;
import org.chromium.chrome.browser.logo.LogoView;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarAlphaInOverviewObserver;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelAnimatorFactory;

import java.util.function.BooleanSupplier;

/** The mediator implements interacts between the views and the caller. */
class StartSurfaceToolbarMediator implements ButtonDataProvider.ButtonDataObserver {
    private final PropertyModel mPropertyModel;
    private final Callback<IPHCommandBuilder> mShowIdentityIPHCallback;
    private final boolean mHideIncognitoSwitchWhenNoTabs;
    private final Supplier<ButtonData> mIdentityDiscButtonSupplier;
    private final boolean mIsTabGroupsAndroidContinuationEnabled;
    private final boolean mIsTabToGtsFadeAnimationEnabled;
    private final BooleanSupplier mIsIncognitoModeEnabledSupplier;
    private final MenuButtonCoordinator mMenuButtonCoordinator;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final IncognitoTabModelObserver mIncognitoTabModelObserver;
    private final Callback<LoadUrlParams> mLogoClickedCallback;
    private final boolean mIsRefactorEnabled;
    private final boolean mShouldFetchDoodle;
    private final ButtonDataProvider mIdentityDiscController;
    private final boolean mShouldCreateLogoInToolbar;
    private final Context mContext;

    private TabModelSelector mTabModelSelector;
    private TabCountProvider mTabCountProvider;

    @StartSurfaceState
    private int mStartSurfaceState;
    @LayoutType
    private int mLayoutType;
    private boolean mDefaultSearchEngineHasLogo;

    private CallbackController mCallbackController = new CallbackController();
    private float mNonIncognitoHomepageTranslationY;

    private boolean mIsNativeInitializedForLogo;
    private LogoCoordinator mLogoCoordinator;

    private ObjectAnimator mAlphaAnimator;
    private Callback<Boolean> mFinishedTransitionCallback;
    private @Nullable ToolbarAlphaInOverviewObserver mToolbarAlphaInOverviewObserver;

    StartSurfaceToolbarMediator(Context context, PropertyModel model,
            Callback<IPHCommandBuilder> showIdentityIPHCallback,
            boolean hideIncognitoSwitchWhenNoTabs, MenuButtonCoordinator menuButtonCoordinator,
            ButtonDataProvider identityDiscController,
            Supplier<ButtonData> identityDiscButtonSupplier, boolean isTabToGtsFadeAnimationEnabled,
            boolean isTabGroupsAndroidContinuationEnabled,
            BooleanSupplier isIncognitoModeEnabledSupplier,
            Callback<LoadUrlParams> logoClickedCallback, boolean isRefactorEnabled,
            boolean shouldFetchDoodle, boolean shouldCreateLogoInToolbar,
            Callback<Boolean> finishedTransitionCallback,
            ToolbarAlphaInOverviewObserver toolbarAlphaInOverviewObserver) {
        mPropertyModel = model;
        mStartSurfaceState = StartSurfaceState.NOT_SHOWN;
        mShowIdentityIPHCallback = showIdentityIPHCallback;
        mHideIncognitoSwitchWhenNoTabs = hideIncognitoSwitchWhenNoTabs;
        mMenuButtonCoordinator = menuButtonCoordinator;
        mIdentityDiscButtonSupplier = identityDiscButtonSupplier;
        mIsTabToGtsFadeAnimationEnabled = isTabToGtsFadeAnimationEnabled;
        mIsTabGroupsAndroidContinuationEnabled = isTabGroupsAndroidContinuationEnabled;
        mIsIncognitoModeEnabledSupplier = isIncognitoModeEnabledSupplier;
        mLogoClickedCallback = logoClickedCallback;
        mDefaultSearchEngineHasLogo = true;
        mShouldFetchDoodle = shouldFetchDoodle;
        mIdentityDiscController = identityDiscController;
        mIdentityDiscController.addObserver(this);
        mShouldCreateLogoInToolbar = shouldCreateLogoInToolbar;
        mIsRefactorEnabled = isRefactorEnabled;
        mFinishedTransitionCallback = finishedTransitionCallback;
        mToolbarAlphaInOverviewObserver = toolbarAlphaInOverviewObserver;
        mContext = context;

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mPropertyModel.set(IS_INCOGNITO, mTabModelSelector.isIncognitoSelected());
                updateIdentityDisc(mIdentityDiscButtonSupplier.get());
            }

            @Override
            public void onTabStateInitialized() {
                maybeInitializeIncognitoToggle();
            }
        };

        mIncognitoTabModelObserver = new IncognitoTabModelObserver() {
            @Override
            public void wasFirstTabCreated() {
                updateIncognitoToggleTabVisibility();
            }

            @Override
            public void didBecomeEmpty() {
                updateIncognitoToggleTabVisibility();
            }
        };
    }

    void destroy() {
        if (mTabModelSelector != null && mTabModelSelectorObserver != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
        if (mTabModelSelector != null && mIncognitoTabModelObserver != null) {
            mTabModelSelector.removeIncognitoTabModelObserver(mIncognitoTabModelObserver);
        }
        if (mLogoCoordinator != null) {
            mLogoCoordinator.destroy();
            mLogoCoordinator = null;
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        if (mToolbarAlphaInOverviewObserver != null) {
            mToolbarAlphaInOverviewObserver = null;
        }
        mIdentityDiscController.removeObserver(this);
    }

    void onStartSurfaceStateChanged(@StartSurfaceState int newState,
            boolean shouldShowStartSurfaceToolbar, @LayoutType int newLayoutType) {
        boolean wasOnGridTabSwitcher = isOnGridTabSwitcher();
        mStartSurfaceState = newState;
        mLayoutType = newLayoutType;
        updateLogoVisibility();
        updateTabSwitcherButtonVisibility();
        updateIncognitoToggleTabVisibility();
        updateNewTabViewVisibility();
        updateIdentityDisc(mIdentityDiscButtonSupplier.get());
        updateAppMenuUpdateBadgeSuppression();
        setStartSurfaceToolbarVisibility(shouldShowStartSurfaceToolbar, wasOnGridTabSwitcher);
        updateButtonsClickable(shouldShowStartSurfaceToolbar);
        updateTranslationY(mNonIncognitoHomepageTranslationY);
    }

    void onStartSurfaceHeaderOffsetChanged(int verticalOffset) {
        updateTranslationY(verticalOffset);
    }

    /**
     * The real omnibox should be shown in three cases:
     * 1. It's on the homepage, the url is focused and start surface toolbar is not visible.
     * 2. It's on the homepage and the fake search box is scrolled up to the screen top.
     * 3. It's on a tab.
     *
     * In the other cases:
     * 1. It's on the homepage and start surface toolbar is at least partially shown -- the user
     *    sees the fake search box.
     * 2. It's on the tab switcher surface, there is no search box (fake or real).
     *
     * @param fakeSearchBoxMarginToScreenTop The margin of fake search box to the screen top.
     * @return Whether toolbar layout should be shown.
     */
    boolean shouldShowRealSearchBox(int fakeSearchBoxMarginToScreenTop) {
        return isRealSearchBoxFocused()
                || isFakeSearchBoxScrolledToScreenTop(fakeSearchBoxMarginToScreenTop) || isOnATab();
    }

    /** Returns whether it's on the start surface homepage. */
    boolean isOnHomepage() {
        return mIsRefactorEnabled ? mLayoutType == LayoutType.START_SURFACE
                                  : mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE;
    }

    /** Returns whether it's on a normal tab. */
    private boolean isOnATab() {
        return mIsRefactorEnabled ? (!isOnHomepage() && !isOnGridTabSwitcher())
                                  : mStartSurfaceState == StartSurfaceState.NOT_SHOWN;
    }

    /** Returns whether it's on grid tab switcher surface. */
    boolean isOnGridTabSwitcher() {
        return mIsRefactorEnabled
                ? mLayoutType == LayoutType.TAB_SWITCHER
                : (mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER
                        || mStartSurfaceState == StartSurfaceState.SHOWING_TABSWITCHER);
    }

    /**
     * When mPropertyModel.get(IS_VISIBLE) is false on the homepage, start surface toolbar and fake
     * search box are not shown and the real search box is focused.
     * @return Whether the real search box is focused.
     */
    private boolean isRealSearchBoxFocused() {
        return isOnHomepage() && !mPropertyModel.get(IS_VISIBLE);
    }

    /**
     * @param fakeSearchBoxMarginToScreenTop The margin of fake search box to the screen top.
     * @return Whether the fake search box is scrolled to the top of the screen.
     */
    private boolean isFakeSearchBoxScrolledToScreenTop(int fakeSearchBoxMarginToScreenTop) {
        return mPropertyModel.get(IS_VISIBLE)
                && -mPropertyModel.get(TRANSLATION_Y) >= fakeSearchBoxMarginToScreenTop;
    }

    // Updates interactability to both New tab button "+" and New tab view text "New tab".
    void setNewTabEnabled(boolean enabled) {
        mPropertyModel.set(IS_NEW_TAB_ENABLED, enabled);
    }

    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mPropertyModel.set(NEW_TAB_CLICK_HANDLER, listener);
    }

    void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
    }

    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;

        if (mTabModelSelector.isTabStateInitialized()) maybeInitializeIncognitoToggle();
        mPropertyModel.set(IS_INCOGNITO, mTabModelSelector.isIncognitoSelected());
        updateIdentityDisc(mIdentityDiscButtonSupplier.get());
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mTabModelSelector.addIncognitoTabModelObserver(mIncognitoTabModelObserver);
    }

    void initLogoWithNative() {
        mIsNativeInitializedForLogo = true;
        if (mLogoCoordinator != null) mLogoCoordinator.initWithNative();
    }

    private void maybeInitializeIncognitoToggle() {
        if (mIsIncognitoModeEnabledSupplier.getAsBoolean()) {
            assert mTabCountProvider != null;
            mPropertyModel.set(INCOGNITO_TAB_COUNT_PROVIDER, mTabCountProvider);
            mPropertyModel.set(INCOGNITO_TAB_MODEL_SELECTOR, mTabModelSelector);
        }
    }

    private void updateIncognitoToggleTabVisibility() {
        if ((mIsRefactorEnabled && mLayoutType != LayoutType.TAB_SWITCHER)
                || (!mIsRefactorEnabled && mStartSurfaceState != StartSurfaceState.SHOWN_TABSWITCHER
                        && mStartSurfaceState != StartSurfaceState.SHOWING_TABSWITCHER)) {
            mPropertyModel.set(INCOGNITO_SWITCHER_VISIBLE, false);
            updateNewTabViewTextVisibility();
            return;
        }

        mPropertyModel.set(
                INCOGNITO_SWITCHER_VISIBLE, !mHideIncognitoSwitchWhenNoTabs || hasIncognitoTabs());
        updateNewTabViewTextVisibility();
    }

    private boolean hasIncognitoTabs() {
        if (mTabModelSelector == null) return false;
        return mTabModelSelector.getModel(true).getCount() != 0;
    }

    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mPropertyModel.set(INCOGNITO_STATE_PROVIDER, provider);
    }

    void onAccessibilityStatusChanged(boolean enabled) {
        mPropertyModel.set(ACCESSIBILITY_ENABLED, enabled);
        updateNewTabViewVisibility();
    }

    /**
     * @param highlight If the new tab button should be highlighted.
     */
    void setNewTabButtonHighlight(boolean highlight) {
        mPropertyModel.set(NEW_TAB_BUTTON_HIGHLIGHT, highlight);
    }

    /**
     * Called when the logo view is inflated.
     * @param logoView The logo view.
     */
    void onLogoViewReady(LogoView logoView) {
        if (!mShouldCreateLogoInToolbar) return;

        mLogoCoordinator = new LogoCoordinator(mContext, mLogoClickedCallback, logoView,
                mShouldFetchDoodle, /*onLogoAvailableCallback=*/null,
                /*onCachedLogoRevalidatedRunnable=*/null, isOnHomepage(), null);

        // The logo view may be ready after native is initialized, so we need to call
        // mLogoCoordinator.initWithNative() here in case that initLogoNative() skip it.
        if (mIsNativeInitializedForLogo) mLogoCoordinator.initWithNative();
    }

    private void setStartSurfaceToolbarVisibility(
            boolean shouldShowStartSurfaceToolbar, boolean wasOnGridTabSwitcher) {
        if (mPropertyModel.get(IS_VISIBLE) == shouldShowStartSurfaceToolbar) return;

        if (mAlphaAnimator != null) {
            mAlphaAnimator.cancel();
            mAlphaAnimator = null;
        }

        // Only show cross fade animation when switching between tab and grid tab switcher surface.
        // When switching between Start surface and grid tab switcher, the visibility won't change,
        // so it's shortcut above.
        boolean shouldShowAnimation =
                mIsTabToGtsFadeAnimationEnabled && (wasOnGridTabSwitcher || isOnGridTabSwitcher());

        // When animating into the TabSwitcherMode when the GTS supports accessibility then the
        // transition should also be immediate if touch exploration is enabled as the animation
        // causes races in the Android accessibility focus framework.
        if (shouldShowAnimation && !wasOnGridTabSwitcher
                && ChromeFeatureList.sTabGroupsContinuationAndroid.isEnabled()
                && ChromeFeatureList.sTabGroupsAndroid.isEnabled()
                && DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.getValue()
                && ChromeAccessibilityUtil.get().isTouchExplorationEnabled()) {
            shouldShowAnimation = false;
        }

        mPropertyModel.set(IS_VISIBLE, true);
        mPropertyModel.set(ALPHA, shouldShowStartSurfaceToolbar ? 0.0f : 1.0f);
        float targetAlpha = shouldShowStartSurfaceToolbar ? 1.0f : 0.0f;
        final long duration = TopToolbarCoordinator.TAB_SWITCHER_MODE_GTS_ANIMATION_DURATION_MS;
        mAlphaAnimator = PropertyModelAnimatorFactory.ofFloat(mPropertyModel, ALPHA, targetAlpha);
        mAlphaAnimator.setDuration(duration);
        mAlphaAnimator.setStartDelay(shouldShowStartSurfaceToolbar ? duration : 0);
        mAlphaAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        mAlphaAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onEnd(Animator animation) {
                finishAlphaAnimator(shouldShowStartSurfaceToolbar);
            }
        });
        // Notify the observer that the toolbar alpha value is changed and pass the rendering
        // toolbar alpha value to the observer.
        if (OmniboxFeatures.shouldMatchToolbarAndStatusBarColor()) {
            mAlphaAnimator.addUpdateListener(animation -> {
                Object alphaValue = animation.getAnimatedValue();
                if (mToolbarAlphaInOverviewObserver != null && alphaValue instanceof Float) {
                    mToolbarAlphaInOverviewObserver.onToolbarAlphaInOverviewChanged(
                            (Float) alphaValue);
                }
            });
        }

        mAlphaAnimator.start();
        if (!shouldShowAnimation) {
            mAlphaAnimator.end();
            return;
        }
    }

    private void finishAlphaAnimator(boolean shouldShowStartSurfaceToolbar) {
        mPropertyModel.set(ALPHA, 1.0f);
        mPropertyModel.set(IS_VISIBLE, shouldShowStartSurfaceToolbar);
        mFinishedTransitionCallback.onResult(shouldShowStartSurfaceToolbar);

        mAlphaAnimator = null;
    }

    private void updateLogoVisibility() {
        if (mLogoCoordinator == null) return;

        mLogoCoordinator.updateVisibilityAndMaybeCleanUp(isOnHomepage(),
                isOnATab() || isOnGridTabSwitcher()
                        || mStartSurfaceState == StartSurfaceState.DISABLED,
                /*animationEnabled*/ false);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void updateIdentityDisc(ButtonData buttonData) {
        boolean shouldShow = buttonData.canShow() && !mTabModelSelector.isIncognitoSelected();
        if (shouldShow) {
            ButtonSpec buttonSpec = buttonData.getButtonSpec();
            mPropertyModel.set(IDENTITY_DISC_CLICK_HANDLER, buttonSpec.getOnClickListener());
            // Take a defensive copy of the Drawable, since Drawables aren't immutable, and another
            // view mutating our drawable could cause it to display incorrectly.
            mPropertyModel.set(
                    IDENTITY_DISC_IMAGE, buttonSpec.getDrawable().getConstantState().newDrawable());
            mPropertyModel.set(IDENTITY_DISC_DESCRIPTION, buttonSpec.getContentDescription());
            mPropertyModel.set(IDENTITY_DISC_IS_VISIBLE, true);
            mShowIdentityIPHCallback.onResult(buttonSpec.getIPHCommandBuilder());
        } else {
            mPropertyModel.set(IDENTITY_DISC_IS_VISIBLE, false);
        }
    }

    private void updateNewTabViewVisibility() {
        boolean isShownTabSwitcherState = isOnGridTabSwitcher();

        // New tab button is only shown for homepage when accessibility is enabled and
        // OverviewListLayout is shown as the tab switcher instead of the start surface.
        mPropertyModel.set(NEW_TAB_VIEW_IS_VISIBLE,
                isShownTabSwitcherState
                        || (ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                                && !mIsTabGroupsAndroidContinuationEnabled));
        updateNewTabViewTextVisibility();
    }

    private void updateNewTabViewTextVisibility() {
        // Show new tab view text view when new tab view is visible and incognito switch is hidden.
        mPropertyModel.set(NEW_TAB_VIEW_TEXT_IS_VISIBLE,
                mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE)
                        && !mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
    }

    private void updateButtonsClickable(boolean isClickable) {
        mPropertyModel.set(BUTTONS_CLICKABLE, isClickable);
        mMenuButtonCoordinator.setClickable(isClickable);
    }

    private void updateTabSwitcherButtonVisibility() {
        // This button should only be shown on homepage. On tab switcher page, new tab button is
        // shown.
        boolean shouldShow = isOnHomepage();
        mPropertyModel.set(TAB_SWITCHER_BUTTON_IS_VISIBLE, shouldShow);
        // If tab switcher button is visible, we should move identity disc to the left.
        mPropertyModel.set(IDENTITY_DISC_AT_START, shouldShow);
    }

    private void updateAppMenuUpdateBadgeSuppression() {
        mMenuButtonCoordinator.setAppMenuUpdateBadgeSuppressed(isOnGridTabSwitcher());
    }

    private void updateTranslationY(float transY) {
        if (isOnHomepage() && !mPropertyModel.get(IS_INCOGNITO)) {
            // If it's on the non-incognito homepage, document the homepage translationY.
            mNonIncognitoHomepageTranslationY = transY;
            // Update the translationY of the toolbarView.
            mPropertyModel.set(TRANSLATION_Y, transY);
        } else {
            // If it's not on the non-incognito homepage, set the translationY as 0.
            mPropertyModel.set(TRANSLATION_Y, 0);
        }
    }

    @VisibleForTesting
    @StartSurfaceState
    int getOverviewModeStateForTesting() {
        return mStartSurfaceState;
    }

    @VisibleForTesting
    @LayoutType
    int getLayoutTypeForTesting() {
        return mLayoutType;
    }

    @VisibleForTesting
    boolean isLogoVisibleForTesting() {
        return mLogoCoordinator != null && mLogoCoordinator.isLogoVisible();
    }

    @VisibleForTesting
    LogoCoordinator getLogoCoordinatorForTesting() {
        return mLogoCoordinator;
    }

    @Override
    public void buttonDataChanged(boolean canShowHint) {
        // If the identity disc wants to be hidden and is hidden, there's nothing we need to do.
        if (!canShowHint && !mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE)) return;
        updateIdentityDisc(mIdentityDiscButtonSupplier.get());
    }
}
