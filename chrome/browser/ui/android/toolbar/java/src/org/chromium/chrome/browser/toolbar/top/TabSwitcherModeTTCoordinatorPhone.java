// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.view.View;
import android.view.ViewStub;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.BooleanSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;

/**
 * The coordinator for the tab switcher mode top toolbar shown on phones, responsible for
 * communication with other UI components and lifecycle. Lazily creates the tab
 * switcher mode top toolbar the first time it's needed.
 */
class TabSwitcherModeTTCoordinatorPhone {
    private final ViewStub mTabSwitcherToolbarStub;

    // TODO(twellington): Create a model to hold all of these properties. Consider using
    // LazyConstructionPropertyMcp to collect all of the properties since it is designed to
    // aggregate properties and bind them to a view the first time it's shown.
    private View.OnClickListener mTabSwitcherListener;
    private View.OnClickListener mNewTabListener;
    private TabCountProvider mTabCountProvider;
    private TabModelSelector mTabModelSelector;
    private IncognitoStateProvider mIncognitoStateProvider;
    private MenuButtonCoordinator mMenuButtonCoordinator;
    private boolean mAccessibilityEnabled;

    private TabSwitcherModeTTPhone mTabSwitcherModeToolbar;

    @Nullable
    private IncognitoTabModelObserver mIncognitoTabModelObserver;

    private final boolean mIsGridTabSwitcherEnabled;
    private final boolean mIsTabToGtsAnimationEnabled;
    private final boolean mIsStartSurfaceEnabled;
    private final BooleanSupplier mIsIncognitoModeEnabledSupplier;

    TabSwitcherModeTTCoordinatorPhone(ViewStub tabSwitcherToolbarStub,
            MenuButtonCoordinator menuButtonCoordinator, boolean isGridTabSwitcherEnabled,
            boolean isTabToGtsAnimationEnabled, boolean isStartSurfaceEnabled,
            BooleanSupplier isIncognitoModeEnabledSupplier) {
        mTabSwitcherToolbarStub = tabSwitcherToolbarStub;
        mMenuButtonCoordinator = menuButtonCoordinator;
        mIsGridTabSwitcherEnabled = isGridTabSwitcherEnabled;
        mIsTabToGtsAnimationEnabled = isTabToGtsAnimationEnabled;
        mIsStartSurfaceEnabled = isStartSurfaceEnabled;
        mIsIncognitoModeEnabledSupplier = isIncognitoModeEnabledSupplier;
    }

    /**
     * Cleans up any code and removes observers as necessary.
     */
    void destroy() {
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.destroy();
            mTabSwitcherModeToolbar = null;
        }
        if (mTabModelSelector != null && mIncognitoTabModelObserver != null) {
            mTabModelSelector.removeIncognitoTabModelObserver(mIncognitoTabModelObserver);
        }
        if (mMenuButtonCoordinator != null) {
            mMenuButtonCoordinator.destroy();
            mMenuButtonCoordinator = null;
        }
    }

    /**
     * Called when tab switcher mode is entered or exited.
     * @param inTabSwitcherMode Whether or not tab switcher mode should be shown or hidden.
     */
    void setTabSwitcherMode(boolean inTabSwitcherMode) {
        if (inTabSwitcherMode) {
            if (mTabSwitcherModeToolbar == null) {
                initializeTabSwitcherToolbar();
            }

            mTabSwitcherModeToolbar.setTabSwitcherMode(inTabSwitcherMode);
        } else if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setTabSwitcherMode(inTabSwitcherMode);
        }
    }

    /**
     * Sets the OnClickListener that will be notified when the TabSwitcher button is pressed.
     * @param listener The callback that will be notified when the TabSwitcher button is pressed.
     */
    void setOnTabSwitcherClickHandler(View.OnClickListener listener) {
        mTabSwitcherListener = listener;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setOnTabSwitcherClickHandler(listener);
        }
    }

    /**
     * Sets the OnClickListener that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mNewTabListener = listener;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setOnNewTabClickHandler(listener);
        }
    }

    /**
     * @param tabCountProvider The {@link TabCountProvider} used to observe the number of tabs in
     *                         the current model.
     */
    void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setTabCountProvider(tabCountProvider);
        }
    }

    /**
     * Sets the current TabModelSelector so the toolbar can pass it into buttons that need access to
     * it.
     */
    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setTabModelSelector(selector);
        }

        maybeInitializeIncognitoTabModelObserver();
        maybeNotifyOnIncognitoTabsExistenceChanged();
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mIncognitoStateProvider = provider;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setIncognitoStateProvider(provider);
        }
    }

    /** Called when accessibility status changes. */
    void onAccessibilityStatusChanged(boolean enabled) {
        mAccessibilityEnabled = enabled;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.onAccessibilityStatusChanged(enabled);
        }
    }

    private void initializeTabSwitcherToolbar() {
        mTabSwitcherModeToolbar = (TabSwitcherModeTTPhone) mTabSwitcherToolbarStub.inflate();
        mTabSwitcherModeToolbar.initialize(mIsGridTabSwitcherEnabled, mIsTabToGtsAnimationEnabled,
                mIsStartSurfaceEnabled, mIsIncognitoModeEnabledSupplier);
        mMenuButtonCoordinator.setMenuButton(
                mTabSwitcherModeToolbar.findViewById(R.id.menu_button_wrapper));

        // It's expected that these properties are set by the time the tab switcher is entered.
        assert mTabSwitcherListener != null;
        mTabSwitcherModeToolbar.setOnTabSwitcherClickHandler(mTabSwitcherListener);

        assert mNewTabListener != null;
        mTabSwitcherModeToolbar.setOnNewTabClickHandler(mNewTabListener);

        assert mTabCountProvider != null;
        mTabSwitcherModeToolbar.setTabCountProvider(mTabCountProvider);

        assert mTabModelSelector != null;
        mTabSwitcherModeToolbar.setTabModelSelector(mTabModelSelector);

        assert mIncognitoStateProvider != null;
        mTabSwitcherModeToolbar.setIncognitoStateProvider(mIncognitoStateProvider);

        maybeInitializeIncognitoTabModelObserver();
        maybeNotifyOnIncognitoTabsExistenceChanged();

        if (mAccessibilityEnabled) {
            mTabSwitcherModeToolbar.onAccessibilityStatusChanged(mAccessibilityEnabled);
        }
    }

    private boolean isNewTabVariationEnabled() {
        return mIsGridTabSwitcherEnabled && ChromeFeatureList.isInitialized()
                && mIsIncognitoModeEnabledSupplier.getAsBoolean()
                && !ChromeFeatureList
                            .getFieldTrialParamByFeature(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                    "tab_grid_layout_android_new_tab")
                            .equals("false");
    }

    /**
     * @param highlight If the new tab button should be highlighted.
     */
    void setNewTabButtonHighlight(boolean highlight) {
        assert mTabSwitcherModeToolbar != null;
        mTabSwitcherModeToolbar.setNewTabButtonHighlight(highlight);
    }

    /**
     * Initialize {@link IncognitoTabModelObserver}, if the new tab variation is enabled. This
     * function will initialize observer, if it is not initialized before.
     */
    private void maybeInitializeIncognitoTabModelObserver() {
        if (mTabModelSelector == null || mTabSwitcherModeToolbar == null
                || !isNewTabVariationEnabled() || mIncognitoTabModelObserver != null) {
            return;
        }

        mIncognitoTabModelObserver = new IncognitoTabModelObserver() {
            @Override
            public void wasFirstTabCreated() {
                if (mTabSwitcherModeToolbar != null) {
                    mTabSwitcherModeToolbar.onIncognitoTabsExistenceChanged(true);
                }
            }

            @Override
            public void didBecomeEmpty() {
                if (mTabSwitcherModeToolbar != null) {
                    mTabSwitcherModeToolbar.onIncognitoTabsExistenceChanged(false);
                }
            }
        };
        mTabModelSelector.addIncognitoTabModelObserver(mIncognitoTabModelObserver);
    }

    /**
     * Update incognito logo visibility on toolbar, if the new tab variation is enabled.
     */
    private void maybeNotifyOnIncognitoTabsExistenceChanged() {
        if (mTabModelSelector == null || mTabSwitcherModeToolbar == null
                || !isNewTabVariationEnabled()) {
            return;
        }

        boolean doesExist = mTabModelSelector.getModel(true).getCount() != 0;
        mTabSwitcherModeToolbar.onIncognitoTabsExistenceChanged(doesExist);
    }
}
