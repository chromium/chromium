// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.ui.util.ColorUtils;

/**
 * Holds data used to represent identity provider for display in the "account chooser" dialog.
 */
public class IdentityProviderMetadata {
    private final Integer mBrandTextColor;
    private final Integer mBrandBackgroundColor;
    private final Bitmap mBrandIcon;

    @CalledByNative
    public IdentityProviderMetadata(
            long brandTextColor, long brandBackgroundColor, Bitmap brandIcon) {
        // Parameters are longs because ColorUtils.INVALID_COLOR does not fit in an int.
        mBrandTextColor =
                (brandTextColor == ColorUtils.INVALID_COLOR) ? null : (int) brandTextColor;
        mBrandBackgroundColor = (brandBackgroundColor == ColorUtils.INVALID_COLOR)
                ? null
                : (int) brandBackgroundColor;
        mBrandIcon = brandIcon;
    }

    public @Nullable Integer getBrandTextColor() {
        return mBrandTextColor;
    }

    public @Nullable Integer getBrandBackgroundColor() {
        return mBrandBackgroundColor;
    }

    public Bitmap getBrandIcon() {
        return mBrandIcon;
    }
}
