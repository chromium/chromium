// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/**
 * This class holds the data used to represent client ID metadata for display
 * in the account chooser dialog.
 */
@NullMarked
public class ClientIdMetadata {
    private final GURL mTermsOfServiceUrl;
    private final GURL mPrivacyPolicyUrl;
    private final Bitmap mBrandIconBitmap;

    /**
     * @param termsOfServiceUrl URL for the terms of service for this client ID.
     * @param privacyPolicyUrl URL for the privacy policy for this client ID.
     * @param brandIconBitmap Bitmap of the brand icon for this client ID.
     */
    @CalledByNative
    public ClientIdMetadata(
            @JniType("GURL") GURL termsOfServiceUrl,
            @JniType("GURL") GURL privacyPolicyUrl,
            Bitmap brandIconBitmap) {
        mTermsOfServiceUrl = termsOfServiceUrl;
        mPrivacyPolicyUrl = privacyPolicyUrl;
        mBrandIconBitmap = brandIconBitmap;
    }

    public GURL getTermsOfServiceUrl() {
        return mTermsOfServiceUrl;
    }

    public GURL getPrivacyPolicyUrl() {
        return mPrivacyPolicyUrl;
    }

    public Bitmap getBrandIconBitmap() {
        return mBrandIconBitmap;
    }
}
