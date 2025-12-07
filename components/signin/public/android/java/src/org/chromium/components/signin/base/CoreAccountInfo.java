// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import android.accounts.Account;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.AccountUtils;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;

/**
 * Structure storing the core information about a Google account that is always known. The {@link
 * CoreAccountInfo} for a given user is almost always the same but it may change in some rare cases.
 * For example, the email will change if the user changes email.
 *
 * <p>This class has a native counterpart called CoreAccountInfo.
 */
@NullMarked
@DoNotMock("Use TestAccounts or create a real instance.")
public class CoreAccountInfo {
    private final CoreAccountId mId;
    private final String mEmail;
    private final GaiaId mGaiaId;

    /**
     * Constructs a CoreAccountInfo with the provided parameters
     *
     * @param id A CoreAccountId associated with the account, equal to either email or gaiaId.
     * @param email The email of the account.
     * @param gaiaId An object representing a Gaia ID.
     */
    @CalledByNative
    protected CoreAccountInfo(
            @JniType("CoreAccountId") CoreAccountId id,
            @JniType("std::string") String email,
            @JniType("GaiaId") GaiaId gaiaId) {
        assert id != null;
        assert email != null;
        assert gaiaId != null;
        assert !gaiaId.toString().contains("@");

        mId = id;
        mEmail = email;
        mGaiaId = gaiaId;
    }

    /** Returns a unique identifier of the current account. */
    @CalledByNative
    public @JniType("CoreAccountId") CoreAccountId getId() {
        return mId;
    }

    /** Returns the email of the current account. */
    @CalledByNative
    public @JniType("std::string") String getEmail() {
        return mEmail;
    }

    /** Returns the Gaia ID */
    @CalledByNative
    public @JniType("GaiaId") GaiaId getGaiaId() {
        return mGaiaId;
    }

    @Override
    public String toString() {
        return String.format("CoreAccountInfo{id[%s], name[%s]}", getId(), getEmail());
    }

    @Override
    public int hashCode() {
        int result = 31 * mId.hashCode() + mEmail.hashCode();
        return 31 * result + mGaiaId.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof CoreAccountInfo)) return false;
        CoreAccountInfo other = (CoreAccountInfo) obj;
        return mId.equals(other.mId)
                && mEmail.equals(other.mEmail)
                && mGaiaId.equals(other.mGaiaId);
    }

    /**
     * Null-checking helper to create {@link Account} from a possibly null {@link CoreAccountInfo}.
     *
     * @return {@link Account} for the argument if it is not null, null otherwise.
     */
    @Contract("!null -> !null")
    public static @Nullable Account getAndroidAccountFrom(@Nullable CoreAccountInfo accountInfo) {
        return accountInfo == null
                ? null
                : AccountUtils.createAccountFromEmail(accountInfo.getEmail());
    }

    /**
     * Null-checking helper to get an account id from a possibly null {@link CoreAccountInfo}.
     *
     * @return {@link #getId()} for the argument if it is not null, null otherwise.
     */
    @Contract("!null -> !null")
    public static @Nullable CoreAccountId getIdFrom(@Nullable CoreAccountInfo accountInfo) {
        return accountInfo == null ? null : accountInfo.getId();
    }

    /**
     * Null-checking helper to get an email from a possibly null {@link CoreAccountInfo}.
     *
     * @return {@link #getEmail()} for the argument if it is not null, null otherwise.
     */
    @Contract("!null -> !null")
    public static @Nullable String getEmailFrom(@Nullable CoreAccountInfo accountInfo) {
        return accountInfo == null ? null : accountInfo.getEmail();
    }

    /**
     * Null-checking helper to get a GaiaId from a possibly null {@link CoreAccountInfo}.
     *
     * @return {@link #getGaiaId()} ()} for the argument if it is not null, null otherwise.
     */
    @Contract("!null -> !null")
    public static @Nullable GaiaId getGaiaIdFrom(@Nullable CoreAccountInfo accountInfo) {
        return accountInfo == null ? null : accountInfo.getGaiaId();
    }

    /** Creates a {@link CoreAccountInfo} object from email and gaiaID. */
    public static CoreAccountInfo createFromEmailAndGaiaId(String email, GaiaId gaiaId) {
        return new CoreAccountInfo(new CoreAccountId(gaiaId), email, gaiaId);
    }
}
