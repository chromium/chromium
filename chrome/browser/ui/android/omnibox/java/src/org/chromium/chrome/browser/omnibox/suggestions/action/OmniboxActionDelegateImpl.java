// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.ActivityState;
import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

import java.util.function.Consumer;
import java.util.function.Supplier;

/** Handle the events related to {@link OmniboxAction}. */
@NullMarked
public class OmniboxActionDelegateImpl implements OmniboxActionDelegate {
    private final Context mContext;
    private final Consumer<String> mOpenUrlInExistingTabElseNewTabCb;
    private final Runnable mOpenIncognitoTabCb;
    private final Runnable mOpenPasswordSettingsCb;
    private final Supplier<@Nullable Tab> mTabSupplier;
    private final @Nullable Runnable mOpenQuickDeleteCb;
    private final Supplier<TabWindowManager> mTabWindowManagerSupplier;
    private final Callback<Tab> mBringTabToFrontCallback;

    public OmniboxActionDelegateImpl(
            Context context,
            Supplier<@Nullable Tab> tabSupplier,
            Consumer<String> openUrlInExistingTabElseNewTabCb,
            Runnable openIncognitoTabCb,
            Runnable openPasswordSettingsCb,
            @Nullable Runnable openQuickDeleteCb,
            Supplier<TabWindowManager> tabWindowManagerSupplier,
            Callback<Tab> bringTabToFrontCallback) {
        mContext = context;
        mTabSupplier = tabSupplier;
        mOpenUrlInExistingTabElseNewTabCb = openUrlInExistingTabElseNewTabCb;
        mOpenIncognitoTabCb = openIncognitoTabCb;
        mOpenPasswordSettingsCb = openPasswordSettingsCb;
        mOpenQuickDeleteCb = openQuickDeleteCb;
        mTabWindowManagerSupplier = tabWindowManagerSupplier;
        mBringTabToFrontCallback = bringTabToFrontCallback;
    }

    @Override
    public void handleClearBrowsingData() {
        if (mOpenQuickDeleteCb != null) {
            mOpenQuickDeleteCb.run();
        } else {
            openSettingsPage(SettingsFragment.CLEAR_BROWSING_DATA);
        }
    }

    @Override
    public void openIncognitoTab() {
        mOpenIncognitoTabCb.run();
    }

    @Override
    public void openPasswordManager() {
        mOpenPasswordSettingsCb.run();
    }

    @Override
    public void openSettingsPage(@SettingsFragment int fragment) {
        SettingsNavigationFactory.createSettingsNavigation().startSettings(mContext, fragment);
    }

    @Override
    public boolean isIncognito() {
        var tab = mTabSupplier.get();
        return (tab != null && tab.isIncognito());
    }

    @Override
    public void loadPageInCurrentTab(String url) {
        var tab = mTabSupplier.get();
        if (tab != null && tab.isUserInteractable()) {
            tab.loadUrl(new LoadUrlParams(url));
        } else {
            mOpenUrlInExistingTabElseNewTabCb.accept(url);
        }
    }

    @Override
    public boolean startActivity(Intent intent) {
        try {
            if (IntentUtils.intentTargetsSelf(intent)) {
                IntentUtils.addTrustedIntentExtras(intent);
            }
            mContext.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException e) {
        }
        return false;
    }

    @Override
    public boolean switchToTab(int tabId) {
        TabWindowManager tabWindowManager = mTabWindowManagerSupplier.get();
        if (tabWindowManager == null) return false;

        Tab tab = tabWindowManager.getTabById(tabId);
        if (tab == null) return false;

        // When invoked directly from a browser, we want to trigger switch to tab animation.
        // If invoked from other activities, ex. searchActivity, we do not need to trigger the
        // animation since Android will show the animation for switching apps.
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null) return false;
        if (windowAndroid.getActivityState() == ActivityState.STOPPED
                || windowAndroid.getActivityState() == ActivityState.DESTROYED) {
            mBringTabToFrontCallback.onResult(tab);
            return true;
        }

        TabModel tabModel = tabWindowManager.getTabModelForTab(tab);
        if (tabModel == null) return false;

        int tabIndex = TabModelUtils.getTabIndexById(tabModel, tabId);
        // In the event the user deleted the tab as part during the interaction with the
        // Omnibox, reject the switch to tab action.
        if (tabIndex == TabModel.INVALID_TAB_INDEX) return false;
        tabModel.setIndex(tabIndex, TabSelectionType.FROM_OMNIBOX);
        return true;
    }
}
