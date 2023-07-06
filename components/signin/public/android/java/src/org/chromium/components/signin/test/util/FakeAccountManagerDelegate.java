// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.accounts.AuthenticatorDescription;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.AuthException;

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
 */
public class FakeAccountManagerDelegate implements AccountManagerDelegate {
    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private final Set<AccountHolder> mAccounts = new LinkedHashSet<>();
    private AccountsChangeObserver mObserver;

    public FakeAccountManagerDelegate() {
        mObserver = null;
    }

    @Nullable
    @Override
    public String getAccountGaiaId(String accountEmail) {
        return "gaia-id-" + accountEmail.replace("@", "_at_");
    }

    @Override
    public void attachAccountsChangeObserver(AccountsChangeObserver observer) {
        mObserver = observer;
    }

    @Override
    public Account[] getAccounts() {
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
        ThreadUtils.runOnUiThreadBlocking(mObserver::onCoreAccountInfosChanged);
    }

    /**
     * Removes an AccountHolder.
     */
    public void removeAccount(AccountHolder accountHolder) {
        synchronized (mLock) {
            boolean removed = mAccounts.remove(accountHolder);
            assert removed : "Can't find account";
        }
        ThreadUtils.runOnUiThreadBlocking(mObserver::onCoreAccountInfosChanged);
    }

    @Override
    public AccessTokenData getAuthToken(Account account, String scope) throws AuthException {
        AccountHolder accountHolder = tryGetAccountHolder(account);
        if (accountHolder == null) {
            throw new AuthException(AuthException.NONTRANSIENT,
                    "Cannot get auth token for unknown account '" + account + "'");
        }
        synchronized (mLock) {
            if (accountHolder.getAuthToken(scope) == null) {
                accountHolder.updateAuthToken(scope, UUID.randomUUID().toString());
            }
        }
        return accountHolder.getAuthToken(scope);
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
    public @CapabilityResponse int hasCapability(Account account, String capability) {
        return hasFeature(account, capability) ? CapabilityResponse.YES : CapabilityResponse.NO;
    }

    @Override
    public void createAddAccountIntent(Callback<Intent> callback) {
        ThreadUtils.assertOnUiThread();
        ThreadUtils.postOnUiThread(callback.bind(null));
    }

    @Override
    public void updateCredentials(
            Account account, Activity activity, final Callback<Boolean> callback) {
        if (callback != null) {
            ThreadUtils.postOnUiThread(callback.bind(true));
        }
    }

    @Override
    public void confirmCredentials(Account account, Activity activity, Callback<Bundle> callback) {
        callback.onResult(null);
    }

    private AccountHolder tryGetAccountHolder(Account account) {
        synchronized (mLock) {
            for (AccountHolder accountHolder : mAccounts) {
                if (account.equals(accountHolder.getAccount())) {
                    return accountHolder;
                }
            }
        }
        return null;
    }
}
