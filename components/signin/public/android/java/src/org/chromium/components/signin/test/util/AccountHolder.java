// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;

import androidx.annotation.Nullable;

import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.util.HashMap;
import java.util.Map;

/**
 * This class is used by the {@link FakeAccountManagerDelegate} and {@link FakeAccountManagerFacade}
 * to hold information about a given account, such as its password and set of granted auth tokens.
 */
public class AccountHolder {
    private AccountInfo mAccountInfo;
    private final Map<String, AccessTokenData> mAuthTokens;

    public AccountHolder(AccountInfo accountInfo) {
        assert accountInfo != null : "account shouldn't be null!";
        mAccountInfo = accountInfo;
        mAuthTokens = new HashMap<>();
    }

    public Account getAccount() {
        return CoreAccountInfo.getAndroidAccountFrom(mAccountInfo);
    }

    public AccountInfo getAccountInfo() {
        return mAccountInfo;
    }

    @Nullable
    AccessTokenData getAuthToken(String authTokenType) {
        return mAuthTokens.get(authTokenType);
    }

    void updateAuthToken(String scope, String token) {
        mAuthTokens.put(scope, new AccessTokenData(token));
    }

    /**
     * Removes an auth token from the auth token map.
     *
     * @param authToken the auth token to remove
     * @return true if the auth token was found
     */
    boolean removeAuthToken(String authToken) {
        String foundKey = null;
        for (Map.Entry<String, AccessTokenData> tokenEntry : mAuthTokens.entrySet()) {
            if (authToken.equals(tokenEntry.getValue().getToken())) {
                foundKey = tokenEntry.getKey();
                break;
            }
        }
        if (foundKey == null) {
            return false;
        } else {
            mAuthTokens.remove(foundKey);
            return true;
        }
    }

    @Override
    public int hashCode() {
        return mAccountInfo.hashCode();
    }

    @Override
    public boolean equals(Object that) {
        return that instanceof AccountHolder
                && mAccountInfo.equals(((AccountHolder) that).mAccountInfo);
    }

    public AccountCapabilities getAccountCapabilities() {
        return mAccountInfo.getAccountCapabilities();
    }

    /** Manually replace the previously set capabilities with given accountCapabilities */
    public void setAccountCapabilities(AccountCapabilities accountCapabilities) {
        final AccountInfo oldAccountInfo = mAccountInfo;
        mAccountInfo =
                new AccountInfo.Builder(oldAccountInfo)
                        .accountCapabilities(accountCapabilities)
                        .build();
    }
}
