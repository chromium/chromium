// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.AuthException;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;

/**
 * FakeAccountManagerFacade is an {@link AccountManagerFacade} stub intended
 * for testing.
 */
public class FakeAccountManagerFacade implements AccountManagerFacade {
    /**
     * All the account names starting with this prefix will be considered as
     * a child account in {@link FakeAccountManagerFacade}.
     */
    private static final String CHILD_ACCOUNT_NAME_PREFIX = "child.";

    /** AddAccountActivityStub intent arguments to set account name and result */
    private static final String ADDED_ACCOUNT_NAME = "AddedAccountName";

    private static final String ADD_ACCOUNT_RESULT = "AddAccountResult";

    /** An {@link Activity} stub to test add account flow. */
    public static final class AddAccountActivityStub extends Activity {
        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            Intent data = new Intent();
            int result = getIntent().getIntExtra(ADD_ACCOUNT_RESULT, RESULT_CANCELED);
            String addedAccountName = getIntent().getStringExtra(ADDED_ACCOUNT_NAME);
            data.putExtra(AccountManager.KEY_ACCOUNT_NAME, addedAccountName);
            if (result != RESULT_CANCELED && addedAccountName != null) {
                ((FakeAccountManagerFacade) AccountManagerFacadeProvider.getInstance())
                        .addAccount(AccountUtils.createAccountFromName(addedAccountName));
            }
            setResult(result, data);
            finish();
        }
    }

    private final Set<AccountHolder> mAccountHolders = new LinkedHashSet<>();
    private final List<AccountsChangeObserver> mObservers = new ArrayList<>();

    /** Can be used to block {@link #getCoreAccountInfos()} ()} result. */
    private @Nullable Promise<List<CoreAccountInfo>> mBlockedGetCoreAccountInfosPromise;

    private @Nullable Intent mAddAccountIntent;

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
    public boolean hasGoogleAccountAuthenticator() {
        return true;
    }

    @Override
    public AccessTokenData getAccessToken(CoreAccountInfo coreAccountInfo, String scope)
            throws AuthException {
        @Nullable
        AccessTokenData result =
                ThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            @Nullable
                            AccountHolder accountHolder =
                                    getAccountHolder(
                                            AccountUtils.createAccountFromName(
                                                    coreAccountInfo.getEmail()));
                            if (accountHolder == null) {
                                return null;
                            }
                            if (accountHolder.getAuthToken(scope) == null) {
                                accountHolder.updateAuthToken(scope, UUID.randomUUID().toString());
                            }
                            return accountHolder.getAuthToken(scope);
                        });
        if (result != null) {
            return result;
        }
        // Since token requests are asynchronous, sometimes they arrive after the account has been
        // removed. Thus, throwing an unchecked exception here would cause test failures (see
        // https://crbug.com/1205346 for details). On the other hand, AuthException thrown here
        // will be caught by ProfileOAuth2TokenServiceDelegate and reported as a token request
        // failure (which matches the behavior of the production code in the situation when a token
        // is requested for an account that doesn't exist or has been removed).
        throw new AuthException(
                /* isTransientError= */ false, "Cannot find account:" + coreAccountInfo.toString());
    }

    @Override
    public void invalidateAccessToken(String accessToken) {
        ThreadUtils.checkUiThread();
        for (AccountHolder accountHolder : mAccountHolders) {
            if (accountHolder.removeAuthToken(accessToken)) {
                break;
            }
        }
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

    public Promise<AccountCapabilities> getAccountCapabilities(CoreAccountInfo coreAccountInfo) {
        AccountHolder accountHolder =
                getAccountHolder(AccountUtils.createAccountFromName(coreAccountInfo.getEmail()));
        return Promise.fulfilled(accountHolder.getAccountCapabilities());
    }

    @Override
    public void createAddAccountIntent(Callback<Intent> callback) {
        callback.onResult(mAddAccountIntent);
        mAddAccountIntent = null;
    }

    @Override
    public void updateCredentials(
            Account account, Activity activity, @Nullable Callback<Boolean> callback) {}

    @Override
    public String getAccountGaiaId(String accountEmail) {
        return toGaiaId(accountEmail);
    }

    @Override
    public void confirmCredentials(Account account, Activity activity, Callback<Bundle> callback) {
        callback.onResult(new Bundle());
    }

    /** Adds an account to the fake AccountManagerFacade. */
    public void addAccount(Account account) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAccountHolders.add(AccountHolder.createFromAccount(account));
                    fireOnAccountsChangedNotification();
                });
    }

    public void addAccountWithAccountInfo(AccountInfo accountInfo) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAccountHolders.add(AccountHolder.createFromAccount(accountInfo));
                    fireOnAccountsChangedNotification();
                });
    }

    /** Removes an account from the fake AccountManagerFacade. */
    public void removeAccount(Account account) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccountHolder accountHolder = AccountHolder.createFromAccount(account);
                    if (!mAccountHolders.remove(accountHolder)) {
                        throw new IllegalArgumentException("Cannot find account:" + accountHolder);
                    }
                    fireOnAccountsChangedNotification();
                });
    }

    /** Converts an email to a fake gaia Id. */
    public static String toGaiaId(String email) {
        return "gaia-id-" + email.replace("@", "_at_");
    }

    /**
     * Creates an email used to identify child accounts in tests.
     * A child-specific prefix will be appended to the base name so that the created account
     * will be considered a child account in {@link FakeAccountManagerFacade}.
     */
    public static String generateChildEmail(String baseEmail) {
        return CHILD_ACCOUNT_NAME_PREFIX + baseEmail;
    }

    /**
     * Blocks callers from getting accounts through {@link #getCoreAccountInfos()}. After this
     * method is called, subsequent calls to {@link #getCoreAccountInfos()} will return a
     * non-fulfilled promise. Use {@link #unblockGetCoreAccountInfos()} to unblock this promise.
     */
    public void blockGetCoreAccountInfos() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assert mBlockedGetCoreAccountInfosPromise == null;
                    mBlockedGetCoreAccountInfosPromise = new Promise<>();
                });
    }

    /**
     * Unblocks callers that are waiting for {@link #getCoreAccountInfos()} result. Use after {@link
     * #blockGetCoreAccountInfos()} to unblock callers waiting for promises obtained from {@link
     * #getCoreAccountInfos()}.
     */
    public void unblockGetCoreAccountInfos() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assert mBlockedGetCoreAccountInfosPromise != null;
                    mBlockedGetCoreAccountInfosPromise.fulfill(getCoreAccountInfosInternal());
                    mBlockedGetCoreAccountInfosPromise = null;
                });
    }

    /**
     * Sets the result for the next add account flow.
     * @param result The activity result to return when the intent is launched
     * @param newAccountName The account name to return when the intent is launched
     */
    public void setResultForNextAddAccountFlow(int result, @Nullable String newAccountName) {
        assert mAddAccountIntent == null : "mAddAccountIntent is already set";
        mAddAccountIntent =
                new Intent(ContextUtils.getApplicationContext(), AddAccountActivityStub.class);
        mAddAccountIntent.putExtra(ADD_ACCOUNT_RESULT, result);
        mAddAccountIntent.putExtra(ADDED_ACCOUNT_NAME, newAccountName);
    }

    private List<CoreAccountInfo> getCoreAccountInfosInternal() {
        ThreadUtils.checkUiThread();
        List<CoreAccountInfo> coreAccountInfos = new ArrayList<>();
        for (AccountHolder accountHolder : mAccountHolders) {
            String accountEmail = accountHolder.getAccount().name;
            coreAccountInfos.add(
                    CoreAccountInfo.createFromEmailAndGaiaId(accountEmail, toGaiaId(accountEmail)));
        }
        return coreAccountInfos;
    }

    @MainThread
    private @Nullable AccountHolder getAccountHolder(Account account) {
        ThreadUtils.checkUiThread();
        for (AccountHolder accountHolder : mAccountHolders) {
            if (accountHolder.getAccount().equals(account)) {
                return accountHolder;
            }
        }
        return null;
    }

    @MainThread
    private void fireOnAccountsChangedNotification() {
        for (AccountsChangeObserver observer : mObservers) {
            observer.onCoreAccountInfosChanged();
        }
    }
}
