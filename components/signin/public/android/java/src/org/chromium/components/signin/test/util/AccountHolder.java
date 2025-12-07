// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;

import androidx.annotation.AnyThread;
import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

/**
 * This class is used by the {@link FakeAccountManagerDelegate} and {@link FakeAccountManagerFacade}
 * to hold information about a given account, such as its password and set of granted auth tokens.
 */
@NullMarked
public class AccountHolder {
    private final AccountInfo mAccountInfo;
    private final Map<String, AccessTokenData> mAccessTokens =
            Collections.synchronizedMap(new HashMap<>());

    public AccountHolder(AccountInfo accountInfo) {
        assert accountInfo != null : "account shouldn't be null!";
        mAccountInfo = accountInfo;
    }

    public Account getAccount() {
        return CoreAccountInfo.getAndroidAccountFrom(mAccountInfo);
    }

    public AccountInfo getAccountInfo() {
        return mAccountInfo;
    }

    @Nullable
    @AnyThread
    AccessTokenData getAccessTokenOrGenerateNew(String scope) {
        return mAccessTokens.computeIfAbsent(
                scope, (ignored) -> new AccessTokenData(UUID.randomUUID().toString()));
    }

    /**
     * Removes an auth token from the auth token map.
     *
     * @param authToken the auth token to remove
     * @return true if the auth token was found
     */
    boolean removeAccessToken(String accessToken) {
        return mAccessTokens
                .values()
                .removeIf(tokenData -> accessToken.equals(tokenData.getToken()));
    }

    @Override
    public int hashCode() {
        return mAccountInfo.hashCode();
    }

    @Override
    public boolean equals(@Nullable Object that) {
        return that instanceof AccountHolder
                && mAccountInfo.equals(((AccountHolder) that).mAccountInfo);
    }

    public AccountCapabilities getAccountCapabilities() {
        ThreadUtils.checkUiThread();
        return mAccountInfo.getAccountCapabilities();
    }
}
