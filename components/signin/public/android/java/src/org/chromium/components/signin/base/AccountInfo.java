// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;

/**
 * Stores all the information known about an account.
 *
 * This class has a native counterpart called AccountInfo.
 */
public class AccountInfo extends CoreAccountInfo {
    private final String mFullName;
    private final String mGivenName;
    private final @Nullable Bitmap mAccountImage;
    private final AccountCapabilities mAccountCapabilities;

    @VisibleForTesting
    @CalledByNative
    public AccountInfo(CoreAccountId id, String email, String gaiaId, String fullName,
            String givenName, @Nullable Bitmap accountImage,
            AccountCapabilities accountCapabilities) {
        super(id, email, gaiaId);
        mFullName = fullName;
        mGivenName = givenName;
        mAccountImage = accountImage;
        mAccountCapabilities = accountCapabilities;
    }

    /**
     * @return Full name of the account.
     */
    public String getFullName() {
        return mFullName;
    }

    /**
     * @return Given name of the account.
     */
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

    /**
     * @return the capability values associated with the account.
     */
    public AccountCapabilities getAccountCapabilities() {
        return mAccountCapabilities;
    }

    /**
     * @return Whether the {@link AccountInfo} has any valid displayable information.
     * The displayable information are full name, given name and avatar.
     */
    public boolean hasDisplayableInfo() {
        return !TextUtils.isEmpty(mFullName) || !TextUtils.isEmpty(mGivenName)
                || mAccountImage != null;
    }
}