// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;

/**
 * Stores all the information known about an account.
 *
 * This class has a native counterpart called AccountInfo.
 */
public class AccountInfo extends CoreAccountInfo {
    private final @Nullable Bitmap mAccountImage;

    @VisibleForTesting
    @CalledByNative
    public AccountInfo(
            CoreAccountId id, String email, String gaiaId, @Nullable Bitmap accountImage) {
        super(id, email, gaiaId);
        mAccountImage = accountImage;
    }

    /**
     * Gets the account's image.
     * It can be the image user uploaded, monogram or null.
     */
    public @Nullable Bitmap getAccountImage() {
        return mAccountImage;
    }
}