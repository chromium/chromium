// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.content.Context;
import android.content.res.Configuration;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Optional toolbar button which opens a new tab. May be used by {@link
 * AdaptiveToolbarButtonController}.
 */
public class OptionalNewTabButtonController
        implements ButtonDataProvider, ConfigurationChangedObserver {
    /**
     * Set of methods used to interact with dependencies which may require native libraries to
     * function. Robolectric tests can use shadows to inject dependencies in tests.
     */
    @VisibleForTesting
    /* package */ static class Delegate {
        private final Supplier<TabCreatorManager> mTabCreatorManagerSupplier;
        private final Supplier<TabModelSelector> mTabModelSelectorSupplier;

        public Delegate(Supplier<TabCreatorManager> tabCreatorManagerSupplier,
                Supplier<TabModelSelector> tabModelSelectorSupplier) {
            mTabCreatorManagerSupplier = tabCreatorManagerSupplier;
            mTabModelSelectorSupplier = tabModelSelectorSupplier;
        }

        /** Returns a {@link TabCreatorManager} used for creating the new tab. */
        @Nullable
        TabCreatorManager getTabCreatorManager() {
            return mTabCreatorManagerSupplier.get();
        }

        /**
         * Returns a {@link TabModelSelector} used for obtaining the current tab and the incognito
         * state.
         *
         * <p>Not using {@link IncognitoStateProvider} because ISP is created in the {@link
         * ToolbarManager} and not in {@link RootUiCoordinator}.
         *
         * <p>TODO(crbug.com/1185948): Make IncognitoStateProvider available in RootUiCooridnator.
         */
        @Nullable
        TabModelSelector getTabModelSelector() {
            return mTabModelSelectorSupplier.get();
        }

        /** Returns {@code true} if the {@code tab} is on the new tab page. */
        boolean isNTPTab(Tab tab) {
            return UrlUtilities.isNTPUrl(tab.getUrl());
        }
    }

    /** Minimum width to show the new tab button. */
    public static final int MIN_WIDTH_DP = 360;

    /** Context used for fetching resources and window size. */
    private final Context mContext;
    private final Delegate mDelegate;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final ButtonDataImpl mButtonData;
    private final ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();

    private boolean mIsTablet;
    private int mScreenWidthDp;

    /**
     * Creates {@code OptionalNewTabButtonController}.
     *
     * @param context The Context for retrieving resources, etc.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events, e.g.
     *         configuration changes.
     * @param tabCreatorManagerSupplier Used to open new tabs.
     * @param tabModelSelectorSupplier Used to access the current incognito state.
     */
    public OptionalNewTabButtonController(Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Supplier<TabCreatorManager> tabCreatorManagerSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier) {
        mContext = context;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        mDelegate = new Delegate(tabCreatorManagerSupplier, tabModelSelectorSupplier);

        View.OnClickListener onClickListener = view -> {
            TabModelSelector tabModelSelector = mDelegate.getTabModelSelector();
            if (tabModelSelector == null) return;

            TabCreatorManager tabCreatorManager = mDelegate.getTabCreatorManager();
            if (tabCreatorManager == null) return;

            boolean isIncognito = tabModelSelector.isIncognitoSelected();
            RecordUserAction.record("MobileTopToolbarOptionalButtonNewTab");
            tabCreatorManager.getTabCreator(isIncognito).launchNTP();
        };

        mButtonData = new ButtonDataImpl(/*canShow=*/false,
                AppCompatResources.getDrawable(mContext, R.drawable.new_tab_icon), onClickListener,
                R.string.button_new_tab, /*supportsTinting=*/true, /*iphCommandBuilder=*/null,
                /*isEnabled=*/true, AdaptiveToolbarButtonVariant.NEW_TAB);

        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        mScreenWidthDp = mContext.getResources().getConfiguration().screenWidthDp;
    }

    @Override
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(this);
        mObservers.clear();
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        if (mScreenWidthDp == configuration.screenWidthDp && mIsTablet == isTablet) {
            return;
        }
        mScreenWidthDp = configuration.screenWidthDp;
        mIsTablet = isTablet;
        updateCanShow(getCurrentTab());
        notifyObservers(mButtonData.canShow());
    }

    @Override
    public void addObserver(ButtonDataObserver obs) {
        mObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(ButtonDataObserver obs) {
        mObservers.removeObserver(obs);
    }

    @Override
    public ButtonData get(Tab tab) {
        updateCanShow(tab);
        return mButtonData;
    }

    /**
     * Notifies each observer that the {@link ButtonData} provided by this {@link
     * ButtonDataProvider} has changed.
     *
     * @see #get(Tab)
     * @param canShowHint Hints whether the provided button can be shown.
     */
    private void notifyObservers(boolean canShowHint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(canShowHint);
        }
    }

    @Nullable
    private Tab getCurrentTab() {
        TabModelSelector tabModelSelector = mDelegate.getTabModelSelector();
        if (tabModelSelector == null) return null;
        return tabModelSelector.getCurrentTab();
    }

    private void updateCanShow(@Nullable Tab tab) {
        mButtonData.setCanShow(calculateCanShow(tab));
    }

    private boolean calculateCanShow(@Nullable Tab tab) {
        if (tab == null || tab.getWebContents() == null) {
            return false;
        }
        // The screen is too narrow to fit the icon.
        if (mScreenWidthDp < MIN_WIDTH_DP) {
            return false;
        }
        // On tablets a new tab button is shown on the tab strip.
        if (mIsTablet) {
            return false;
        }
        // Don't show on the NTP.
        if (mDelegate.isNTPTab(tab)) {
            return false;
        }

        return true;
    }
}
