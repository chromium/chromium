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
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GaiaId;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;

import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.stream.Collectors;

/** FakeAccountManagerFacade is an {@link AccountManagerFacade} stub intended for testing. */
public class FakeAccountManagerFacade implements AccountManagerFacade {
    /**
     * Can be closed to unblock updates to the list of accounts. See {@link
     * FakeAccountManagerFacade#blockGetCoreAccountInfos}.
     */
    public class UpdateBlocker implements AutoCloseable {
        /** Use {@link FakeAccountManagerFacade#blockGetCoreAccountInfos} to instantiate. */
        private UpdateBlocker() {}

        @Override
        public void close() {
            unblockGetCoreAccountInfos();
        }
    }

    private static final String TAG = "FakeAccountManager";

    /**
     * All the account names starting with this prefix will be considered as a child account in
     * {@link FakeAccountManagerFacade}.
     */
    private static final String CHILD_ACCOUNT_NAME_PREFIX = "child.";

    /** An {@link Activity} stub to test add account flow. */
    public static final class AddAccountActivityStub extends Activity {
        public static final @IdRes int OK_BUTTON_ID = R.id.ok_button;
        public static final @IdRes int CANCEL_BUTTON_ID = R.id.cancel_button;

        @Override
        public void onCreate(Bundle savedInstanceState) {
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
    private final Set<AccountHolder> mAccountHolders =
            Collections.synchronizedSet(new LinkedHashSet<>());

    /** Can be used to block {@link #getCoreAccountInfos()} ()} result. */
    private @Nullable Promise<List<CoreAccountInfo>> mBlockedGetCoreAccountInfosPromise;

    /** Can be used to block {@link #getAccounts()} ()} result. */
    private @Nullable Promise<List<AccountInfo>> mBlockedGetAccountsPromise;

    private Intent mAddAccountIntent =
            new Intent(ContextUtils.getApplicationContext(), AddAccountActivityStub.class);

    /** The account that will be added by AddAccountActivityStub. */
    private AccountInfo mAccountToAdd;

    private boolean mDidAccountFetchingSucceed = true;

    /** Creates an object of FakeAccountManagerFacade. */
    public FakeAccountManagerFacade() {}

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
    public Promise<List<CoreAccountInfo>> getCoreAccountInfos() {
        ThreadUtils.checkUiThread();
        if (mBlockedGetCoreAccountInfosPromise != null) {
            return mBlockedGetCoreAccountInfosPromise;
        }
        return Promise.fulfilled(getCoreAccountInfosInternal());
    }

    @Override
    public Promise<List<AccountInfo>> getAccounts() {
        ThreadUtils.checkUiThread();
        if (mBlockedGetAccountsPromise != null) {
            return mBlockedGetAccountsPromise;
        }
        return Promise.fulfilled(getAccountsInternal());
    }

    @MainThread
    @Override
    public void getAccessToken(
            CoreAccountInfo coreAccountInfo, String scope, GetAccessTokenCallback callback) {
        @Nullable AccountHolder accountHolder = getAccountHolder(coreAccountInfo.getId());
        if (accountHolder == null) {
            Log.w(TAG, "Cannot find account:" + coreAccountInfo.toString());
            ThreadUtils.runOnUiThread(
                    () ->
                            callback.onGetTokenFailure(
                                    new GoogleServiceAuthError(
                                            GoogleServiceAuthErrorState.USER_NOT_SIGNED_UP)));
            return;
        }
        ThreadUtils.runOnUiThread(
                () -> callback.onGetTokenSuccess(accountHolder.getAccessTokenOrGenerateNew(scope)));
    }

    @Override
    public void invalidateAccessToken(String accessToken, @Nullable Runnable completedRunnable) {
        ThreadUtils.checkUiThread();
        synchronized (mAccountHolders) {
            for (AccountHolder accountHolder : mAccountHolders) {
                if (accountHolder.removeAccessToken(accessToken)) {
                    break;
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
    public void checkChildAccountStatus(
            CoreAccountInfo coreAccountInfo, ChildAccountStatusListener listener) {
        if (coreAccountInfo.getEmail().startsWith(CHILD_ACCOUNT_NAME_PREFIX)) {
            listener.onStatusReady(true, coreAccountInfo);
        } else {
            listener.onStatusReady(false, /* childAccount= */ null);
        }
    }

    @Override
    public void checkIsSubjectToParentalControls(
            CoreAccountInfo coreAccountInfo, ChildAccountStatusListener listener) {
        AccountHolder accountHolder = getAccountHolder(coreAccountInfo.getId());
        if (accountHolder.getAccountCapabilities().isSubjectToParentalControls() == Tribool.TRUE) {
            listener.onStatusReady(true, coreAccountInfo);
        } else {
            listener.onStatusReady(false, /* childAccount= */ null);
        }
    }

    @Override
    public Promise<AccountCapabilities> getAccountCapabilities(CoreAccountInfo coreAccountInfo) {
        AccountHolder accountHolder = getAccountHolder(coreAccountInfo.getId());
        return Promise.fulfilled(accountHolder.getAccountCapabilities());
    }

    @Override
    public void createAddAccountIntent(Callback<Intent> callback) {
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAccountHolders.add(new AccountHolder(accountInfo));
                    assert (mBlockedGetCoreAccountInfosPromise == null)
                            == (mBlockedGetAccountsPromise == null);
                    if (mBlockedGetCoreAccountInfosPromise == null) {
                        fireOnAccountsChangedNotification();
                    }
                });
    }

    /**
     * Updates that account that is already present. Uses `AccountInfo.getId()` and `CoreAccountId`
     * equality to search for the account to update. Throws if the account can't be found.
     */
    public void updateAccount(AccountInfo accountInfo) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    synchronized (mAccountHolders) {
                        @Nullable
                        AccountHolder accountHolder =
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
                    assert (mBlockedGetCoreAccountInfosPromise == null)
                            == (mBlockedGetAccountsPromise == null);
                    if (mBlockedGetCoreAccountInfosPromise == null) {
                        fireOnAccountsChangedNotification();
                    }
                });
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
                        @Nullable
                        AccountHolder accountHolder =
                                mAccountHolders.stream()
                                        .filter((ah) -> ah.getAccount().equals(account))
                                        .findFirst()
                                        .orElse(null);
                        if (accountHolder == null || !mAccountHolders.remove(accountHolder)) {
                            throw new IllegalArgumentException("Cannot find account:" + account);
                        }
                    }
                    assert (mBlockedGetCoreAccountInfosPromise == null)
                            == (mBlockedGetAccountsPromise == null);
                    if (mBlockedGetCoreAccountInfosPromise == null) {
                        fireOnAccountsChangedNotification();
                    }
                });
    }

    /** Removes an account from the fake AccountManagerFacade. */
    public void removeAccount(CoreAccountId accountId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    synchronized (mAccountHolders) {
                        @Nullable
                        AccountHolder accountHolder =
                                mAccountHolders.stream()
                                        .filter(
                                                (ah) ->
                                                        ah.getAccountInfo()
                                                                .getId()
                                                                .equals(accountId))
                                        .findFirst()
                                        .orElse(null);
                        if (accountHolder == null || !mAccountHolders.remove(accountHolder)) {
                            throw new IllegalArgumentException("Cannot find account:" + accountId);
                        }
                    }
                    assert (mBlockedGetCoreAccountInfosPromise == null)
                            == (mBlockedGetAccountsPromise == null);
                    if (mBlockedGetCoreAccountInfosPromise == null) {
                        fireOnAccountsChangedNotification();
                    }
                });
    }

    /** Converts an email to a fake gaia Id. */
    public static GaiaId toGaiaId(String email) {
        return new GaiaId("gaia-id-" + email.replace("@", "_at_"));
    }

    /**
     * Creates an email used to identify child accounts in tests. A child-specific prefix will be
     * appended to the base name so that the created account will be considered a child account in
     * {@link FakeAccountManagerFacade}.
     */
    public static String generateChildEmail(String baseEmail) {
        return CHILD_ACCOUNT_NAME_PREFIX + baseEmail;
    }

    /**
     * Blocks updates to the account lists returned by {@link #getCoreAccountInfos} and {@link
     * #getAccounts}. After this method is called, subsequent calls to {@link #getCoreAccountInfos}
     * and {@link #getAccounts} will return promises that won't be updated until the returned {@link
     * AutoCloseable} is closed.
     *
     * <p>TODO(crbug.com/385309416): Rename to `blockGetAccounts` after removing
     * `getCoreAccountInfos`.
     *
     * @param populateCache whether {@link #getCoreAccountInfos} and {@link #getAccounts} should
     *     return a fulfilled promise. If true, then the promise will be fulfilled with the current
     *     list of available accounts. Any account addition/removal later on will not be reflected
     *     in {@link #getCoreAccountInfos()} and {@link #getAccounts}.
     * @return {@link AutoCloseable} that should be closed to unblock account updates.
     */
    public UpdateBlocker blockGetCoreAccountInfos(boolean populateCache) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assert mBlockedGetCoreAccountInfosPromise == null;
                    assert mBlockedGetAccountsPromise == null;
                    mBlockedGetCoreAccountInfosPromise = new Promise<>();
                    mBlockedGetAccountsPromise = new Promise<>();
                    if (populateCache) {
                        mBlockedGetCoreAccountInfosPromise.fulfill(getCoreAccountInfosInternal());
                        mBlockedGetAccountsPromise.fulfill(getAccountsInternal());
                    }
                });
        return new UpdateBlocker();
    }

    /**
     * Unblocks callers that are waiting for {@link #getCoreAccountInfos()} and {@link #getAccounts}
     * results. Use after {@link #blockGetCoreAccountInfos(boolean)} to unblock callers waiting for
     * promises obtained from {@link #getCoreAccountInfos()} and {@link #getAccounts}.
     */
    private void unblockGetCoreAccountInfos() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assert mBlockedGetCoreAccountInfosPromise != null;
                    assert mBlockedGetAccountsPromise != null;
                    assert mBlockedGetCoreAccountInfosPromise.isFulfilled()
                            == mBlockedGetAccountsPromise.isFulfilled();
                    if (!mBlockedGetCoreAccountInfosPromise.isFulfilled()) {
                        mBlockedGetCoreAccountInfosPromise.fulfill(getCoreAccountInfosInternal());
                        mBlockedGetAccountsPromise.fulfill(getAccountsInternal());
                    }
                    mBlockedGetCoreAccountInfosPromise = null;
                    mBlockedGetAccountsPromise = null;
                    fireOnAccountsChangedNotification();
                });
    }

    /**
     * Initializes the next add account flow with a given account to add.
     *
     * @param newAccount The account that should be added by the add account flow.
     */
    public void setAddAccountFlowResult(@Nullable AccountInfo newAccount) {
        mAccountToAdd = newAccount;
    }

    /**
     * Makes the add account intent creation fail: createAddAccountIntent() will provide a null
     * intent when it's called.
     */
    public void forceAddAccountIntentCreationFailure() {
        mAddAccountIntent = null;
    }

    private List<CoreAccountInfo> getCoreAccountInfosInternal() {
        ThreadUtils.checkUiThread();
        synchronized (mAccountHolders) {
            return mAccountHolders.stream()
                    .map(AccountHolder::getAccountInfo)
                    .collect(Collectors.toList());
        }
    }

    private List<AccountInfo> getAccountsInternal() {
        ThreadUtils.checkUiThread();
        synchronized (mAccountHolders) {
            return mAccountHolders.stream()
                    .map(AccountHolder::getAccountInfo)
                    .collect(Collectors.toList());
        }
    }

    @AnyThread
    private @Nullable AccountHolder getAccountHolder(CoreAccountId accountId) {
        synchronized (mAccountHolders) {
            return mAccountHolders.stream()
                    .filter(
                            accountHolder ->
                                    accountId.equals(accountHolder.getAccountInfo().getId()))
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
     * Replaces any capabilities that have been previously set with the given accountCapabilities.
     * and notifies AccountsChangeObservers.
     */
    public void setAccountCapabilities(
            CoreAccountId accountId, AccountCapabilities accountCapabilities) {
        ThreadUtils.checkUiThread();
        assert accountId != null;
        AccountHolder accountHolder = getAccountHolder(accountId);
        accountHolder.setAccountCapabilities(accountCapabilities);
        fireOnAccountsChangedNotification();
    }
}
