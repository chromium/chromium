// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.DeviceLockActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator.EntryPoint;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetProperties.ViewState;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.List;

/**
 * Mediator of the account picker bottom sheet in web sign-in flow.
 */
class AccountPickerBottomSheetMediator implements AccountPickerCoordinator.Listener,
                                                  AccountPickerBottomSheetView.BackPressListener,
                                                  AccountsChangeObserver,
                                                  ProfileDataCache.Observer {
    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final AccountPickerDelegate mAccountPickerDelegate;
    private final ProfileDataCache mProfileDataCache;
    private final PropertyModel mModel;
    private final AccountManagerFacade mAccountManagerFacade;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;

    private @Nullable String mSelectedAccountName;
    private @Nullable String mDefaultAccountName;
    private @Nullable String mAddedAccountName;

    private final PropertyObserver<PropertyKey> mModelPropertyChangedObserver;
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();

    AccountPickerBottomSheetMediator(WindowAndroid windowAndroid,
            AccountPickerDelegate accountPickerDelegate, Runnable onDismissButtonClicked,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
            DeviceLockActivityLauncher deviceLockActivityLauncher) {
        mWindowAndroid = windowAndroid;
        mActivity = windowAndroid.getActivity().get();
        mAccountPickerDelegate = accountPickerDelegate;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mActivity);
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;

        mModel = AccountPickerBottomSheetProperties.createModel(this::onSelectedAccountClicked,
                this::onContinueAsClicked,
                view
                -> onDismissButtonClicked.run(),
                accountPickerDelegate.getEntryPoint(), accountPickerBottomSheetStrings);
        mModelPropertyChangedObserver = (source, propertyKey) -> {
            if (AccountPickerBottomSheetProperties.VIEW_STATE == propertyKey) {
                mBackPressStateChangedSupplier.set(
                        mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE)
                        == ViewState.EXPANDED_ACCOUNT_LIST);
            }
        };
        mModel.addObserver(mModelPropertyChangedObserver);
        mProfileDataCache.addObserver(this);

        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(this);
        mAddedAccountName = null;
        updateAccounts(
                AccountUtils.getAccountsIfFulfilledOrEmpty(mAccountManagerFacade.getAccounts()));
    }

    /**
     * Notifies that the user has selected an account.
     *
     * @param accountName The email of the selected account.
     *
     * TODO(https://crbug.com/1115965): Use CoreAccountInfo instead of account's email
     * as the first argument of the method.
     */
    @Override
    public void onAccountSelected(String accountName) {
        // Clicking on one account in the account list when the account list is expanded
        // will collapse it to the selected account
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.COLLAPSED_ACCOUNT_LIST);
        setSelectedAccountName(accountName);
    }

    /**
     * Notifies when the user clicked the "add account" button.
     */
    @Override
    public void addAccount() {
        SigninMetricsUtils.logAccountConsistencyPromoAction(
                AccountConsistencyPromoAction.ADD_ACCOUNT_STARTED);
        final WindowAndroid.IntentCallback onAddAccountCompleted =
                (int resultCode, Intent data) -> {
            if (resultCode != Activity.RESULT_OK) {
                return;
            }
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.ADD_ACCOUNT_COMPLETED);
            mAddedAccountName = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
            onAccountSelected(mAddedAccountName);
        };
        mAccountManagerFacade.createAddAccountIntent(intent -> {
            if (intent == null) {
                // AccountManagerFacade couldn't create intent, use SigninUtils to open
                // settings instead.
                SigninUtils.openSettingsForAllAccounts(mActivity);
                return;
            }

            mWindowAndroid.showIntent(intent, onAddAccountCompleted, null);
        });
    }

    /**
     * Notifies when user clicks the back-press button.
     *
     * @return true if the listener handles the back press, false if not.
     */
    @Override
    public boolean onBackPressed() {
        @ViewState
        int viewState = mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE);
        if (viewState == ViewState.EXPANDED_ACCOUNT_LIST) {
            mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE,
                    ViewState.COLLAPSED_ACCOUNT_LIST);
            return true;
        }
        return false;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    /**
     * Implements {@link AccountsChangeObserver}.
     */
    @Override
    public void onAccountsChanged() {
        mAccountManagerFacade.getAccounts().then(this::updateAccounts);
    }

    /**
     * Implements {@link ProfileDataCache.Observer}.
     */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        updateSelectedAccountData(accountEmail);
    }

    PropertyModel getModel() {
        return mModel;
    }

    boolean isEntryPointWebSignin() {
        return mModel.get(AccountPickerBottomSheetProperties.ENTRY_POINT) == EntryPoint.WEB_SIGNIN;
    }

    void destroy() {
        mAccountPickerDelegate.destroy();
        mProfileDataCache.removeObserver(this);
        mAccountManagerFacade.removeObserver(this);
        mModel.removeObserver(mModelPropertyChangedObserver);
    }

    public void setTryAgainBottomSheetView() {
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_GENERAL_ERROR);
    }

    private void updateAccounts(List<Account> accounts) {
        if (accounts.isEmpty()) {
            // If all accounts disappeared, no matter if the account list is collapsed or expanded,
            // we will go to the zero account screen.
            mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.NO_ACCOUNTS);
            mSelectedAccountName = null;
            mDefaultAccountName = null;
            mModel.set(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA, null);
            return;
        }

        mDefaultAccountName = accounts.get(0).name;
        @ViewState
        int viewState = mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE);
        if (viewState == ViewState.NO_ACCOUNTS) {
            // When a non-empty account list appears while it is currently zero-account screen,
            // we should change the screen to collapsed account list and set the selected account
            // to the first account of the account list
            setSelectedAccountName(mDefaultAccountName);
            mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE,
                    ViewState.COLLAPSED_ACCOUNT_LIST);
        } else if (viewState == ViewState.COLLAPSED_ACCOUNT_LIST
                && AccountUtils.findAccountByName(accounts, mSelectedAccountName) == null) {
            // When it is already collapsed account list, we update the selected account only
            // when the current selected account name is no longer in the new account list
            setSelectedAccountName(mDefaultAccountName);
        }
    }

    private void setSelectedAccountName(String accountName) {
        mSelectedAccountName = accountName;
        updateSelectedAccountData(mSelectedAccountName);
    }

    private void updateSelectedAccountData(String accountEmail) {
        if (TextUtils.equals(mSelectedAccountName, accountEmail)) {
            mModel.set(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA,
                    mProfileDataCache.getProfileDataOrDefault(accountEmail));
        }
    }

    /**
     * Callback for the PropertyKey
     * {@link AccountPickerBottomSheetProperties#ON_SELECTED_ACCOUNT_CLICKED}.
     */
    private void onSelectedAccountClicked() {
        // Clicking on the selected account when the account list is collapsed will expand the
        // account list and make the account list visible
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.EXPANDED_ACCOUNT_LIST);
    }

    /**
     * Callback for the PropertyKey
     * {@link AccountPickerBottomSheetProperties#ON_CONTINUE_AS_CLICKED}.
     */
    private void onContinueAsClicked() {
        @ViewState
        int viewState = mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE);
        if (viewState == ViewState.COLLAPSED_ACCOUNT_LIST
                || viewState == ViewState.SIGNIN_GENERAL_ERROR) {
            if (BuildInfo.getInstance().isAutomotive) {
                mDeviceLockActivityLauncher.launchDeviceLockActivity(mActivity, true,
                        mSelectedAccountName, mWindowAndroid, (resultCode, data) -> {
                            if (resultCode == Activity.RESULT_OK) {
                                signIn();
                            }
                        });
            } else {
                signIn();
            }
        } else if (viewState == ViewState.NO_ACCOUNTS) {
            addAccount();
        } else if (viewState == ViewState.SIGNIN_AUTH_ERROR) {
            updateCredentials();
        }
    }

    private void signIn() {
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_IN_PROGRESS);
        if (TextUtils.equals(mSelectedAccountName, mAddedAccountName)) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SIGNED_IN_WITH_ADDED_ACCOUNT);
        } else if (TextUtils.equals(mSelectedAccountName, mDefaultAccountName)) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT);
        } else {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT);
        }

        if (isEntryPointWebSignin()) {
            SigninPreferencesManager.getInstance()
                    .clearWebSigninAccountPickerActiveDismissalCount();
        }

        mAccountPickerDelegate.signIn(mSelectedAccountName, this::onSigninFailed);
    }

    private void onSigninFailed(GoogleServiceAuthError error) {
        final @AccountConsistencyPromoAction int promoAction;
        final @ViewState int newViewState;
        if (error.getState() == State.INVALID_GAIA_CREDENTIALS) {
            promoAction = AccountConsistencyPromoAction.AUTH_ERROR_SHOWN;
            newViewState = ViewState.SIGNIN_AUTH_ERROR;
        } else {
            promoAction = AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN;
            newViewState = ViewState.SIGNIN_GENERAL_ERROR;
        }
        SigninMetricsUtils.logAccountConsistencyPromoAction(promoAction);
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, newViewState);
    }

    private void updateCredentials() {
        final Callback<Boolean> onUpdateCredentialsCompleted = isSuccess -> {
            if (isSuccess) {
                mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE,
                        ViewState.COLLAPSED_ACCOUNT_LIST);
            }
        };
        mAccountManagerFacade.updateCredentials(
                AccountUtils.createAccountFromName(mSelectedAccountName), mActivity,
                onUpdateCredentialsCompleted);
    }
}
