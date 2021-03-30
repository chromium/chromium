// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;

import androidx.annotation.NonNull;
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
    private final Map<String, String> mAuthTokens;
    private final Map<String, Boolean> mHasBeenAccepted;
    private final boolean mAlwaysAccept;
    private final Set<String> mFeatures;

    private AccountHolder(Account account, Map<String, String> authTokens,
            Map<String, Boolean> hasBeenAccepted, boolean alwaysAccept, Set<String> features) {
        assert account != null;
        assert authTokens != null;
        assert hasBeenAccepted != null;
        assert features != null;

        mAccount = account;
        mAuthTokens = authTokens;
        mHasBeenAccepted = hasBeenAccepted;
        mAlwaysAccept = alwaysAccept;
        mFeatures = features;
    }

    public Account getAccount() {
        return mAccount;
    }

    boolean hasAuthTokenRegistered(String authTokenType) {
        return mAuthTokens.containsKey(authTokenType);
    }

    @Nullable
    AccessTokenData getAuthToken(String authTokenType) {
        String authTokenString = mAuthTokens.get(authTokenType);
        return authTokenString == null ? null : new AccessTokenData(authTokenString);
    }

    boolean hasBeenAccepted(String authTokenType) {
        return mAlwaysAccept
                || mHasBeenAccepted.containsKey(authTokenType)
                && mHasBeenAccepted.get(authTokenType);
    }

    /**
     * Removes an auth token from the auth token map.
     *
     * @param authToken the auth token to remove
     * @return true if the auth token was found
     */
    boolean removeAuthToken(String authToken) {
        String foundKey = null;
        for (Map.Entry<String, String> tokenEntry : mAuthTokens.entrySet()) {
            if (authToken.equals(tokenEntry.getValue())) {
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

    public Set<String> getFeatures() {
        return mFeatures;
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
     * Creates builder of {@link AccountHolder} for the given account email.
     */
    public static Builder builder(String accountEmail) {
        return new Builder(AccountUtils.createAccountFromName(accountEmail));
    }

    /**
     * Creates builder of {@link AccountHolder} for the given account.
     */
    public static Builder builder(@NonNull Account account) {
        return new Builder(account);
    }

    AccountHolder withAuthToken(String authTokenType, String authToken) {
        return copy().authToken(authTokenType, authToken).build();
    }

    private Builder copy() {
        return builder(mAccount)
                .authTokens(mAuthTokens)
                .hasBeenAcceptedMap(mHasBeenAccepted)
                .alwaysAccept(mAlwaysAccept);
    }

    /**
     * Used to construct AccountHolder instances.
     */
    public static class Builder {
        private final Account mAccount;
        private Map<String, String> mAuthTokens = new HashMap<>();
        private Map<String, Boolean> mHasBeenAccepted = new HashMap<>();
        private boolean mAlwaysAccept;
        private final Set<String> mFeatures = new HashSet<>();

        Builder(@NonNull Account account) {
            mAccount = account;
        }

        Builder authToken(String authTokenType, String authToken) {
            mAuthTokens.put(authTokenType, authToken);
            return this;
        }

        Builder authTokens(@NonNull Map<String, String> authTokens) {
            mAuthTokens = authTokens;
            return this;
        }

        Builder hasBeenAcceptedMap(@NonNull Map<String, Boolean> hasBeenAcceptedMap) {
            mHasBeenAccepted = hasBeenAcceptedMap;
            return this;
        }

        public Builder alwaysAccept(boolean alwaysAccept) {
            mAlwaysAccept = alwaysAccept;
            return this;
        }

        /**
         * Adds features to the set of features for this account.
         */
        public Builder addFeatures(String... features) {
            Collections.addAll(mFeatures, features);
            return this;
        }

        public AccountHolder build() {
            return new AccountHolder(
                    mAccount, mAuthTokens, mHasBeenAccepted, mAlwaysAccept, mFeatures);
        }
    }
}
