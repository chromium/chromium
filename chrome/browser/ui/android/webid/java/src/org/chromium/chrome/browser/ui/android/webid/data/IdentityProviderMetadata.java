// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

/** Holds data used to represent identity provider for display in the "account chooser" dialog. */
public class IdentityProviderMetadata {
    private final Integer mBrandTextColor;
    private final Integer mBrandBackgroundColor;
    private final String mBrandIconUrl;
    private final GURL mConfigUrl;
    private final GURL mLoginUrl;
    private final boolean mSupportsAddAccount;

    @CalledByNative
    public IdentityProviderMetadata(
            long brandTextColor,
            long brandBackgroundColor,
            String brandIconUrl,
            GURL configUrl,
            GURL loginUrl,
            boolean supportsAddAccount) {
        // Parameters are longs because ColorUtils.INVALID_COLOR does not fit in an int.
        mBrandTextColor =
                (brandTextColor == ColorUtils.INVALID_COLOR) ? null : (int) brandTextColor;
        mBrandBackgroundColor =
                (brandBackgroundColor == ColorUtils.INVALID_COLOR)
                        ? null
                        : (int) brandBackgroundColor;
        mBrandIconUrl = brandIconUrl;
        mConfigUrl = configUrl;
        mLoginUrl = loginUrl;
        mSupportsAddAccount = supportsAddAccount;
    }

    public @Nullable Integer getBrandTextColor() {
        return mBrandTextColor;
    }

    public @Nullable Integer getBrandBackgroundColor() {
        return mBrandBackgroundColor;
    }

    public String getBrandIconUrl() {
        return mBrandIconUrl;
    }

    public GURL getConfigUrl() {
        return mConfigUrl;
    }

    public GURL getLoginUrl() {
        return mLoginUrl;
    }

    public boolean supportsAddAccount() {
        return mSupportsAddAccount;
    }
}
