// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import android.accounts.Account;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.signin.AccountManagerFacade;

/**
 * Structure storing the core information about a Google account that is always known. The {@link
 * CoreAccountInfo} for a given user is almost always the same but it may change in some rare cases.
 * For example, the {@link android.accounts.Account} will change if a user changes email.
 *
 * This class has a native counterpart called CoreAccountInfo. There are several differences between
 * these two classes:
 * - Android class additionally exposes {@link android.accounts.Account} object for interactions
 * with the system.
 * - Android class has the "account name" whereas the native class has "email". This is the same
 * string, only the naming in different.
 */
public class CoreAccountInfo {
    private final CoreAccountId mId;
    private final Account mAccount;
    private final String mGaiaId;

    /**
     * Constructs a CoreAccountInfo with the provided parameters
     * @param id A CoreAccountId associated with the account, equal to either account.name or
     *         gaiaId.
     * @param account Android account.
     * @param gaiaId String representation of the Gaia ID. Must not be an email address.
     */
    public CoreAccountInfo(
            @NonNull CoreAccountId id, @NonNull Account account, @NonNull String gaiaId) {
        assert id != null;
        assert account != null;
        assert gaiaId != null;
        assert !gaiaId.contains("@");

        mId = id;
        mAccount = account;
        mGaiaId = gaiaId;
    }

    @CalledByNative
    private CoreAccountInfo(
            @NonNull CoreAccountId id, @NonNull String name, @NonNull String gaiaId) {
        assert id != null;
        assert name != null;
        assert gaiaId != null;
        assert !gaiaId.contains("@");

        mId = id;
        mAccount = AccountManagerFacade.createAccountFromName(name);
        mGaiaId = gaiaId;
    }

    /**
     * Returns a unique identifier of the current account.
     */
    @CalledByNative
    public CoreAccountId getId() {
        return mId;
    }

    /**
     * Returns a name of the current account.
     */
    @CalledByNative
    public String getName() {
        return mAccount.name;
    }

    /**
     * Returns the string representation of the Gaia ID
     */
    @CalledByNative
    public String getGaiaId() {
        return mGaiaId;
    }

    /**
     * Returns {@link android.accounts.Account} object holding a name of the current account.
     */
    public Account getAccount() {
        return mAccount;
    }

    @Override
    public String toString() {
        return String.format("CoreAccountInfo{id[%s], name[%s]}", getId(), getName());
    }

    @Override
    public int hashCode() {
        return 31 * mId.hashCode() + mAccount.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof CoreAccountInfo)) return false;
        CoreAccountInfo other = (CoreAccountInfo) obj;
        return mId.equals(other.mId) && mAccount.equals(other.mAccount);
    }
}
