// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.accounts.AuthenticatorDescription;
import android.app.Activity;
import android.content.Intent;

import androidx.annotation.GuardedBy;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.AuthException;
import org.chromium.components.signin.ProfileDataSource;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.UUID;

/**
 * The FakeAccountManagerDelegate is intended for testing components that use AccountManagerFacade.
 *
 * You should provide a set of accounts as a constructor argument, or use the more direct approach
 * and provide an array of AccountHolder objects.
 *
 * Currently, this implementation supports adding and removing accounts, handling credentials
 * (including confirming them), and handling of dummy auth tokens.
 *
 * If you want to auto-approve a given authtokentype, use {@link #addAccount} with
 * an AccountHolder you have built with hasBeenAccepted("yourAuthTokenType", true).
 */
public class FakeAccountManagerDelegate implements AccountManagerDelegate {
    private static final String TAG = "FakeAccountManager";

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private final Set<AccountHolder> mAccounts = new LinkedHashSet<>();
    private final ObserverList<AccountsChangeObserver> mObservers = new ObserverList<>();

    public FakeAccountManagerDelegate() {}

    @Nullable
    @Override
    public ProfileDataSource getProfileDataSource() {
        return null;
    }

    @Nullable
    @Override
    public String getAccountGaiaId(String accountEmail) {
        return "gaia-id-" + accountEmail.replace("@", "_at_");
    }

    @Override
    public boolean isGooglePlayServicesAvailable() {
        return true;
    }

    @Override
    public void registerObservers() {}

    @Override
    public void addObserver(AccountsChangeObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(AccountsChangeObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public Account[] getAccountsSync() {
        ArrayList<Account> result = new ArrayList<>();
        synchronized (mLock) {
            for (AccountHolder ah : mAccounts) {
                result.add(ah.getAccount());
            }
        }
        return result.toArray(new Account[0]);
    }

    /**
     * Adds an AccountHolder.
     */
    public void addAccount(AccountHolder accountHolder) {
        synchronized (mLock) {
            boolean added = mAccounts.add(accountHolder);
            assert added : "Account already added";
        }
        ThreadUtils.runOnUiThreadBlocking(this::fireOnAccountsChangedNotification);
    }

    /**
     * Removes an AccountHolder.
     */
    public void removeAccount(AccountHolder accountHolder) {
        synchronized (mLock) {
            boolean removed = mAccounts.remove(accountHolder);
            assert removed : "Can't find account";
        }
        ThreadUtils.runOnUiThreadBlocking(this::fireOnAccountsChangedNotification);
    }

    @Override
    public AccessTokenData getAuthToken(Account account, String authTokenScope)
            throws AuthException {
        AccountHolder ah = tryGetAccountHolder(account);
        if (ah == null) {
            throw new AuthException(AuthException.NONTRANSIENT,
                    "Cannot get auth token for unknown account '" + account + "'");
        }
        synchronized (mLock) {
            // Some tests register auth tokens with value null, and those should be preserved.
            if (!ah.hasAuthTokenRegistered(authTokenScope)
                    && ah.getAuthToken(authTokenScope) == null) {
                // No authtoken registered. Need to create one.
                String authToken = UUID.randomUUID().toString();
                Log.d(TAG,
                        "Created new auth token for " + ah.getAccount() + ": authTokenScope = "
                                + authTokenScope + ", authToken = " + authToken);
                ah = ah.withAuthToken(authTokenScope, authToken);
                mAccounts.add(ah);
            }
        }
        return ah.getAuthToken(authTokenScope);
    }

    @Override
    public void invalidateAuthToken(String authToken) {
        if (authToken == null) {
            throw new IllegalArgumentException("AuthToken can not be null");
        }
        synchronized (mLock) {
            for (AccountHolder ah : mAccounts) {
                if (ah.removeAuthToken(authToken)) {
                    break;
                }
            }
        }
    }

    @Override
    public AuthenticatorDescription[] getAuthenticatorTypes() {
        AuthenticatorDescription googleAuthenticator =
                new AuthenticatorDescription(AccountUtils.GOOGLE_ACCOUNT_TYPE, "p1", 0, 0, 0, 0);

        return new AuthenticatorDescription[] {googleAuthenticator};
    }

    @Override
    public boolean hasFeature(Account account, String feature) {
        AccountHolder accountHolder = tryGetAccountHolder(account);
        // Features status is queried asynchronously, so the account could have been removed.
        return accountHolder != null && accountHolder.hasFeature(feature);
    }

    @Override
    public void createAddAccountIntent(Callback<Intent> callback) {
        ThreadUtils.assertOnUiThread();
        ThreadUtils.postOnUiThread(callback.bind(null));
    }

    @Override
    public void updateCredentials(
            Account account, Activity activity, final Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
        if (callback == null) {
            return;
        }

        ThreadUtils.postOnUiThread(callback.bind(true));
    }

    private AccountHolder tryGetAccountHolder(Account account) {
        if (account == null) {
            throw new IllegalArgumentException("Account can not be null");
        }
        synchronized (mLock) {
            for (AccountHolder accountHolder : mAccounts) {
                if (account.equals(accountHolder.getAccount())) {
                    return accountHolder;
                }
            }
        }
        return null;
    }

    @MainThread
    private void fireOnAccountsChangedNotification() {
        for (AccountsChangeObserver observer : mObservers) {
            observer.onAccountsChanged();
        }
    }
}
