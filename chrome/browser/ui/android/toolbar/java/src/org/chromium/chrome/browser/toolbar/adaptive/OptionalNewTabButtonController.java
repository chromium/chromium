// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.content.Context;
import android.content.res.Configuration;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
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
    /** Minimum width to show the new tab button. */
    public static final int MIN_WIDTH_DP = 360;

    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final ButtonDataImpl mButtonData;
    /** Context used for fetching resources. */
    private final Context mContext;
    private final ObservableSupplier<TabCreatorManager> mTabCreatorManagerSupplier;
    /**
     * Source of the currently active tab and the incognito state.
     *
     * <p>Not using {@link IncognitoStateProvider} because ISP is created in the {@link
     * ToolbarManager} and not in {@link RootUiCoordinator}.
     *
     * <p>TODO(crbug.com/1185948): Make IncognitoStateProvider available in RootUiCooridnator.
     */
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();

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
            ObservableSupplier<TabCreatorManager> tabCreatorManagerSupplier,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mContext = context;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        mTabCreatorManagerSupplier = tabCreatorManagerSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;

        View.OnClickListener onClickListener = view -> {
            TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
            if (tabModelSelector == null) return;

            TabCreatorManager tabCreatorManager = mTabCreatorManagerSupplier.get();
            if (tabCreatorManager == null) return;

            boolean isIncognito = tabModelSelector.isIncognitoSelected();
            RecordUserAction.record("MobileTopToolbarOptionalButtonNewTab");
            tabCreatorManager.getTabCreator(isIncognito).launchNTP();
        };

        mButtonData = new ButtonDataImpl(/*canShow=*/false,
                AppCompatResources.getDrawable(mContext, R.drawable.new_tab_icon), onClickListener,
                R.string.button_new_tab, /*supportsTinting=*/true, /*iphCommandBuilder=*/null,
                /*isEnabled=*/true, AdaptiveToolbarButtonVariant.NEW_TAB);

        mScreenWidthDp = mContext.getResources().getConfiguration().screenWidthDp;
    }

    @Override
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(this);
        mObservers.clear();
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        if (mScreenWidthDp == configuration.screenWidthDp) {
            return;
        }
        mScreenWidthDp = configuration.screenWidthDp;
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
        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
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
        if (mScreenWidthDp < MIN_WIDTH_DP) return false;
        // On tablets a new tab button is shown on the tab strip.
        if (mScreenWidthDp >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP) return false;
        // Don't show on the NTP.
        if (UrlUtilities.isNTPUrl(tab.getUrl())) return false;
        return true;
    }
}
