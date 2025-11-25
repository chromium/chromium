// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.Event;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetProperties.ViewState;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.List;
import java.util.Objects;

/** Mediator of the account picker bottom sheet in web sign-in flow. */
@NullMarked
public class AccountPickerBottomSheetMediator
        implements AccountPickerCoordinator.Listener,
                AccountPickerBottomSheetView.BackPressListener,
                AccountPickerDelegate.SigninStateController,
                AccountsChangeObserver,
                ProfileDataCache.Observer {

    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final IdentityManager mIdentityManager;
    private final SigninManager mSigninManager;
    private final AccountPickerDelegate mAccountPickerDelegate;
    private final @Nullable Runnable mRequestDisplayBottomSheet;
    private final Runnable mDismissBottomSheet;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final @ViewState int mInitialViewState;
    // TODO(crbug.com/328747528): The web sign-in specific logic should be moved out of the bottom
    // sheet MVC.
    private final boolean mIsWebSignin;
    private final @SigninAccessPoint int mSigninAccessPoint;
    private final ProfileDataCache mProfileDataCache;
    private final PropertyModel mModel;
    private final AccountManagerFacade mAccountManagerFacade;
    private final boolean mIsSeamlessSignin;

    private @Nullable SigninFlowTimestampsLogger mSigninTimestampsLogger;
    private @Nullable CoreAccountInfo mSelectedAccount;
    private @Nullable CoreAccountInfo mDefaultAccount;
    private @Nullable CoreAccountInfo mAddedAccount;
    // This field is used to save the added account email while the account info becomes available
    // in AccountManagerFacade for sign-in.
    private @Nullable String mPendingAddedAccountEmail;
    private boolean mAcceptedAccountManagement;

    private final PropertyObserver<PropertyKey> mModelPropertyChangedObserver;
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();

    static AccountPickerBottomSheetMediator create(
            WindowAndroid windowAndroid,
            IdentityManager identityManager,
            SigninManager signinManager,
            AccountPickerDelegate accountPickerDelegate,
            Runnable dismissBottomSheet,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            @AccountPickerLaunchMode int launchMode,
            boolean isWebSignin,
            @SigninAccessPoint int signinAccessPoint,
            @Nullable CoreAccountId accountId) {

        final @ViewState int initialView;
        switch (launchMode) {
            case AccountPickerLaunchMode.CHOOSE_ACCOUNT:
                initialView = ViewState.EXPANDED_ACCOUNT_LIST;
                break;
            case AccountPickerLaunchMode.DEFAULT:
                initialView = ViewState.COLLAPSED_ACCOUNT_LIST;
                break;
            case AccountPickerLaunchMode.SEAMLESS_SIGNIN:
                throw new IllegalStateException("Should be handled by createForSeamlessSignin()");
            default:
                throw new IllegalStateException(
                        "All values of AccountPickerLaunchMode should be handled.");
        }

        return new AccountPickerBottomSheetMediator(
                windowAndroid,
                identityManager,
                signinManager,
                accountPickerDelegate,
                /* requestDisplayBottomSheet= */ null,
                dismissBottomSheet,
                accountPickerBottomSheetStrings,
                deviceLockActivityLauncher,
                launchMode,
                initialView,
                isWebSignin,
                signinAccessPoint,
                accountId);
    }

    static AccountPickerBottomSheetMediator createForSeamlessSignin(
            WindowAndroid windowAndroid,
            IdentityManager identityManager,
            SigninManager signinManager,
            AccountPickerDelegate accountPickerDelegate,
            Runnable requestDisplayBottomSheet,
            Runnable dismissBottomSheet,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            @SigninAccessPoint int signinAccessPoint,
            CoreAccountId accountId) {
        return new AccountPickerBottomSheetMediator(
                windowAndroid,
                identityManager,
                signinManager,
                accountPickerDelegate,
                requestDisplayBottomSheet,
                dismissBottomSheet,
                accountPickerBottomSheetStrings,
                deviceLockActivityLauncher,
                AccountPickerLaunchMode.SEAMLESS_SIGNIN,
                ViewState.NONE,
                /* isWebSignin= */ false,
                signinAccessPoint,
                accountId);
    }

    private AccountPickerBottomSheetMediator(
            WindowAndroid windowAndroid,
            IdentityManager identityManager,
            SigninManager signinManager,
            AccountPickerDelegate accountPickerDelegate,
            @Nullable Runnable requestDisplayBottomSheet,
            Runnable dismissBottomSheet,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            @AccountPickerLaunchMode int launchMode,
            @ViewState int initialViewState,
            boolean isWebSignin,
            @SigninAccessPoint int signinAccessPoint,
            @Nullable CoreAccountId accountId) {
        mWindowAndroid = windowAndroid;
        mActivity = assertNonNull(windowAndroid.getActivity().get());
        mIdentityManager = identityManager;
        mSigninManager = signinManager;
        mAccountPickerDelegate = accountPickerDelegate;
        mRequestDisplayBottomSheet = requestDisplayBottomSheet;
        mDismissBottomSheet = dismissBottomSheet;
        mProfileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mActivity, identityManager);
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mInitialViewState = initialViewState;
        mIsWebSignin = isWebSignin;
        mSigninAccessPoint = signinAccessPoint;

        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        List<AccountInfo> accounts =
                AccountUtils.getAccountsIfFulfilledOrEmpty(mAccountManagerFacade.getAccounts());

        switch (launchMode) {
            case AccountPickerLaunchMode.CHOOSE_ACCOUNT:
            case AccountPickerLaunchMode.DEFAULT:
                mIsSeamlessSignin = false;
                mModel =
                        AccountPickerBottomSheetProperties.createModel(
                                this::onSelectedAccountClicked,
                                this::onContinueAsClicked,
                                view -> assertNonNull(dismissBottomSheet).run(),
                                accountPickerBottomSheetStrings);
                initializeAccountPickerAccountAndModel(accounts, accountId);
                break;
            case AccountPickerLaunchMode.SEAMLESS_SIGNIN:
                assert requestDisplayBottomSheet != null
                        : "Seamless sign-in requires a display request callback.";
                assert accountId != null : "Seamless sign-in requires an initial account ID.";
                mIsSeamlessSignin = true;
                mModel =
                        AccountPickerBottomSheetProperties.createModelForSeamlessSignin(
                                this::onContinueAsClicked, accountPickerBottomSheetStrings);
                if (accounts.isEmpty()) {
                    // TODO(crbug.com/437038737): Handle missing account during seamless sign-in
                    // initialization.
                    throw new UnsupportedOperationException(
                            "Account being unavailable during initialization is not supported.");
                }
                mDefaultAccount =
                        assertNonNull(
                                AccountUtils.findAccountByGaiaId(accounts, accountId.getId()));
                setSelectedAccount(mDefaultAccount);
                break;
            default:
                throw new IllegalStateException(
                        "All values of AccountPickerLaunchMode should be handled.");
        }

        mModelPropertyChangedObserver =
                (source, propertyKey) -> {
                    if (AccountPickerBottomSheetProperties.VIEW_STATE == propertyKey) {
                        mBackPressStateChangedSupplier.set(shouldHandleBackPress());
                    }
                };
        mModel.addObserver(mModelPropertyChangedObserver);
        mProfileDataCache.addObserver(this);
        mAccountManagerFacade.addObserver(this);
    }

    /** Implements {@link AccountPickerCoordinator.Listener}. */
    @Override
    public void onAccountSelected(CoreAccountInfo account) {
        assert !mIsSeamlessSignin
                : "Account selection is not supported in the seamless sign-in flow.";
        if (mPendingAddedAccountEmail != null) {
            // If another account is selected before the added account is available in account
            // manager facade then clear the pending added account email so that it doesn't get
            // selected automatically in #updateAccounts().
            mPendingAddedAccountEmail = null;
        }
        setSelectedAccount(account);
        launchDeviceLockIfNeededAndSignIn();
    }

    /** Implements {@link AccountPickerCoordinator.Listener}. */
    @Override
    public void addAccount() {
        assert !mIsSeamlessSignin
                : "Adding an account is not supported in the seamless sign-in flow.";
        SigninMetricsUtils.logAccountConsistencyPromoAction(
                AccountConsistencyPromoAction.ADD_ACCOUNT_STARTED, mSigninAccessPoint);

        if (mAccountPickerDelegate.canHandleAddAccount()) {
            mAccountPickerDelegate.addAccount();
            return;
        }

        final WindowAndroid.IntentCallback onAddAccountCompleted =
                (int resultCode, @Nullable Intent data) -> {
                    @Nullable String addedAccountEmail =
                            data == null
                                    ? null
                                    : data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
                    if (resultCode != Activity.RESULT_OK || addedAccountEmail == null) {
                        return;
                    }
                    onAccountAddedInternal(addedAccountEmail);
                };
        mAccountManagerFacade.createAddAccountIntent(
                null,
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
        assert !mIsSeamlessSignin
                : "Signing in an added account is not supported in the seamless sign-in flow.";
        assert mAccountPickerDelegate.canHandleAddAccount();
        onAccountAddedInternal(accountEmail);
    }

    private void onAccountAddedInternal(String accountEmail) {
        SigninMetricsUtils.logAccountConsistencyPromoAction(
                AccountConsistencyPromoAction.ADD_ACCOUNT_COMPLETED, mSigninAccessPoint);

        var accounts =
                AccountUtils.getAccountsIfFulfilledOrEmpty(mAccountManagerFacade.getAccounts());
        mAddedAccount = AccountUtils.findAccountByEmail(accounts, accountEmail);
        if (mAddedAccount == null) {
            // #updateAccounts() will call #onAccountSelected() when the account is available in
            // AccountManagerFacade.
            mPendingAddedAccountEmail = accountEmail;
            return;
        }
        onAccountSelected(mAddedAccount);
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
        if (mIsSeamlessSignin) {
            // TODO(crbug.com/460030880): Decouple the 'Confirm Management' cancel button from this
            // general back press handler using a dedicated property and callback
            mDismissBottomSheet.run();
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
        if (mIsSeamlessSignin) {
            // TODO(crbug.com/437038737): Handle selected account disappearance in seamless sign-in
            // when bottom sheet is shown.
            throw new UnsupportedOperationException(
                    "Account changes are not yet supported in the seamless sign-in flow.");
        }
        mAccountManagerFacade.getAccounts().then(this::updateAccounts);
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        updateSelectedAccountData(accountEmail);
    }

    /** Implements {@link AccountPickerDelegate.SigninStateController controller}. */
    @Override
    public void showGenericError() {
        assertNonNull(mSigninTimestampsLogger).recordTimestamp(Event.SIGNIN_ABORTED);
        // Switches the bottom sheet to the general error view that allows the user to try again.
        if (mAcceptedAccountManagement) {
            // Clear acceptance on failed signin, but do not clear |mAcceptedAccountManagement| so
            // that if the user chooses to retry, we don't confirm account management again.
            mSigninManager.setUserAcceptedAccountManagement(false);
        }
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_GENERAL_ERROR);
        if (mIsSeamlessSignin) {
            assumeNonNull(mRequestDisplayBottomSheet).run();
        }
    }

    /** Implements {@link AccountPickerDelegate.SigninStateController controller}. */
    @Override
    public void showAuthError() {
        assert !mIsSeamlessSignin
                : "Showing auth error is not supported for seamless sign-in flow.";
        // Switches the bottom sheet to the auth error view that asks the user to reauth.
        assertNonNull(mSigninTimestampsLogger).recordTimestamp(Event.SIGNIN_ABORTED);
        if (mAcceptedAccountManagement) {
            // Clear acceptance on failed signin.
            mAcceptedAccountManagement = false;
            mSigninManager.setUserAcceptedAccountManagement(false);
        }
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_AUTH_ERROR);
    }

    /** Implements {@link AccountPickerDelegate.ResultHandler}. */
    @Override
    public void onSigninComplete() {
        assertNonNull(mSigninTimestampsLogger).recordTimestamp(Event.SIGNIN_COMPLETED);
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

    private boolean shouldHandleBackPress() {
        if (mIsSeamlessSignin) {
            // Seamless sign-in always dismisses the bottom sheet on back press
            return false;
        }
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

    private void initializeAccountPickerAccountAndModel(
            List<AccountInfo> accounts, @Nullable CoreAccountId accountId) {
        if (accounts.isEmpty()) {
            // If all accounts disappeared, no matter if the account list initial state, we will go
            // to the zero account screen.
            setNoAccountState();
            return;
        }

        if (accountId != null) {
            mDefaultAccount =
                    assertNonNull(AccountUtils.findAccountByGaiaId(accounts, accountId.getId()));
            setSelectedAccount(mDefaultAccount);
            mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, mInitialViewState);
            return;
        }
        mDefaultAccount = accounts.get(0);
        setSelectedAccount(mDefaultAccount);
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, mInitialViewState);
    }

    private void updateAccounts(List<AccountInfo> accounts) {
        if (accounts.isEmpty()) {
            // If all accounts disappeared, no matter if the account list is collapsed or expanded,
            // we will go to the zero account screen.
            setNoAccountState();
            return;
        }

        @Nullable AccountInfo pendingAddedAccount =
                mPendingAddedAccountEmail == null
                        ? null
                        : AccountUtils.findAccountByEmail(accounts, mPendingAddedAccountEmail);
        if (pendingAddedAccount != null) {
            mPendingAddedAccountEmail = null;
            mAddedAccount = pendingAddedAccount;
            onAccountSelected(mAddedAccount);
            return;
        }

        mDefaultAccount = accounts.get(0);
        mSelectedAccount =
                mSelectedAccount == null
                        ? null
                        : AccountUtils.findAccountByEmail(accounts, mSelectedAccount.getEmail());
        @ViewState int viewState = mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE);
        if (viewState == ViewState.NO_ACCOUNTS) {
            // When a non-empty account list appears while it is currently zero-account screen,
            // we should change the screen to collapsed account list and set the selected account
            // to the first account of the account list
            setSelectedAccount(mDefaultAccount);
            mModel.set(
                    AccountPickerBottomSheetProperties.VIEW_STATE,
                    ViewState.COLLAPSED_ACCOUNT_LIST);
        } else if (viewState == ViewState.COLLAPSED_ACCOUNT_LIST && mSelectedAccount == null) {
            // When it is already collapsed account list, we update the selected account only
            // when the current selected account name is no longer in the new account list
            setSelectedAccount(mDefaultAccount);
        }
    }

    private void setNoAccountState() {
        assert !mIsSeamlessSignin;
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.NO_ACCOUNTS);
        mSelectedAccount = null;
        mDefaultAccount = null;
        mModel.set(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA, null);
    }

    private void setSelectedAccount(CoreAccountInfo account) {
        mSelectedAccount = account;
        updateSelectedAccountData(account.getEmail());
    }

    private void updateSelectedAccountData(String accountEmail) {
        if (mSelectedAccount != null
                && TextUtils.equals(mSelectedAccount.getEmail(), accountEmail)) {
            mModel.set(
                    AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA,
                    mProfileDataCache.getProfileDataOrDefault(accountEmail));
            mModel.set(
                    AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DOMAIN,
                    mSigninManager.extractDomainName(accountEmail));
        }
    }

    /**
     * Callback for the PropertyKey
     * {@link AccountPickerBottomSheetProperties#ON_SELECTED_ACCOUNT_CLICKED}.
     */
    private void onSelectedAccountClicked() {
        assert !mIsSeamlessSignin;

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
            assert !mIsSeamlessSignin;
            launchDeviceLockIfNeededAndSignIn();
        } else if (viewState == ViewState.SIGNIN_GENERAL_ERROR) {
            if (mAcceptedAccountManagement) {
                // User already accepted account management and is re-trying login, so the
                // management status check & confirmation sheet can be skipped.
                startSigninTimestampLogging();
                signInAfterCheckingManagement();
            } else {
                launchDeviceLockIfNeededAndSignIn();
            }
        } else if (viewState == ViewState.NO_ACCOUNTS) {
            assert !mIsSeamlessSignin;
            addAccount();
        } else if (viewState == ViewState.SIGNIN_AUTH_ERROR) {
            assert !mIsSeamlessSignin;
            updateCredentials();
        } else if (viewState == ViewState.CONFIRM_MANAGEMENT) {
            mAcceptedAccountManagement = true;
            assertNonNull(mSigninTimestampsLogger).onManagementNoticeAccepted();
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.CONFIRM_MANAGEMENT_ACCEPTED, mSigninAccessPoint);
            signInAfterCheckingManagement();
        }
    }

    void launchDeviceLockIfNeededAndSignIn() {
        if (DeviceInfo.isAutomotive()) {
            mDeviceLockActivityLauncher.launchDeviceLockActivity(
                    mActivity,
                    CoreAccountInfo.getEmailFrom(mSelectedAccount),
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
        // If the account is not available or disappears right after the user adds it, the sign-in
        // can't be done and a general error view with retry button is shown.
        if (mSelectedAccount == null) {
            if (mIsSeamlessSignin) {
                // TODO(crbug.com/437038737): Confirm if error screen should be shown or sign-in
                // should be abandoned.
                throw new UnsupportedOperationException(
                        "Account being unavailable during sign-in is not supported.");
            }
            mModel.set(
                    AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_GENERAL_ERROR);
            return;
        }

        startSigninTimestampLogging();
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_IN_PROGRESS);
        mSigninManager.isAccountManaged(
                mSelectedAccount,
                (Boolean isAccountManaged) -> {
                    assertNonNull(mSigninTimestampsLogger)
                            .recordTimestamp(Event.MANAGEMENT_STATUS_LOADED);
                    if (isAccountManaged) {
                        SigninMetricsUtils.logAccountConsistencyPromoAction(
                                AccountConsistencyPromoAction.CONFIRM_MANAGEMENT_SHOWN,
                                mSigninAccessPoint);
                        shownConfirmManagementSheet();
                        assertNonNull(mSigninTimestampsLogger).onManagementNoticeShown();
                    } else {
                        signInAfterCheckingManagement();
                    }
                });
    }

    private void shownConfirmManagementSheet() {
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.CONFIRM_MANAGEMENT);
        if (mIsSeamlessSignin) {
            assumeNonNull(mRequestDisplayBottomSheet).run();
        }
    }

    private void signInAfterCheckingManagement() {
        // If the account is not available or disappears right after the user adds it, the sign-in
        // can't be done and a general error view with retry button is shown.
        if (mSelectedAccount == null) {
            if (mIsSeamlessSignin) {
                // TODO(crbug.com/437038737): Confirm if error screen should be shown or sign-in
                // should be abandoned.
                throw new UnsupportedOperationException(
                        "Account being unavailable during sign-in is not supported.");
            }
            mModel.set(
                    AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_GENERAL_ERROR);
            return;
        }

        if (mAcceptedAccountManagement) {
            mSigninManager.setUserAcceptedAccountManagement(true);
        }
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_IN_PROGRESS);

        if (Objects.equals(mSelectedAccount, mAddedAccount)) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SIGNED_IN_WITH_ADDED_ACCOUNT, mSigninAccessPoint);
        } else if (Objects.equals(mSelectedAccount, mDefaultAccount)) {
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

        // TODO(crbug.com/435381574): Investigate whether this sign-out is still needed, and remove
        // it if possible.
        if (mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            mAccountPickerDelegate.onSignoutBeforeSignin();
            mSigninManager.signOut(SignoutReason.SIGNIN_RETRIGGERED);
        }

        CoreAccountInfo selectedAccount = mSelectedAccount;
        mSigninManager.signin(
                selectedAccount,
                mSigninAccessPoint,
                new SigninManager.SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        mAccountPickerDelegate.onSignInComplete(
                                selectedAccount, AccountPickerBottomSheetMediator.this);
                    }

                    @Override
                    public void onSignInAborted() {
                        showGenericError();
                    }
                });
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
        assertNonNull(mSelectedAccount);
        mAccountManagerFacade.updateCredentials(
                CoreAccountInfo.getAndroidAccountFrom(mSelectedAccount),
                mActivity,
                onUpdateCredentialsCompleted);
    }

    private void startSigninTimestampLogging() {
        @FlowVariant String flowVariant = mAccountPickerDelegate.getSigninFlowVariant();
        mSigninTimestampsLogger = SigninFlowTimestampsLogger.startLogging(flowVariant);
    }
}
