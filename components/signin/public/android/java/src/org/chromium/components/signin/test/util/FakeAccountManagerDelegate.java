// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.AuthException;
import org.chromium.components.signin.PlatformAccount;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Objects;
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
    // Prefix used to define the capability name for querying Identity services.
    private static final String ACCOUNT_CAPABILITY_NAME_PREFIX = "accountcapabilities/";

    /** Converts an email to a fake gaia Id. */
    public static GaiaId toGaiaId(String email) {
        return new GaiaId("gaia-id-" + email.replace("@", "_at_"));
    }

    private final Set<AccountHolder> mAccounts = Collections.synchronizedSet(new LinkedHashSet<>());
    private final Set<PlatformAccount> mPlatformAccounts =
            Collections.synchronizedSet(new LinkedHashSet<>());

    private AccountsChangeObserver mObserver;

    public FakeAccountManagerDelegate() {
        mObserver = null;
    }

    @Nullable
    @Override
    public GaiaId getAccountGaiaId(String accountEmail) {
        @Nullable AccountHolder accountHolder = tryGetAccountHolder(accountEmail);
        return accountHolder != null ? accountHolder.getAccountInfo().getGaiaId() : null;
    }

    @Override
    public void attachAccountsChangeObserver(AccountsChangeObserver observer) {
        mObserver = observer;
    }

    @Override
    public Account[] getAccountsSynchronous() throws AccountManagerDelegateException {
        assert !SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
        synchronized (mAccounts) {
            return mAccounts.stream().map((ah) -> ah.getAccount()).toArray(Account[]::new);
        }
    }

    /** Adds an AccountHolder. */
    public PlatformAccount addAccount(AccountInfo accountInfo) {
        boolean added = false;
        FakePlatformAccount account = new FakePlatformAccount(accountInfo);
        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            added = mPlatformAccounts.add(account);
        } else {
            added = mAccounts.add(new AccountHolder(accountInfo));
        }
        assert added : "Account already added";
        callOnCoreAccountInfoChanged();
        return account;
    }

    /** Removes an AccountHolder. */
    public void removeAccount(CoreAccountId accountId) {
        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            synchronized (mPlatformAccounts) {
                @Nullable PlatformAccount account = tryGetPlatformAccount(accountId);
                if (account == null || !mPlatformAccounts.remove(account)) {
                    throw new IllegalArgumentException(
                            String.format("Can't find the account: %s", accountId.getId()));
                }
            }
        } else {
            synchronized (mAccounts) {
                @Nullable AccountHolder accountHolder = tryGetAccountHolder(accountId);
                if (accountHolder == null || !mAccounts.remove(accountHolder)) {
                    throw new IllegalArgumentException(
                            String.format("Can't find the account: %s", accountId.getId()));
                }
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
    public AccessTokenData getAccessToken(Account account, String scope) throws AuthException {
        AccountHolder accountHolder = tryGetAccountHolder(account.name);
        if (accountHolder == null) {
            throw new AuthException(
                    "Error while getting token for scope '" + scope + "'",
                    new IllegalStateException(
                            "Cannot get auth token for unknown account '" + account + "'"),
                    new GoogleServiceAuthError(GoogleServiceAuthErrorState.USER_NOT_SIGNED_UP));
        }
        return accountHolder.getAccessTokenOrGenerateNew(scope);
    }

    @Override
    public AccessTokenData getAccessTokenForPlatformAccount(
            PlatformAccount account, String authTokenScopes) throws AuthException {
        FakePlatformAccount platformAccount = (FakePlatformAccount) account;
        assert platformAccount != null;
        return platformAccount.getAccessTokenOrGenerateNew(authTokenScopes);
    }

    @Override
    public void invalidateAccessToken(String authToken) {
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
    public void invalidateAccessTokenForPlatformAccount(String authToken) throws AuthException {
        if (authToken == null) {
            throw new IllegalArgumentException("AuthToken can not be null");
        }
        synchronized (mPlatformAccounts) {
            for (PlatformAccount account : mPlatformAccounts) {
                FakePlatformAccount fakePlatformAccount = (FakePlatformAccount) account;
                if (fakePlatformAccount.removeAccessToken(authToken)) {
                    break;
                }
            }
        }
    }

    @Override
    public @CapabilityResponse int hasCapability(Account account, String capability) {
        return CapabilityResponse.NO;
    }

    @Override
    public void createAddAccountIntent(
            @Nullable String prefilledEmail, Callback<@Nullable Intent> callback) {
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

    /** Returns the list of PlatformAccounts added to the AccountManagerDelegate */
    @Override
    public List<PlatformAccount> getPlatformAccountsSynchronous()
            throws AccountManagerDelegateException {
        assert SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
        synchronized (mPlatformAccounts) {
            return new ArrayList<>(mPlatformAccounts);
        }
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

    /** Returns the PlatformAccounts associated with specified CoreAccountId. */
    private @Nullable PlatformAccount tryGetPlatformAccount(CoreAccountId accountId) {
        synchronized (mPlatformAccounts) {
            return mPlatformAccounts.stream()
                    .filter(account -> Objects.equals(account.getId(), accountId.getId()))
                    .findFirst()
                    .orElse(null);
        }
    }

    @Override
    @CapabilityResponse
    public int fetchCapability(PlatformAccount account, String capability) {
        FakePlatformAccount platformAccount = (FakePlatformAccount) account;

        @Tribool
        int hasCapability =
                platformAccount
                        .getAccountInfo()
                        .getAccountCapabilities()
                        .getCapabilityByName(ACCOUNT_CAPABILITY_NAME_PREFIX + capability);

        switch (hasCapability) {
            case Tribool.TRUE:
                return CapabilityResponse.YES;
            case Tribool.FALSE:
                return CapabilityResponse.NO;
            case Tribool.UNKNOWN:
            default:
                return CapabilityResponse.EXCEPTION;
        }
    }
}
