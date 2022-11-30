// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;

import androidx.annotation.Nullable;

import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountUtils;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * This class is used by the {@link FakeAccountManagerDelegate} and
 * {@link FakeAccountManagerFacade} to hold information about a given
 * account, such as its password and set of granted auth tokens.
 */
public class AccountHolder {
    private final Account mAccount;
    private final Map<String, AccessTokenData> mAuthTokens;
    private final Set<String> mFeatures;

    private AccountHolder(Account account) {
        assert account != null : "account shouldn't be null!";
        mAccount = account;
        mAuthTokens = new HashMap<>();
        mFeatures = new HashSet<>();
    }

    public Account getAccount() {
        return mAccount;
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

    boolean hasFeature(String feature) {
        return mFeatures.contains(feature);
    }

    @Override
    public int hashCode() {
        return mAccount.hashCode();
    }

    @Override
    public boolean equals(Object that) {
        return that instanceof AccountHolder
                && mAccount.equals(((AccountHolder) that).getAccount());
    }

    /**
     * Creates an {@link AccountHolder} from email.
     */
    public static AccountHolder createFromEmail(String email) {
        return createFromAccount(AccountUtils.createAccountFromName(email));
    }

    /**
     * Creates an {@link AccountHolder} from {@link Account}.
     */
    public static AccountHolder createFromAccount(Account account) {
        return new AccountHolder(account);
    }

    /**
     * Creates an {@link AccountHolder} from email and features.
     */
    public static AccountHolder createFromEmailAndFeatures(String email, String... features) {
        final AccountHolder accountHolder = createFromEmail(email);
        Collections.addAll(accountHolder.mFeatures, features);
        return accountHolder;
    }
}
