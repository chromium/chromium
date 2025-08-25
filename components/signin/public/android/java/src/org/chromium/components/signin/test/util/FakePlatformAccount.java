// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.signin.test.util;

import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.PlatformAccount;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.google_apis.gaia.GaiaId;

/**
 * A test implementation of {@link PlatformAccount} for testing components that depend on {@link
 * AccountManagerDelegate}.
 */
public class FakePlatformAccount implements PlatformAccount {
    private final AccountInfo mAccount;

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
}
