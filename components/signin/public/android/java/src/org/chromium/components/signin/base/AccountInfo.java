// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import android.graphics.Bitmap;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.AccountEmailDisplayHook;
import org.chromium.components.signin.SigninConstants;
import org.chromium.components.signin.Tribool;

import java.util.HashMap;

/**
 * Stores all the information known about an account.
 *
 * This class has a native counterpart called AccountInfo.
 */
@NullMarked
public class AccountInfo extends CoreAccountInfo {
    /** Used to instantiate `AccountInfo`. */
    public static class Builder {
        private final CoreAccountInfo mCoreAccountInfo;
        private String mFullName = "";
        private String mGivenName = "";
        private @Nullable String mHostedDomain;
        private @Nullable Bitmap mAccountImage;
        private AccountCapabilities mAccountCapabilities = new AccountCapabilities(new HashMap<>());

        public Builder(String email, GaiaId gaiaId) {
            mCoreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(email, gaiaId);
        }

        public Builder(CoreAccountInfo coreAccountInfo) {
            mCoreAccountInfo = coreAccountInfo;
        }

        /** Creates a builder constructor which holds a copy of {@param accountInfo}. */
        public Builder(AccountInfo accountInfo) {
            this(accountInfo.getEmail(), accountInfo.getGaiaId());
            mFullName = accountInfo.getFullName();
            mGivenName = accountInfo.getGivenName();
            mHostedDomain = accountInfo.mHostedDomain;
            mAccountImage = accountInfo.getAccountImage();
            mAccountCapabilities = accountInfo.getAccountCapabilities();
        }

        public Builder fullName(String fullName) {
            mFullName = fullName;
            return this;
        }

        public Builder givenName(String givenName) {
            mGivenName = givenName;
            return this;
        }

        public Builder hostedDomain(String hostedDomain) {
            mHostedDomain = hostedDomain;
            return this;
        }

        public Builder accountImage(Bitmap accountImage) {
            mAccountImage = accountImage;
            return this;
        }

        public Builder accountCapabilities(AccountCapabilities accountCapabilities) {
            mAccountCapabilities = accountCapabilities;
            return this;
        }

        public AccountInfo build() {
            return new AccountInfo(
                    mCoreAccountInfo.getId(),
                    mCoreAccountInfo.getEmail(),
                    mCoreAccountInfo.getGaiaId(),
                    mFullName,
                    mGivenName,
                    mHostedDomain,
                    mAccountImage,
                    mAccountCapabilities);
        }
    }

    private final String mFullName;
    private final String mGivenName;

    /**
     * `null` if the hosted domain isn't know yet. Contains {@link
     * SigninConstants.NO_HOSTED_DOMAIN_FOUND} if the account is not managed.
     */
    private final @Nullable String mHostedDomain;

    private final @Nullable Bitmap mAccountImage;
    private final AccountCapabilities mAccountCapabilities;

    /** Used from JNI to marshal `AccountInfo` from C++ to Java. */
    @CalledByNative
    private AccountInfo(
            CoreAccountId id,
            String email,
            GaiaId gaiaId,
            String fullName,
            String givenName,
            @Nullable String hostedDomain,
            @Nullable Bitmap accountImage,
            AccountCapabilities accountCapabilities) {
        super(id, email, gaiaId);
        mFullName = fullName;
        mGivenName = givenName;

        mHostedDomain = hostedDomain;
        assert mHostedDomain == null || !mHostedDomain.isEmpty()
                : "Empty string is not permitted for hostedDomain";

        mAccountImage = accountImage;
        mAccountCapabilities = accountCapabilities;
    }

    /**
     * Returns whether the account email can be used in display fields. If
     * `AccountCapabilities.canHaveEmailAddressDisplayed()` is not available (Tribool.UNKNOWN), uses
     * fallback.
     */
    public boolean canHaveEmailAddressDisplayed() {
        return switch (mAccountCapabilities.canHaveEmailAddressDisplayed()) {
            case Tribool.FALSE -> false;
            case Tribool.TRUE -> true;
            default -> AccountEmailDisplayHook.canHaveEmailAddressDisplayed(getEmail());
        };
    }

    /** Returns the full name of the account. */
    @CalledByNative
    public String getFullName() {
        return mFullName;
    }

    /** Returns the given name of the account. */
    @CalledByNative
    public String getGivenName() {
        return mGivenName;
    }

    /** Whether the account is managed. */
    public @Tribool int isManaged() {
        if (mHostedDomain == null) {
            return Tribool.UNKNOWN;
        }
        return mHostedDomain.equals(SigninConstants.NO_HOSTED_DOMAIN_FOUND)
                ? Tribool.FALSE
                : Tribool.TRUE;
    }

    /**
     * Management domain for the account. Can only be called if `isManaged` returns `Tribool.TRUE`.
     */
    public @Nullable String getManagementDomain() {
        if (isManaged() != Tribool.TRUE) {
            throw new IllegalStateException("The account isn't managed (or the status is unknown)");
        }
        return mHostedDomain;
    }

    /** Gets the account's image. It can be the image user uploaded, monogram or null. */
    public @Nullable Bitmap getAccountImage() {
        return mAccountImage;
    }

    /** Returns the capability values associated with the account. */
    public AccountCapabilities getAccountCapabilities() {
        return mAccountCapabilities;
    }

    /**
     * Returns whether the {@link AccountInfo} has any valid displayable information. The
     * displayable information are full name, given name and avatar.
     */
    public boolean hasDisplayableInfo() {
        return !TextUtils.isEmpty(mFullName)
                || !TextUtils.isEmpty(mGivenName)
                || mAccountImage != null;
    }

    @CalledByNative
    private @Nullable String getRawHostedDomain() {
        return mHostedDomain;
    }
}
