// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.google_apis.gaia.GaiaId;

/** Default empty implementation of {@link AccountManagerDelegate}. */
@NullMarked
public class NullAccountManagerDelegate implements AccountManagerDelegate {

    public NullAccountManagerDelegate() {}

    @Override
    public void attachAccountsChangeObserver(AccountsChangeObserver observer) {}

    @Override
    public Account[] getAccountsSynchronous() {
        return new Account[] {};
    }

    @Override
    @NullUnmarked
    public AccessTokenData getAccessToken(Account account, String authTokenScope) {
        throw new UnsupportedOperationException(
                "NullAccountManagerDelegate does not implement getAccessToken");
    }

    @Override
    public void invalidateAccessToken(String authToken) throws AuthException {
        throw new UnsupportedOperationException(
                "NullAccountManagerDelegate does not implement invalidateAccessToken");
    }

    @Override
    public @CapabilityResponse int hasCapability(@Nullable Account account, String capability) {
        return CapabilityResponse.EXCEPTION;
    }

    @Override
    public void createAddAccountIntent(
            @Nullable String prefilledEmail, Callback<@Nullable Intent> callback) {
        throw new UnsupportedOperationException(
                "NullAccountManagerDelegate does not implement createAddAccountIntent");
    }

    @Override
    public void updateCredentials(
            Account account, Activity activity, final @Nullable Callback<Boolean> callback) {
        throw new UnsupportedOperationException(
                "NullAccountManagerDelegate does not implement updateCredentials");
    }

    @Override
    public @Nullable GaiaId getAccountGaiaId(String accountEmail) {
        throw new UnsupportedOperationException(
                "NullAccountManagerDelegate does not implement getAccountGaiaId");
    }

    @Override
    public void confirmCredentials(
            Account account, @Nullable Activity activity, Callback<@Nullable Bundle> callback) {
        throw new UnsupportedOperationException(
                "NullAccountManagerDelegate does not implement confirmCredentials");
    }
}
