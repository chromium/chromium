// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetProperties.ViewState;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.List;

/** Mediator of the account picker bottom sheet in web sign-in flow. */
public class AccountPickerBottomSheetMediator
        implements AccountPickerCoordinator.Listener,
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
    private final @ViewState int mInitialViewState;
    // TODO(crbug.com/328747528): The web sign-in specific logic should be moved out of the bottom
    // sheet MVC.
    private final boolean mIsWebSignin;
    private final @SigninAccessPoint int mSigninAccessPoint;

    // TODO(crbug.com/41487829): Use CoreAccountInfo here instead.
    private @Nullable String mSelectedAccountEmail;
    private @Nullable String mDefaultAccountEmail;
    private @Nullable String mAddedAccountEmail;
    private boolean mAcceptedAccountManagement;

    private final PropertyObserver<PropertyKey> mModelPropertyChangedObserver;
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();

    AccountPickerBottomSheetMediator(
            WindowAndroid windowAndroid,
            AccountPickerDelegate accountPickerDelegate,
            Runnable onDismissButtonClicked,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            @AccountPickerLaunchMode int launchMode,
            boolean isWebSignin,
            @SigninAccessPoint int signinAccessPoint) {
        mWindowAndroid = windowAndroid;
        mActivity = windowAndroid.getActivity().get();
        mAccountPickerDelegate = accountPickerDelegate;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mActivity);
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mIsWebSignin = isWebSignin;
        mSigninAccessPoint = signinAccessPoint;

        switch (launchMode) {
            case AccountPickerLaunchMode.CHOOSE_ACCOUNT:
                mInitialViewState = ViewState.EXPANDED_ACCOUNT_LIST;
                break;
            case AccountPickerLaunchMode.DEFAULT:
                mInitialViewState = ViewState.COLLAPSED_ACCOUNT_LIST;
                break;
            default:
                throw new IllegalStateException(
                        "All values of AccountPickerLaunchMode should be handled.");
        }
        mModel =
                AccountPickerBottomSheetProperties.createModel(
                        this::onSelectedAccountClicked,
                        this::onContinueAsClicked,
                        view -> onDismissButtonClicked.run(),
                        accountPickerBottomSheetStrings);
        mModelPropertyChangedObserver =
                (source, propertyKey) -> {
                    if (AccountPickerBottomSheetProperties.VIEW_STATE == propertyKey) {
                        mBackPressStateChangedSupplier.set(shouldHandleBackPress());
                    }
                };
        mModel.addObserver(mModelPropertyChangedObserver);
        mProfileDataCache.addObserver(this);

        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAddedAccountEmail = null;
        initializeViewState(
                AccountUtils.getCoreAccountInfosIfFulfilledOrEmpty(
                        mAccountManagerFacade.getCoreAccountInfos()));
        mAccountManagerFacade.addObserver(this);
    }

    /** Implements {@link AccountPickerCoordinator.Listener}. */
    @Override
    public void onAccountSelected(String accountName) {
        setSelectedAccountName(accountName);
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            launchDeviceLockIfNeededAndSignIn();
        } else {
            // Clicking on one account in the account list when the account list is expanded
            // will collapse it to the selected account
            mModel.set(
                    AccountPickerBottomSheetProperties.VIEW_STATE,
                    ViewState.COLLAPSED_ACCOUNT_LIST);
        }
    }

    /** Implements {@link AccountPickerCoordinator.Listener}. */
    @Override
    public void addAccount() {
        SigninMetricsUtils.logAccountConsistencyPromoAction(
                AccountConsistencyPromoAction.ADD_ACCOUNT_STARTED, mSigninAccessPoint);

        if (mAccountPickerDelegate.canHandleAddAccount()) {
            mAccountPickerDelegate.addAccount();
            return;
        }

        final WindowAndroid.IntentCallback onAddAccountCompleted =
                (int resultCode, Intent data) -> {
                    if (resultCode != Activity.RESULT_OK) {
                        return;
                    }
                    SigninMetricsUtils.logAccountConsistencyPromoAction(
                            AccountConsistencyPromoAction.ADD_ACCOUNT_COMPLETED,
                            mSigninAccessPoint);
                    mAddedAccountEmail = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
                    onAccountSelected(mAddedAccountEmail);
                };
        mAccountManagerFacade.createAddAccountIntent(
                intent -> {
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
     * Called by the embedder when an account is added through the latter. Sign-in the just added
     * user.
     */
    public void onAccountAdded(@NonNull String accountEmail) {
        SigninMetricsUtils.logAccountConsistencyPromoAction(
                AccountConsistencyPromoAction.ADD_ACCOUNT_COMPLETED, mSigninAccessPoint);

        assert mAccountPickerDelegate.canHandleAddAccount();
        onAccountSelected(accountEmail);
    }

    /**
     * Notifies when user clicks the back-press button.
     *
     * @return true if the listener handles the back press, false if not.
     */
    @Override
    public boolean onBackPressed() {
        if (shouldHandleBackPress()) {
            mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, mInitialViewState);
            return true;
        }
        return false;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    /** Implements {@link AccountsChangeObserver}. */
    @Override
    public void onCoreAccountInfosChanged() {
        mAccountManagerFacade.getCoreAccountInfos().then(this::updateAccounts);
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        updateSelectedAccountData(accountEmail);
    }

    PropertyModel getModel() {
        return mModel;
    }

    void destroy() {
        mAccountPickerDelegate.onAccountPickerDestroy();
        mProfileDataCache.removeObserver(this);
        mAccountManagerFacade.removeObserver(this);
        mModel.removeObserver(mModelPropertyChangedObserver);
    }

    /** Switches the bottom sheet to the general error view that allows the user to try again. */
    public void switchToTryAgainView() {
        if (mAcceptedAccountManagement) {
            // Clear acceptance on failed signin, but do not clear |mAcceptedAccountManagement| so
            // that if the user chooses to retry, we don't confirm account management again.
            mAccountPickerDelegate.setUserAcceptedAccountManagement(false);
        }
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_GENERAL_ERROR);
    }

    /** Switches the bottom sheet to the auth error view that asks the user to sign in again. */
    public void switchToAuthErrorView() {
        if (mAcceptedAccountManagement) {
            // Clear acceptance on failed signin.
            mAcceptedAccountManagement = false;
            mAccountPickerDelegate.setUserAcceptedAccountManagement(false);
        }
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_AUTH_ERROR);
    }

    private boolean shouldHandleBackPress() {
        boolean hasExpandedAccountList =
                mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE)
                                == ViewState.EXPANDED_ACCOUNT_LIST
                        && mInitialViewState != ViewState.EXPANDED_ACCOUNT_LIST;
        boolean isOnConfirmManagement =
                mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE)
                        == ViewState.CONFIRM_MANAGEMENT;
        boolean isOnErrorScreen =
                mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE)
                                == ViewState.SIGNIN_GENERAL_ERROR
                        || mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE)
                                == ViewState.SIGNIN_AUTH_ERROR;
        return hasExpandedAccountList || isOnConfirmManagement || isOnErrorScreen;
    }

    private void initializeViewState(List<CoreAccountInfo> coreAccountInfos) {
        if (coreAccountInfos.isEmpty()) {
            // If all accounts disappeared, no matter if the account list initial state, we will go
            // to the zero account screen.
            setNoAccountState();
            return;
        }

        mDefaultAccountEmail = coreAccountInfos.get(0).getEmail();
        setSelectedAccountName(mDefaultAccountEmail);
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, mInitialViewState);
    }

    private void updateAccounts(List<CoreAccountInfo> coreAccountInfos) {
        if (coreAccountInfos.isEmpty()) {
            // If all accounts disappeared, no matter if the account list is collapsed or expanded,
            // we will go to the zero account screen.
            setNoAccountState();
            return;
        }

        mDefaultAccountEmail = coreAccountInfos.get(0).getEmail();
        @ViewState int viewState = mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE);
        if (viewState == ViewState.NO_ACCOUNTS) {
            // When a non-empty account list appears while it is currently zero-account screen,
            // we should change the screen to collapsed account list and set the selected account
            // to the first account of the account list
            setSelectedAccountName(mDefaultAccountEmail);
            mModel.set(
                    AccountPickerBottomSheetProperties.VIEW_STATE,
                    ViewState.COLLAPSED_ACCOUNT_LIST);
        } else if (viewState == ViewState.COLLAPSED_ACCOUNT_LIST
                && AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, mSelectedAccountEmail)
                        == null) {
            // When it is already collapsed account list, we update the selected account only
            // when the current selected account name is no longer in the new account list
            setSelectedAccountName(mDefaultAccountEmail);
        }
    }

    private void setNoAccountState() {
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.NO_ACCOUNTS);
        mSelectedAccountEmail = null;
        mDefaultAccountEmail = null;
        mModel.set(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA, null);
    }

    private void setSelectedAccountName(String accountName) {
        mSelectedAccountEmail = accountName;
        updateSelectedAccountData(mSelectedAccountEmail);
    }

    private void updateSelectedAccountData(String accountEmail) {
        if (TextUtils.equals(mSelectedAccountEmail, accountEmail)) {
            mModel.set(
                    AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA,
                    mProfileDataCache.getProfileDataOrDefault(accountEmail));
            mModel.set(
                    AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DOMAIN,
                    mAccountPickerDelegate.extractDomainName(accountEmail));
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
        @ViewState int viewState = mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE);
        if (viewState == ViewState.COLLAPSED_ACCOUNT_LIST) {
            launchDeviceLockIfNeededAndSignIn();
        } else if (viewState == ViewState.SIGNIN_GENERAL_ERROR) {
            if (mAcceptedAccountManagement) {
                // User already accepted account management and is re-trying login, so the
                // management status check & confirmation sheet can be skipped.
                signInAfterCheckingManagement();
            } else {
                launchDeviceLockIfNeededAndSignIn();
            }
        } else if (viewState == ViewState.NO_ACCOUNTS) {
            addAccount();
        } else if (viewState == ViewState.SIGNIN_AUTH_ERROR) {
            updateCredentials();
        } else if (viewState == ViewState.CONFIRM_MANAGEMENT) {
            mAcceptedAccountManagement = true;
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.CONFIRM_MANAGEMENT_ACCEPTED, mSigninAccessPoint);
            signInAfterCheckingManagement();
        }
    }

    private void launchDeviceLockIfNeededAndSignIn() {
        if (BuildInfo.getInstance().isAutomotive) {
            mDeviceLockActivityLauncher.launchDeviceLockActivity(
                    mActivity,
                    mSelectedAccountEmail,
                    /* requireDeviceLockReauthentication= */ true,
                    mWindowAndroid,
                    (resultCode, data) -> {
                        if (resultCode == Activity.RESULT_OK) {
                            signIn();
                        }
                    },
                    DeviceLockActivityLauncher.Source.ACCOUNT_PICKER);
        } else {
            signIn();
        }
    }

    private void signIn() {
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_IN_PROGRESS);
        CoreAccountInfo accountInfo =
                AccountUtils.findCoreAccountInfoByEmail(
                        mAccountManagerFacade.getCoreAccountInfos().getResult(),
                        mSelectedAccountEmail);
        // If the account is not available or disappears right after the user adds it, the sign-in
        // can't be done and a general error view with retry button is shown.
        if (accountInfo == null) {
            mModel.set(
                    AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_GENERAL_ERROR);
            return;
        }

        mAccountPickerDelegate.isAccountManaged(
                accountInfo,
                (Boolean isAccountManaged) -> {
                    if (isAccountManaged) {
                        SigninMetricsUtils.logAccountConsistencyPromoAction(
                                AccountConsistencyPromoAction.CONFIRM_MANAGEMENT_SHOWN,
                                mSigninAccessPoint);
                        mModel.set(
                                AccountPickerBottomSheetProperties.VIEW_STATE,
                                ViewState.CONFIRM_MANAGEMENT);
                    } else {
                        signInAfterCheckingManagement();
                    }
                });
    }

    private void signInAfterCheckingManagement() {
        if (mAcceptedAccountManagement) {
            mAccountPickerDelegate.setUserAcceptedAccountManagement(true);
        }
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_IN_PROGRESS);
        if (TextUtils.equals(mSelectedAccountEmail, mAddedAccountEmail)) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SIGNED_IN_WITH_ADDED_ACCOUNT, mSigninAccessPoint);
        } else if (TextUtils.equals(mSelectedAccountEmail, mDefaultAccountEmail)) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT,
                    mSigninAccessPoint);
        } else {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT,
                    mSigninAccessPoint);
        }

        if (mIsWebSignin) {
            SigninPreferencesManager.getInstance()
                    .clearWebSigninAccountPickerActiveDismissalCount();
        }

        CoreAccountInfo accountInfo =
                AccountUtils.findCoreAccountInfoByEmail(
                        mAccountManagerFacade.getCoreAccountInfos().getResult(),
                        mSelectedAccountEmail);
        // If the account is not available or disappears right after the user adds it, the sign-in
        // can't be done and a general error view with retry button is shown.
        if (accountInfo == null) {
            mModel.set(
                    AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_GENERAL_ERROR);
            return;
        }
        mAccountPickerDelegate.signIn(accountInfo, this);
    }

    private void updateCredentials() {
        final Callback<Boolean> onUpdateCredentialsCompleted =
                isSuccess -> {
                    if (isSuccess) {
                        mModel.set(
                                AccountPickerBottomSheetProperties.VIEW_STATE,
                                ViewState.COLLAPSED_ACCOUNT_LIST);
                    }
                };
        mAccountManagerFacade.updateCredentials(
                AccountUtils.createAccountFromName(mSelectedAccountEmail),
                mActivity,
                onUpdateCredentialsCompleted);
    }
}
