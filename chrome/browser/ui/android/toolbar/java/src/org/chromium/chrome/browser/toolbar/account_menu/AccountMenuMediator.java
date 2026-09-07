// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import android.app.Activity;
import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorUtil;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.ItemType;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.MenuItemProperties;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Mediator managing business logic and menu items for the Account Menu popup. */
@NullMarked
public class AccountMenuMediator {
    private final Context mContext;
    private final ModelList mModelList;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<@Nullable Profile> mProfileSupplier;
    private final Runnable mDismissCallback;

    public AccountMenuMediator(
            Context context,
            ModelList modelList,
            WindowAndroid windowAndroid,
            Supplier<@Nullable Profile> profileSupplier,
            Runnable dismissCallback) {
        mContext = context;
        mModelList = modelList;
        mWindowAndroid = windowAndroid;
        mProfileSupplier = profileSupplier;
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

        Profile profile = mProfileSupplier.get();
        if (profile != null && IncognitoUtils.isIncognitoModeEnabled(profile)) {
            mModelList.add(new ListItem(ItemType.DIVIDER, new PropertyModel()));
            int titleRes =
                    IncognitoUtils.shouldOpenIncognitoAsWindow()
                            ? R.string.menu_new_incognito_window
                            : R.string.menu_new_incognito_tab;
            mModelList.add(
                    new ListItem(
                            ItemType.MENU_ITEM,
                            MenuItemProperties.createModel(
                                    titleRes,
                                    R.drawable.ic_incognito_24dp,
                                    v -> {
                                        mDismissCallback.run();
                                        openIncognito();
                                    })));
        }
    }

    private void openAutofillSettings() {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(mContext, SettingsFragment.AUTOFILL_AND_PASSWORDS);
    }

    /** Opens a new Incognito window if supported, or a new Incognito tab otherwise. */
    private void openIncognito() {
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            Activity activity = mWindowAndroid.getActivity().get();
            if (activity != null) {
                MultiInstanceOrchestratorFactory.getInstance()
                        .createNewWindow(
                                activity,
                                /* isIncognito= */ true,
                                /* additionalIntentExtras= */ null,
                                /* startActivityOptions= */ null,
                                NewWindowAppSource.ACCOUNT_MENU);
            }
        } else {
            TabModelSelector selector = TabModelSelectorSupplier.getValueOrNullFrom(mWindowAndroid);
            if (selector != null) {
                TabCreator incognitoTabCreator =
                        selector.getTabCreatorManager().getTabCreator(/* incognito= */ true);
                TabCreatorUtil.launchNtp(incognitoTabCreator);
            }
        }
    }
}
