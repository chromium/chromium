// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import androidx.annotation.LayoutRes;
import androidx.annotation.MainThread;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerProperties.AddAccountRowProperties;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerProperties.ItemType;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * This class is responsible for setting up the account list's view and model and it serves as an
 * access point for users of the account picker MVC.
 */
@MainThread
public class AccountPickerCoordinator {
    /** Listener for account picker. */
    public interface Listener {
        /**
         * Notifies that the user has selected an account. TODO(crbug.com/40144553): Use
         * CoreAccountInfo instead of account's email as the first argument of the method.
         *
         * @param accountName The email of the selected account.
         */
        void onAccountSelected(String accountName);

        /** Notifies when the user clicked the "add account" button. */
        void addAccount();
    }

    private final AccountPickerMediator mMediator;

    /**
     * Constructs an AccountPickerCoordinator object.
     *
     * @param view The account list recycler view.
     * @param listener Listener to notify when an account is selected or the user wants to add an
     *     account.
     */
    AccountPickerCoordinator(
            RecyclerView view,
            Listener listener,
            @LayoutRes int accountRowLayout,
            @LayoutRes int newAccountRowLayout) {
        assert listener != null : "The argument AccountPickerCoordinator.Listener cannot be null!";

        MVCListAdapter.ModelList listModel = new MVCListAdapter.ModelList();

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(listModel);

        adapter.registerType(
                ItemType.ADD_ACCOUNT_ROW,
                new LayoutViewBuilder<>(newAccountRowLayout),
                new OnClickListenerViewBinder(AddAccountRowProperties.ON_CLICK_LISTENER));
        adapter.registerType(
                ItemType.EXISTING_ACCOUNT_ROW,
                new LayoutViewBuilder<>(accountRowLayout),
                new ExistingAccountRowViewBinder());

        view.setAdapter(adapter);
        mMediator = new AccountPickerMediator(view.getContext(), listModel, listener);
    }

    /** Destroys the resources used by the coordinator. */
    void destroy() {
        mMediator.destroy();
    }
}
