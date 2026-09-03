// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.ItemType;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.MenuItemProperties;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/** Mediator managing business logic and menu items for the Account Menu popup. */
@NullMarked
public class AccountMenuMediator {
    private final Context mContext;
    private final ModelList mModelList;
    private final Runnable mDismissCallback;

    public AccountMenuMediator(Context context, ModelList modelList, Runnable dismissCallback) {
        mContext = context;
        mModelList = modelList;
        mDismissCallback = dismissCallback;

        updateMenuItems();
    }

    /** Populates the menu action items. */
    public void updateMenuItems() {
        mModelList.clear();
        mModelList.add(
                new ListItem(
                        ItemType.MENU_ITEM,
                        MenuItemProperties.createModel(
                                R.string.menu_passwords_and_autofill,
                                R.drawable.ic_password_manager_24dp,
                                v -> {
                                    mDismissCallback.run();
                                    openAutofillSettings();
                                })));
    }

    private void openAutofillSettings() {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(mContext, SettingsFragment.AUTOFILL_AND_PASSWORDS);
    }
}
