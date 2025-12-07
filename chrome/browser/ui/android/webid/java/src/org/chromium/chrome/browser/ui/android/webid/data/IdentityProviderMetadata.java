// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

/** Holds data used to represent identity provider for display in the "account chooser" dialog. */
@NullMarked
public class IdentityProviderMetadata {
    private final @Nullable Integer mBrandTextColor;
    private final @Nullable Integer mBrandBackgroundColor;
    private final Bitmap mBrandIconBitmap;
    private final GURL mConfigUrl;
    private final GURL mLoginUrl;
    // Whether use a different account button needs to be shown, whether due to the IDP requesting
    // it or due to filtered out accounts being shown.
    private final boolean mShowUseDifferentAccountButton;

    @CalledByNative
    public IdentityProviderMetadata(
            long brandTextColor,
            long brandBackgroundColor,
            Bitmap brandIconBitmap,
            @JniType("GURL") GURL configUrl,
            @JniType("GURL") GURL loginUrl,
            boolean showUseDifferentAccountButton) {
        // Parameters are longs because ColorUtils.INVALID_COLOR does not fit in an int.
        mBrandTextColor =
                (brandTextColor == ColorUtils.INVALID_COLOR) ? null : (int) brandTextColor;
        mBrandBackgroundColor =
                (brandBackgroundColor == ColorUtils.INVALID_COLOR)
                        ? null
                        : (int) brandBackgroundColor;
        mBrandIconBitmap = brandIconBitmap;
        mConfigUrl = configUrl;
        mLoginUrl = loginUrl;
        mShowUseDifferentAccountButton = showUseDifferentAccountButton;
    }

    public @Nullable Integer getBrandTextColor() {
        return mBrandTextColor;
    }

    public @Nullable Integer getBrandBackgroundColor() {
        return mBrandBackgroundColor;
    }

    public Bitmap getBrandIconBitmap() {
        return mBrandIconBitmap;
    }

    public GURL getConfigUrl() {
        return mConfigUrl;
    }

    public GURL getLoginUrl() {
        return mLoginUrl;
    }

    public boolean showUseDifferentAccountButton() {
        return mShowUseDifferentAccountButton;
    }
}
