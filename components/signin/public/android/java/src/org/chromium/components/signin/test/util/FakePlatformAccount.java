// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.signin.test.util;

import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.PlatformAccount;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

/**
 * A test implementation of {@link PlatformAccount} for testing components that depend on {@link
 * AccountManagerDelegate}.
 */
public class FakePlatformAccount implements PlatformAccount {
    private final AccountInfo mAccount;
    private final Map<String, AccessTokenData> mAccessTokens =
            Collections.synchronizedMap(new HashMap<>());

    public FakePlatformAccount(AccountInfo accountInfo) {
        mAccount = accountInfo;
    }

    /** Returns gaiaId of the PlatformAccount. */
    @Override
    public GaiaId getId() {
        return mAccount.getGaiaId();
    }

    /** Returns email of the PlatformAccount. */
    @Override
    public String getEmail() {
        return mAccount.getEmail();
    }

    /** Returns AccountInfo of the PlatformAccount. */
    public AccountInfo getAccountInfo() {
        return mAccount;
    }

    /**
     * Returns a {@link AccountManagerDelegate.CapabilityResponse} that indicates whether the
     * account has the requested capability or has an exception.
     */
    @Override
    public int fetchCapability(String capability) {
        // TODO (crbug.com/436520680): Implement this method to return if the account has the
        // specified capability or not.
        return AccountManagerDelegate.CapabilityResponse.EXCEPTION;
    }

    /**
     * Gets the access token for the given scope. If a token has not been previously generated for
     * this scope, a new one will be created.
     */
    @Nullable
    public AccessTokenData getAccessTokenOrGenerateNew(String scope) {
        return mAccessTokens.computeIfAbsent(
                scope, (ignored) -> new AccessTokenData(UUID.randomUUID().toString()));
    }

    /**
     * Removes an auth token from the auth token map.
     *
     * @param authToken the auth token to remove.
     * @return true if the auth token was found.
     */
    public boolean removeAccessToken(String accessToken) {
        return mAccessTokens
                .values()
                .removeIf(tokenData -> accessToken.equals(tokenData.getToken()));
    }

    @Override
    public int hashCode() {
        return mAccount.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof FakePlatformAccount)) return false;
        return mAccount.equals(((FakePlatformAccount) obj).mAccount);
    }
}
