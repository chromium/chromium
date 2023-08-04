// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.view.View;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.function.BooleanSupplier;

/**
 * The coordinator for the tab switcher mode top toolbar, responsible for
 * communication with other UI components and lifecycle. Lazily creates the tab
 * switcher mode top toolbar the first time it's needed.
 */
class TabSwitcherModeTTCoordinator {
    private final ViewStub mTabSwitcherToolbarStub;
    private ViewStub mTabSwitcherFullscreenToolbarStub;

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

    private TabSwitcherModeTopToolbar mActiveTabSwitcherToolbar;

    private ToolbarColorObserverManager mToolbarColorObserverManager;

    @Nullable
    private IncognitoTabModelObserver mIncognitoTabModelObserver;

    private final boolean mIsGridTabSwitcherEnabled;
    private final boolean mIsTabToGtsAnimationEnabled;
    private final BooleanSupplier mIsIncognitoModeEnabledSupplier;
    private final TopToolbarInteractabilityManager mTopToolbarInteractabilityManager;

    TabSwitcherModeTTCoordinator(ViewStub tabSwitcherToolbarStub,
            ViewStub tabSwitcherFullscreenToolbarStub, MenuButtonCoordinator menuButtonCoordinator,
            boolean isGridTabSwitcherEnabled, boolean isTabToGtsAnimationEnabled,
            BooleanSupplier isIncognitoModeEnabledSupplier,
            ToolbarColorObserverManager toolbarColorObserverManager) {
        mTabSwitcherToolbarStub = tabSwitcherToolbarStub;
        mTabSwitcherFullscreenToolbarStub = tabSwitcherFullscreenToolbarStub;
        mMenuButtonCoordinator = menuButtonCoordinator;
        mIsGridTabSwitcherEnabled = isGridTabSwitcherEnabled;
        mIsTabToGtsAnimationEnabled = isTabToGtsAnimationEnabled;
        mIsIncognitoModeEnabledSupplier = isIncognitoModeEnabledSupplier;
        mTopToolbarInteractabilityManager =
                new TopToolbarInteractabilityManager(enabled -> setNewTabEnabled(enabled));
        mToolbarColorObserverManager = toolbarColorObserverManager;
    }

    /**
     * Set stub for GTS fullscreen toolbar.
     * @param toolbarStub stub to set.
     */
    void setFullScreenToolbarStub(ViewStub toolbarStub) {
        mTabSwitcherFullscreenToolbarStub = toolbarStub;
    }

    /**
     * Cleans up any code and removes observers as necessary.
     */
    void destroy() {
        if (mActiveTabSwitcherToolbar != null) {
            mActiveTabSwitcherToolbar.destroy();
            mActiveTabSwitcherToolbar = null;
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
        if (inTabSwitcherMode && mActiveTabSwitcherToolbar == null) {
            inflateToolbar();
        }
        if (mActiveTabSwitcherToolbar != null) {
            mActiveTabSwitcherToolbar.setTabSwitcherMode(inTabSwitcherMode);
        }
    }

    /**
     * Sets the OnClickListener that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mNewTabListener = listener;
        if (mActiveTabSwitcherToolbar != null) {
            mActiveTabSwitcherToolbar.setOnNewTabClickHandler(listener);
        }
    }

    /**
     * @param tabCountProvider The {@link TabCountProvider} used to observe the number of tabs in
     *                         the current model.
     */
    void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
        if (mActiveTabSwitcherToolbar != null) {
            mActiveTabSwitcherToolbar.setTabCountProvider(tabCountProvider);
        }
    }

    /**
     * Sets the current TabModelSelector so the toolbar can pass it into buttons that need access to
     * it.
     */
    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        if (mActiveTabSwitcherToolbar != null) {
            mActiveTabSwitcherToolbar.setTabModelSelector(selector);
        }

        maybeInitializeIncognitoTabModelObserver();
        maybeNotifyOnIncognitoTabsExistenceChanged();
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mIncognitoStateProvider = provider;
        if (mActiveTabSwitcherToolbar != null) {
            mActiveTabSwitcherToolbar.setIncognitoStateProvider(provider);
        }
    }

    /** Called when accessibility status changes. */
    void onAccessibilityStatusChanged(boolean enabled) {
        mAccessibilityEnabled = enabled;
        if (mActiveTabSwitcherToolbar != null) {
            mActiveTabSwitcherToolbar.onAccessibilityStatusChanged(enabled);
        }
    }

    /**
     * Inflates the toolbar.
     */
    private void inflateToolbar() {
        ViewStub toolbarStub;
        boolean isFullScreenToolbar;
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                    mTabSwitcherToolbarStub.getContext())) {
            toolbarStub = mTabSwitcherFullscreenToolbarStub;
            isFullScreenToolbar = true;
        } else {
            toolbarStub = mTabSwitcherToolbarStub;
            isFullScreenToolbar = false;
        }

        mActiveTabSwitcherToolbar = (TabSwitcherModeTopToolbar) toolbarStub.inflate();
        initializeToolbar(mActiveTabSwitcherToolbar, isFullScreenToolbar);

        maybeInitializeIncognitoTabModelObserver();
        maybeNotifyOnIncognitoTabsExistenceChanged();
    }

    /**
     * Initialize the toolbar with the requisite listeners, providers, etc.
     *
     * @param toolbar The toolbar to initialize.
     * @param isFullscreenToolbar Whether or not the given toolbar is fullscreen or not.
     */
    private void initializeToolbar(TabSwitcherModeTopToolbar toolbar, boolean isFullscreenToolbar) {
        toolbar.initialize(mIsGridTabSwitcherEnabled, isFullscreenToolbar,
                mIsTabToGtsAnimationEnabled, mIsIncognitoModeEnabledSupplier,
                mToolbarColorObserverManager);
        mMenuButtonCoordinator.setMenuButton(toolbar.findViewById(R.id.menu_button_wrapper));

        // It's expected that these properties are set by the time the tab switcher is entered.
        assert mNewTabListener != null;
        toolbar.setOnNewTabClickHandler(mNewTabListener);

        assert mTabCountProvider != null;
        toolbar.setTabCountProvider(mTabCountProvider);

        assert mTabModelSelector != null;
        toolbar.setTabModelSelector(mTabModelSelector);

        assert mIncognitoStateProvider != null;
        toolbar.setIncognitoStateProvider(mIncognitoStateProvider);

        if (mAccessibilityEnabled) {
            toolbar.onAccessibilityStatusChanged(mAccessibilityEnabled);
        }
    }

    /**
     * @param highlight If the new tab button should be highlighted.
     */
    void setNewTabButtonHighlight(boolean highlight) {
        assert mActiveTabSwitcherToolbar != null;
        mActiveTabSwitcherToolbar.setNewTabButtonHighlight(highlight);
    }

    @NonNull
    TopToolbarInteractabilityManager getTopToolbarInteractabilityManager() {
        return mTopToolbarInteractabilityManager;
    }

    /**
     * Initialize {@link IncognitoTabModelObserver}, if the new tab variation is enabled. This
     * function will initialize observer, if it is not initialized before.
     */
    private void maybeInitializeIncognitoTabModelObserver() {
        if (mTabModelSelector == null || mActiveTabSwitcherToolbar == null
                || !mIsIncognitoModeEnabledSupplier.getAsBoolean()
                || mIncognitoTabModelObserver != null) {
            return;
        }

        mIncognitoTabModelObserver = new IncognitoTabModelObserver() {
            @Override
            public void wasFirstTabCreated() {
                if (mActiveTabSwitcherToolbar != null) {
                    mActiveTabSwitcherToolbar.onIncognitoTabsExistenceChanged(true);
                }
            }

            @Override
            public void didBecomeEmpty() {
                if (mActiveTabSwitcherToolbar != null) {
                    mActiveTabSwitcherToolbar.onIncognitoTabsExistenceChanged(false);
                }
            }
        };
        mTabModelSelector.addIncognitoTabModelObserver(mIncognitoTabModelObserver);
    }

    /**
     * Update incognito logo visibility on toolbar, if the new tab variation is enabled.
     */
    private void maybeNotifyOnIncognitoTabsExistenceChanged() {
        if (mTabModelSelector == null || mActiveTabSwitcherToolbar == null
                || !mIsIncognitoModeEnabledSupplier.getAsBoolean()) {
            return;
        }

        boolean doesExist = mTabModelSelector.getModel(true).getCount() != 0;
        mActiveTabSwitcherToolbar.onIncognitoTabsExistenceChanged(doesExist);
    }

    private void setNewTabEnabled(boolean enabled) {
        if (mActiveTabSwitcherToolbar != null) {
            mActiveTabSwitcherToolbar.setNewTabButtonEnabled(enabled);
        }
    }
}
