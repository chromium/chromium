// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.function.Consumer;

/** Handle the events related to {@link OmniboxAction}. */
public class OmniboxActionDelegateImpl implements OmniboxActionDelegate {
    private final @NonNull Context mContext;
    private final @NonNull Consumer<String> mOpenUrlInExistingTabElseNewTabCb;
    private final @NonNull Runnable mOpenIncognitoTabCb;
    private final @NonNull Runnable mOpenPasswordSettingsCb;
    private final @NonNull Supplier<Tab> mTabSupplier;
    private final @Nullable Runnable mOpenQuickDeleteCb;

    public OmniboxActionDelegateImpl(
            @NonNull Context context,
            @NonNull Supplier<Tab> tabSupplier,
            @NonNull Consumer<String> openUrlInExistingTabElseNewTabCb,
            @NonNull Runnable openIncognitoTabCb,
            @NonNull Runnable openPasswordSettingsCb,
            @Nullable Runnable openQuickDeleteCb) {
        mContext = context;
        mTabSupplier = tabSupplier;
        mOpenUrlInExistingTabElseNewTabCb = openUrlInExistingTabElseNewTabCb;
        mOpenIncognitoTabCb = openIncognitoTabCb;
        mOpenPasswordSettingsCb = openPasswordSettingsCb;
        mOpenQuickDeleteCb = openQuickDeleteCb;
    }

    @Override
    public void handleClearBrowsingData() {
        if (QuickDeleteController.isQuickDeleteEnabled() && mOpenQuickDeleteCb != null) {
            mOpenQuickDeleteCb.run();
        } else {
            openSettingsPage(SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE);
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
    public boolean startActivity(@NonNull Intent intent) {
        try {
            if (IntentUtils.intentTargetsSelf(mContext, intent)) {
                IntentUtils.addTrustedIntentExtras(intent);
            }
            mContext.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException e) {
        }
        return false;
    }
}
