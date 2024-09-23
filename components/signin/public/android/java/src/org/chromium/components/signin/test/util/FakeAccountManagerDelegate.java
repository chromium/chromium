// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.AuthException;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;

import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Set;

/**
 * The FakeAccountManagerDelegate is intended for testing components that use AccountManagerFacade.
 *
 * <p>You should provide a set of accounts as a constructor argument, or use the more direct
 * approach and provide an array of AccountHolder objects.
 *
 * <p>Currently, this implementation supports adding and removing accounts, handling credentials
 * (including confirming them), and handling of placeholder auth tokens.
 */
public class FakeAccountManagerDelegate implements AccountManagerDelegate {
    /** Converts an email to a fake gaia Id. */
    public static String toGaiaId(String email) {
        return "gaia-id-" + email.replace("@", "_at_");
    }

    private final Set<AccountHolder> mAccounts = Collections.synchronizedSet(new LinkedHashSet<>());

    private AccountsChangeObserver mObserver;

    public FakeAccountManagerDelegate() {
        mObserver = null;
    }

    @Nullable
    @Override
    public String getAccountGaiaId(String accountEmail) {
        @Nullable AccountHolder accountHolder = tryGetAccountHolder(accountEmail);
        return accountHolder != null ? accountHolder.getAccountInfo().getGaiaId() : null;
    }

    @Override
    public void attachAccountsChangeObserver(AccountsChangeObserver observer) {
        mObserver = observer;
    }

    @Override
    public Account[] getAccountsSynchronous() throws AccountManagerDelegateException {
        synchronized (mAccounts) {
            return mAccounts.stream().map((ah) -> ah.getAccount()).toArray(Account[]::new);
        }
    }

    /** Adds an AccountHolder. */
    public void addAccount(AccountInfo accountInfo) {
        boolean added = mAccounts.add(new AccountHolder(accountInfo));
        assert added : "Account already added";
        callOnCoreAccountInfoChanged();
    }

    /** Removes an AccountHolder. */
    public void removeAccount(CoreAccountId accountId) {
        synchronized (mAccounts) {
            @Nullable AccountHolder accountHolder = tryGetAccountHolder(accountId);
            if (accountHolder == null || !mAccounts.remove(accountHolder)) {
                throw new IllegalArgumentException(
                        String.format("Can't find the account: %s", accountId.getId()));
            }
        }
        callOnCoreAccountInfoChanged();
    }

    public void callOnCoreAccountInfoChanged() {
        if (mObserver != null) {
            ThreadUtils.runOnUiThreadBlocking(mObserver::onCoreAccountInfosChanged);
        }
    }

    @Override
    public AccessTokenData getAuthToken(Account account, String scope) throws AuthException {
        AccountHolder accountHolder = tryGetAccountHolder(account.name);
        if (accountHolder == null) {
            throw new AuthException(
                    AuthException.NONTRANSIENT,
                    "Cannot get auth token for unknown account '" + account + "'");
        }
        return accountHolder.getAccessTokenOrGenerateNew(scope);
    }

    @Override
    public void invalidateAuthToken(String authToken) {
        if (authToken == null) {
            throw new IllegalArgumentException("AuthToken can not be null");
        }
        synchronized (mAccounts) {
            for (AccountHolder ah : mAccounts) {
                if (ah.removeAccessToken(authToken)) {
                    break;
                }
            }
        }
    }

    @Override
    public boolean hasFeature(Account account, String feature) {
        // Account features aren't supported in FakeAccountManagerDelegate.
        return false;
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

    // TODO(crbug.com/40274844): Remove this method after migrating the interface to CoreAccountId.
    private @Nullable AccountHolder tryGetAccountHolder(String accountEmail) {
        synchronized (mAccounts) {
            return mAccounts.stream()
                    .filter(
                            accountHolder ->
                                    accountEmail.equals(accountHolder.getAccountInfo().getEmail()))
                    .findFirst()
                    .orElse(null);
        }
    }

    private @Nullable AccountHolder tryGetAccountHolder(CoreAccountId accountId) {
        synchronized (mAccounts) {
            return mAccounts.stream()
                    .filter(
                            accountHolder ->
                                    accountId.equals(accountHolder.getAccountInfo().getId()))
                    .findFirst()
                    .orElse(null);
        }
    }
}
