// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;

import androidx.annotation.AnyThread;
import androidx.annotation.IdRes;
import androidx.annotation.MainThread;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.AuthException;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;

import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.stream.Collectors;

/** FakeAccountManagerFacade is an {@link AccountManagerFacade} stub intended for testing. */
public class FakeAccountManagerFacade implements AccountManagerFacade {
    /**
     * Can be closed to unblock updates to the list of accounts. See {@link
     * FakeAccountManagerFacade#blockGetAccounts}.
     */
    public class UpdateBlocker implements AutoCloseable {
        /** Use {@link FakeAccountManagerFacade#blockGetAccounts} to instantiate. */
        private UpdateBlocker() {}

        @Override
        public void close() {
            unblockGetAccounts();
        }
    }

    private static final String TAG = "FakeAccountManager";

    /** An {@link Activity} stub to test add account flow. */
    public static final class AddAccountActivityStub extends Activity {
        public static final @IdRes int OK_BUTTON_ID = R.id.ok_button;
        public static final @IdRes int CANCEL_BUTTON_ID = R.id.cancel_button;

        @Override
        public void onCreate(@Nullable Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            setContentView(R.layout.test_add_account_layout);
            Button okButton = findViewById(OK_BUTTON_ID);
            okButton.setOnClickListener(v -> addAccount());
            Button cancelButton = findViewById(CANCEL_BUTTON_ID);
            cancelButton.setOnClickListener(v -> cancel());
        }

        private void addAccount() {
            Intent data = new Intent();
            FakeAccountManagerFacade accountManagerFacade =
                    (FakeAccountManagerFacade) AccountManagerFacadeProvider.getInstance();
            AccountInfo addedAccount = accountManagerFacade.mAccountToAdd;

            data.putExtra(
                    AccountManager.KEY_ACCOUNT_NAME, CoreAccountInfo.getEmailFrom(addedAccount));
            if (addedAccount != null) {
                ((FakeAccountManagerFacade) AccountManagerFacadeProvider.getInstance())
                        .addAccount(addedAccount);
            }
            accountManagerFacade.mAccountToAdd = null;
            setResult(RESULT_OK, data);
            finish();
        }

        private void cancel() {
            FakeAccountManagerFacade accountManagerFacade =
                    (FakeAccountManagerFacade) AccountManagerFacadeProvider.getInstance();
            accountManagerFacade.mAccountToAdd = null;
            setResult(RESULT_CANCELED, null);
            finish();
        }
    }

    private final List<AccountsChangeObserver> mObservers = new CopyOnWriteArrayList<>();

    // `mAccountHolders` can be read from non-UI threads (this is used by `getAccessToken`), but
    // should only be changed from the UI thread to guarantee the consistency of the observed state.
    // TODO(crbug.com/429143376): Deprecate AccountHolder after sMigrateAccountManagerDelegate is
    // enabled by default.
    private final Set<AccountHolder> mAccountHolders =
            Collections.synchronizedSet(new LinkedHashSet<>());

    private final Set<FakePlatformAccount> mPlatformAccounts =
            Collections.synchronizedSet(new LinkedHashSet<>());

    /** Can be used to cause {@link #getAccessToken} method to fail. */
    private final Map<CoreAccountId, GoogleServiceAuthError> mGetAccessTokenError = new HashMap<>();

    /** Can be used to block {@link #getAccounts()} ()} result. */
    private @Nullable Promise<List<AccountInfo>> mBlockedGetAccountsPromise;

    private final Intent mAddAccountIntent =
            new Intent(ContextUtils.getApplicationContext(), AddAccountActivityStub.class);

    /** The account that will be added by AddAccountActivityStub. */
    private AccountInfo mAccountToAdd;

    /** Used as the result of {@link #didAccountFetchSucceed()}. */
    private boolean mDidAccountFetchingSucceed = true;

    private final boolean mSerializeToPrefs;

    /**
     * Creates an object of FakeAccountManagerFacade. The account data will be stored in memory and
     * wiped when this object is closed.
     */
    public FakeAccountManagerFacade() {
        this(false);
    }

    /**
     * Creates an object of FakeAccountManagerFacade.
     *
     * @param serializeToPrefs Whether to persist account data in SharedPreferences. When true,
     *     accounts are loaded from SharedPreferences on creation and saved to SharedPreferences on
     *     modification. When false, account data is only stored in memory.
     */
    public FakeAccountManagerFacade(boolean serializeToPrefs) {
        mSerializeToPrefs = serializeToPrefs;
        if (mSerializeToPrefs) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        setAccounts(SharedPrefsAccountStorage.loadAccounts());
                    });
        }
    }

    @MainThread
    @Override
    public void addObserver(AccountsChangeObserver observer) {
        ThreadUtils.checkUiThread();
        mObservers.add(observer);
    }

    @MainThread
    @Override
    public void removeObserver(AccountsChangeObserver observer) {
        ThreadUtils.checkUiThread();
        mObservers.remove(observer);
    }

    @Override
    public Promise<List<AccountInfo>> getAccounts() {
        ThreadUtils.checkUiThread();
        if (mBlockedGetAccountsPromise != null) {
            return mBlockedGetAccountsPromise;
        }

        if (!SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            return Promise.fulfilled(getAccountsInternal());
        }
        return Promise.fulfilled(getPlatformAccountInfosInternal());
    }

    @MainThread
    @Override
    public void getAccessToken(
            CoreAccountInfo coreAccountInfo, String scope, GetAccessTokenCallback callback) {
        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            @Nullable FakePlatformAccount account = getPlatformAccount(coreAccountInfo.getGaiaId());
            if (account == null) {
                Log.w(TAG, "Cannot find account:" + coreAccountInfo.toString());
                ThreadUtils.postOnUiThread(
                        () ->
                                callback.onGetTokenFailure(
                                        new GoogleServiceAuthError(
                                                GoogleServiceAuthErrorState.USER_NOT_SIGNED_UP)));
                return;
            }

            GoogleServiceAuthError authError = mGetAccessTokenError.get(coreAccountInfo.getId());
            if (authError != null) {
                ThreadUtils.postOnUiThread(() -> callback.onGetTokenFailure(authError));
            } else {
                ThreadUtils.postOnUiThread(
                        () ->
                                callback.onGetTokenSuccess(
                                        account.getAccessTokenOrGenerateNew(scope)));
            }
            return;
        }

        @Nullable AccountHolder accountHolder = getAccountHolder(coreAccountInfo.getId());
        if (accountHolder == null) {
            Log.w(TAG, "Cannot find account:" + coreAccountInfo.toString());
            ThreadUtils.postOnUiThread(
                    () ->
                            callback.onGetTokenFailure(
                                    new GoogleServiceAuthError(
                                            GoogleServiceAuthErrorState.USER_NOT_SIGNED_UP)));
            return;
        }
        GoogleServiceAuthError authError = mGetAccessTokenError.get(coreAccountInfo.getId());
        if (authError != null) {
            ThreadUtils.postOnUiThread(() -> callback.onGetTokenFailure(authError));
        } else {
            ThreadUtils.postOnUiThread(
                    () ->
                            callback.onGetTokenSuccess(
                                    accountHolder.getAccessTokenOrGenerateNew(scope)));
        }
    }

    @Override
    public void invalidateAccessToken(String accessToken, @Nullable Runnable completedRunnable) {
        ThreadUtils.checkUiThread();
        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            synchronized (mPlatformAccounts) {
                for (FakePlatformAccount account : mPlatformAccounts) {
                    if (account.removeAccessToken(accessToken)) {
                        break;
                    }
                }
            }
        } else {
            synchronized (mAccountHolders) {
                for (AccountHolder accountHolder : mAccountHolders) {
                    if (accountHolder.removeAccessToken(accessToken)) {
                        break;
                    }
                }
            }
        }

        if (completedRunnable != null) {
            completedRunnable.run();
        }
    }

    @Override
    public void waitForPendingTokenRequestsToComplete(Runnable requestsCompletedCallback) {
        throw new UnsupportedOperationException("Not implemented");
    }

    @Override
    public void checkIsSubjectToParentalControls(
            CoreAccountInfo coreAccountInfo, ChildAccountStatusListener listener) {

        if (!SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            AccountHolder accountHolder = getAccountHolder(coreAccountInfo.getId());
            if (accountHolder.getAccountCapabilities().isSubjectToParentalControls()
                    == Tribool.TRUE) {
                listener.onStatusReady(true, coreAccountInfo);
            } else {
                listener.onStatusReady(false, /* childAccount= */ null);
            }
            return;
        }

        FakePlatformAccount account = getPlatformAccount(coreAccountInfo.getGaiaId());
        if (account.getAccountInfo().getAccountCapabilities().isSubjectToParentalControls()
                == Tribool.TRUE) {
            listener.onStatusReady(true, coreAccountInfo);
        } else {
            listener.onStatusReady(false, /* childAccount= */ null);
        }
    }

    @Override
    public Promise<AccountCapabilities> getAccountCapabilities(CoreAccountInfo coreAccountInfo) {
        if (!SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            AccountHolder accountHolder = getAccountHolder(coreAccountInfo.getId());
            return Promise.fulfilled(accountHolder.getAccountCapabilities());
        }

        FakePlatformAccount account = getPlatformAccount(coreAccountInfo.getGaiaId());
        return Promise.fulfilled(account.getAccountInfo().getAccountCapabilities());
    }

    @Override
    public void createAddAccountIntent(
            @Nullable String prefilledEmail, Callback<@Nullable Intent> callback) {
        callback.onResult(mAddAccountIntent);
    }

    @Override
    public void updateCredentials(
            Account account, Activity activity, @Nullable Callback<Boolean> callback) {}

    @Override
    public void confirmCredentials(Account account, Activity activity, Callback<Bundle> callback) {
        callback.onResult(new Bundle());
    }

    @Override
    public boolean didAccountFetchSucceed() {
        return mDidAccountFetchingSucceed;
    }

    /** Set the result of {@link #didAccountFetchSucceed()}. */
    public void setAccountFetchFailed() {
        mDidAccountFetchingSucceed = false;
    }

    @Override
    public void disallowTokenRequestsForTesting() {
        throw new UnsupportedOperationException("Not implemented");
    }

    /**
     * Adds an account to the fake AccountManagerFacade.
     *
     * <p>TODO(crbug.com/40274844): Migrate to the version that uses AccountInfo below.
     */
    @Deprecated
    public void addAccount(Account account) {
        addAccount(new AccountInfo.Builder(account.name, toGaiaId(account.name)).build());
    }

    /** Adds an account represented by {@link AccountInfo}. */
    public void addAccount(AccountInfo accountInfo) {
        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            ThreadUtils.runOnUiThreadBlocking(() -> addAccountOnUiThread(accountInfo));
            return;
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAccountHolders.add(new AccountHolder(accountInfo));
                    if (mBlockedGetAccountsPromise == null) {
                        fireOnAccountsChangedNotification();
                    }
                });
    }

    @MainThread
    private void addAccountOnUiThread(AccountInfo accountInfo) {
        ThreadUtils.checkUiThread();
        mPlatformAccounts.add(new FakePlatformAccount(accountInfo));
        if (mBlockedGetAccountsPromise == null) {
            fireOnAccountsChangedNotification();
        }
        if (mSerializeToPrefs) {
            SharedPrefsAccountStorage.saveAccounts(getPlatformAccountInfosInternal());
        }
    }

    /**
     * Updates that account that is already present. Uses `AccountInfo.getId()` and `CoreAccountId`
     * equality to search for the account to update. Throws if the account can't be found.
     */
    public void updateAccount(AccountInfo accountInfo) {
        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            ThreadUtils.runOnUiThreadBlocking(() -> updateAccountOnUiThread(accountInfo));
            return;
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    synchronized (mAccountHolders) {
                        @Nullable AccountHolder accountHolder =
                                mAccountHolders.stream()
                                        .filter(
                                                (ah) ->
                                                        ah.getAccountInfo()
                                                                .getId()
                                                                .equals(accountInfo.getId()))
                                        .findFirst()
                                        .orElse(null);
                        if (accountHolder == null) {
                            throw new IllegalArgumentException(
                                    "Account " + accountInfo.getEmail() + " can't be found!");
                        }
                        mAccountHolders.remove(accountHolder);
                        mAccountHolders.add(new AccountHolder(accountInfo));
                    }
                    if (mBlockedGetAccountsPromise == null) {
                        fireOnAccountsChangedNotification();
                    }
                });
    }

    @MainThread
    private void updateAccountOnUiThread(AccountInfo accountInfo) {
        ThreadUtils.checkUiThread();
        synchronized (mPlatformAccounts) {
            @Nullable FakePlatformAccount platformAccount =
                    (FakePlatformAccount)
                            mPlatformAccounts.stream()
                                    .filter(
                                            (account) ->
                                                    Objects.equals(
                                                            account.getId(),
                                                            accountInfo.getGaiaId()))
                                    .findFirst()
                                    .orElse(null);
            if (platformAccount == null) {
                throw new IllegalArgumentException(
                        "Account " + accountInfo.getEmail() + " can't be found!");
            }
            mPlatformAccounts.remove(platformAccount);
            mPlatformAccounts.add(new FakePlatformAccount(accountInfo));
        }
        if (mBlockedGetAccountsPromise == null) {
            fireOnAccountsChangedNotification();
        }
        if (mSerializeToPrefs) {
            SharedPrefsAccountStorage.saveAccounts(getPlatformAccountInfosInternal());
        }
    }

    /**
     * Removes an account from the fake AccountManagerFacade.
     *
     * <p>TODO(crbug.com/40274844): Migrate to the version that uses CoreAccountId below.
     */
    @Deprecated
    public void removeAccount(Account account) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    synchronized (mAccountHolders) {
                        @Nullable AccountHolder accountHolder =
                                mAccountHolders.stream()
                                        .filter((ah) -> ah.getAccount().equals(account))
                                        .findFirst()
                                        .orElse(null);
                        if (accountHolder == null || !mAccountHolders.remove(accountHolder)) {
                            throw new IllegalArgumentException("Cannot find account:" + account);
                        }
                    }
                    if (mBlockedGetAccountsPromise == null) {
                        fireOnAccountsChangedNotification();
                    }
                });
    }

    /** Removes an account from the fake AccountManagerFacade. */
    public void removeAccount(CoreAccountId accountId) {
        if (!SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        synchronized (mAccountHolders) {
                            @Nullable AccountHolder accountHolder =
                                    mAccountHolders.stream()
                                            .filter(
                                                    (ah) ->
                                                            ah.getAccountInfo()
                                                                    .getId()
                                                                    .equals(accountId))
                                            .findFirst()
                                            .orElse(null);
                            if (accountHolder == null || !mAccountHolders.remove(accountHolder)) {
                                throw new IllegalArgumentException(
                                        "Cannot find account:" + accountId);
                            }
                        }
                        if (mBlockedGetAccountsPromise == null) {
                            fireOnAccountsChangedNotification();
                        }
                    });
            return;
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    synchronized (mPlatformAccounts) {
                        @Nullable FakePlatformAccount platformAccount =
                                mPlatformAccounts.stream()
                                        .filter(
                                                (account) ->
                                                        account.getAccountInfo()
                                                                .getId()
                                                                .equals(accountId))
                                        .findFirst()
                                        .orElse(null);
                        if (platformAccount == null || !mPlatformAccounts.remove(platformAccount)) {
                            throw new IllegalArgumentException("Cannot find account:" + accountId);
                        }
                    }
                    if (mBlockedGetAccountsPromise == null) {
                        fireOnAccountsChangedNotification();
                    }
                });
    }

    /** Converts an email to a fake gaia Id. */
    public static GaiaId toGaiaId(String email) {
        return new GaiaId("gaia-id-" + email.replace("@", "_at_"));
    }

    /**
     * Blocks updates to the account lists returned by and {@link #getAccounts}. After this method
     * is called, subsequent calls to {@link #getAccounts} will return promises that won't be
     * updated until the returned {@link AutoCloseable} is closed.
     *
     * @param populateCache whether {@link #getAccounts} should return a fulfilled promise. If true,
     *     then the promise will be fulfilled with the current list of available accounts. Any
     *     account addition/removal later on will not be reflected in {@link #getAccounts}.
     * @return {@link AutoCloseable} that should be closed to unblock account updates.
     */
    public UpdateBlocker blockGetAccounts(boolean populateCache) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assert mBlockedGetAccountsPromise == null;
                    mBlockedGetAccountsPromise = new Promise<>();
                    if (populateCache) {
                        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
                            mBlockedGetAccountsPromise.fulfill(getPlatformAccountInfosInternal());
                        } else {
                            mBlockedGetAccountsPromise.fulfill(getAccountsInternal());
                        }
                    }
                });
        return new UpdateBlocker();
    }

    /**
     * Unblocks callers that are waiting for {@link #getAccounts} results. Use after {@link
     * #blockGetAccounts(boolean)} to unblock callers waiting for promises obtained from {@link
     * #getAccounts}.
     */
    private void unblockGetAccounts() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assert mBlockedGetAccountsPromise != null;
                    if (!mBlockedGetAccountsPromise.isFulfilled()) {
                        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
                            mBlockedGetAccountsPromise.fulfill(getPlatformAccountInfosInternal());
                        } else {
                            mBlockedGetAccountsPromise.fulfill(getAccountsInternal());
                        }
                    }
                    mBlockedGetAccountsPromise = null;
                    fireOnAccountsChangedNotification();
                });
    }

    /**
     * Sets an error for the given `accountId` when requesting an access token. After this method is
     * called, subsequent calls to {@link #getAccessToken} with {@param accountInfo} will return an
     * {@link AuthException} with the `authError` provided.
     *
     * <p>If the `authError` has the state {@link GoogleServiceAuthErrorState#NONE} then {@link
     * #getAccessToken} will return valid access tokens instead of returning an error. Errors must
     * be set through a previous call to {@link #addOrUpdateAccessTokenError} before they can be
     * cleared this way.
     *
     * @param accountId The {@link CoreAccountId} to set the authError to.
     * @param authError A {@link GoogleServiceAuthError} to return from {@link #getAccessToken}.
     */
    @MainThread
    public void addOrUpdateAccessTokenError(
            CoreAccountId accountId, GoogleServiceAuthError authError) {
        ThreadUtils.assertOnUiThread();
        if (authError.getState() == GoogleServiceAuthErrorState.NONE) {
            assert mGetAccessTokenError.containsKey(accountId);
            mGetAccessTokenError.remove(accountId);
            return;
        }
        mGetAccessTokenError.put(accountId, authError);
    }

    /**
     * Initializes the next add account flow with a given account to add.
     *
     * @param newAccount The account that should be added by the add account flow.
     */
    public void setAddAccountFlowResult(@Nullable AccountInfo newAccount) {
        mAccountToAdd = newAccount;
    }

    private List<AccountInfo> getAccountsInternal() {
        ThreadUtils.checkUiThread();
        synchronized (mAccountHolders) {
            return mAccountHolders.stream()
                    .map(AccountHolder::getAccountInfo)
                    .collect(Collectors.toList());
        }
    }

    private List<AccountInfo> getPlatformAccountInfosInternal() {
        ThreadUtils.checkUiThread();
        synchronized (mPlatformAccounts) {
            return mPlatformAccounts.stream()
                    .map(FakePlatformAccount::getAccountInfo)
                    .collect(Collectors.toList());
        }
    }

    @AnyThread
    @Nullable
    private AccountHolder getAccountHolder(CoreAccountId accountId) {
        synchronized (mAccountHolders) {
            return mAccountHolders.stream()
                    .filter(
                            accountHolder ->
                                    accountId.equals(accountHolder.getAccountInfo().getId()))
                    .findFirst()
                    .orElse(null);
        }
    }

    @AnyThread
    @Nullable
    private FakePlatformAccount getPlatformAccount(GaiaId gaiaId) {
        synchronized (mPlatformAccounts) {
            return (FakePlatformAccount)
                    mPlatformAccounts.stream()
                            .filter(
                                    platformAccount ->
                                            Objects.equals(gaiaId, platformAccount.getId()))
                            .findFirst()
                            .orElse(null);
        }
    }

    @MainThread
    private void fireOnAccountsChangedNotification() {
        ThreadUtils.checkUiThread();
        for (AccountsChangeObserver observer : mObservers) {
            observer.onCoreAccountInfosChanged();
        }
    }

    /**
     * Updates the previously set capabilities with the ones in accountCapabilities and notifies
     * AccountsChangeObservers if there has been a change. New capabilities that were not already
     * set are added and existing ones are updated with the new values.
     */
    @MainThread
    public void updateAccountCapabilities(
            CoreAccountId accountId, AccountCapabilities accountCapabilities) {
        ThreadUtils.checkUiThread();
        assert accountId != null;
        boolean capabilitiesChanged;
        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            FakePlatformAccount account = getPlatformAccount(accountId.getId());
            capabilitiesChanged =
                    account.getAccountInfo()
                            .getAccountCapabilities()
                            .updateWith(accountCapabilities);
        } else {
            AccountHolder accountHolder = getAccountHolder(accountId);
            capabilitiesChanged =
                    accountHolder.getAccountCapabilities().updateWith(accountCapabilities);
        }
        if (capabilitiesChanged) {
            fireOnAccountsChangedNotification();
        }
    }

    private void setAccounts(List<AccountInfo> accounts) {
        ThreadUtils.checkUiThread();
        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            mPlatformAccounts.clear();
            for (AccountInfo accountInfo : accounts) {
                mPlatformAccounts.add(new FakePlatformAccount(accountInfo));
            }
        } else {
            mAccountHolders.clear();
            for (AccountInfo accountInfo : accounts) {
                mAccountHolders.add(new AccountHolder(accountInfo));
            }
        }
        if (mBlockedGetAccountsPromise == null) {
            fireOnAccountsChangedNotification();
        }
    }
}
