// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.MainThread;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerProperties.AddAccountRowProperties;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerProperties.ExistingAccountRowProperties;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerProperties.ItemType;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * The mediator of account picker handles all the signals from the outside world.
 *
 * It defines the business logic when the user selects or adds an account and updates the model.
 * This class has no visibility of the account picker view.
 */
class AccountPickerMediator implements AccountsChangeObserver, ProfileDataCache.Observer {
    private final MVCListAdapter.ModelList mListModel;
    private final AccountPickerCoordinator.Listener mAccountPickerListener;
    private final ProfileDataCache mProfileDataCache;
    private final AccountManagerFacade mAccountManagerFacade;

    @MainThread
    AccountPickerMediator(
            Context context,
            MVCListAdapter.ModelList listModel,
            AccountPickerCoordinator.Listener listener) {
        mListModel = listModel;
        mAccountPickerListener = listener;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();

        mAccountManagerFacade.addObserver(this);
        mProfileDataCache.addObserver(this);
        updateAccounts(
                AccountUtils.getCoreAccountInfosIfFulfilledOrEmpty(
                        mAccountManagerFacade.getCoreAccountInfos()));
    }

    /** Implements {@link AccountsChangeObserver}. */
    @Override
    public void onCoreAccountInfosChanged() {
        mAccountManagerFacade.getCoreAccountInfos().then(this::updateAccounts);
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        for (MVCListAdapter.ListItem item : mListModel) {
            if (item.type == AccountPickerProperties.ItemType.EXISTING_ACCOUNT_ROW) {
                PropertyModel model = item.model;
                boolean isProfileDataUpdated =
                        TextUtils.equals(
                                accountEmail,
                                model.get(ExistingAccountRowProperties.PROFILE_DATA)
                                        .getAccountEmail());
                if (isProfileDataUpdated) {
                    model.set(
                            ExistingAccountRowProperties.PROFILE_DATA,
                            mProfileDataCache.getProfileDataOrDefault(accountEmail));
                    break;
                }
            }
        }
    }

    /** Unregisters the observers used by the mediator. */
    @MainThread
    void destroy() {
        mProfileDataCache.removeObserver(this);
        mAccountManagerFacade.removeObserver(this);
    }

    private void updateAccounts(List<CoreAccountInfo> coreAccountInfos) {
        mListModel.clear();

        // Add an "existing account" row for each account
        final Callback<DisplayableProfileData> callback =
                profileData ->
                        mAccountPickerListener.onAccountSelected(profileData.getAccountEmail());
        for (CoreAccountInfo coreAccountInfo : coreAccountInfos) {
            PropertyModel model =
                    ExistingAccountRowProperties.createModel(
                            mProfileDataCache.getProfileDataOrDefault(coreAccountInfo.getEmail()),
                            /* isCurrentlySelected= */ false,
                            callback);
            mListModel.add(new MVCListAdapter.ListItem(ItemType.EXISTING_ACCOUNT_ROW, model));
        }

        // Add an "add account" row
        PropertyModel model =
                AddAccountRowProperties.createModel(mAccountPickerListener::addAccount);
        mListModel.add(new MVCListAdapter.ListItem(ItemType.ADD_ACCOUNT_ROW, model));
    }
}
