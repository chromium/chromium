// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.components.signin.AccountEmailDisplayHook;
import org.chromium.components.signin.Tribool;

import java.util.HashMap;

/**
 * Stores all the information known about an account.
 *
 * This class has a native counterpart called AccountInfo.
 */
public class AccountInfo extends CoreAccountInfo {
    /** Used to instantiate `AccountInfo`. */
    public static class Builder {
        private CoreAccountInfo mCoreAccountInfo;
        private String mFullName = "";
        private String mGivenName = "";
        private @Nullable Bitmap mAccountImage;
        private AccountCapabilities mAccountCapabilities = new AccountCapabilities(new HashMap<>());

        public Builder(String email, String gaiaId) {
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
                    mAccountImage,
                    mAccountCapabilities);
        }
    }

    private final String mFullName;
    private final String mGivenName;
    private final @Nullable Bitmap mAccountImage;
    private AccountCapabilities mAccountCapabilities;

    @VisibleForTesting
    @CalledByNative
    public AccountInfo(
            CoreAccountId id,
            String email,
            String gaiaId,
            String fullName,
            String givenName,
            @Nullable Bitmap accountImage,
            AccountCapabilities accountCapabilities) {
        super(id, email, gaiaId);
        mFullName = fullName;
        mGivenName = givenName;
        mAccountImage = accountImage;
        mAccountCapabilities = accountCapabilities;
    }

    /**
     * @return Whether the account email can be used in display fields.
     * If `AccountCapabilities.canHaveEmailAddressDisplayed()` is not available
     * (Tribool.UNKNOWN), uses fallback.
     */
    public boolean canHaveEmailAddressDisplayed() {
        switch (mAccountCapabilities.canHaveEmailAddressDisplayed()) {
            case Tribool.FALSE:
                {
                    return false;
                }
            case Tribool.TRUE:
                {
                    return true;
                }
        }
        return AccountEmailDisplayHook.canHaveEmailAddressDisplayed(getEmail());
    }

    /** @return Full name of the account. */
    public String getFullName() {
        return mFullName;
    }

    /** @return Given name of the account. */
    public String getGivenName() {
        return mGivenName;
    }

    /**
     * Gets the account's image.
     * It can be the image user uploaded, monogram or null.
     */
    public @Nullable Bitmap getAccountImage() {
        return mAccountImage;
    }

    /** @return the capability values associated with the account. */
    public AccountCapabilities getAccountCapabilities() {
        return mAccountCapabilities;
    }

    /**
     * @return Whether the {@link AccountInfo} has any valid displayable information.
     * The displayable information are full name, given name and avatar.
     */
    public boolean hasDisplayableInfo() {
        return !TextUtils.isEmpty(mFullName)
                || !TextUtils.isEmpty(mGivenName)
                || mAccountImage != null;
    }
}
