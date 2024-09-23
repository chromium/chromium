// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import org.jni_zero.CalledByNative;

import org.chromium.url.GURL;

/**
 * This class holds the data used to represent client ID metadata for display
 * in the account chooser dialog.
 */
public class ClientIdMetadata {
    private final GURL mTermsOfServiceUrl;
    private final GURL mPrivacyPolicyUrl;
    private final String mBrandIconUrl;

    /**
     * @param termsOfServiceUrl URL for the terms of service for this client ID.
     * @param privacyPolicyUrl URL for the privacy policy for this client ID.
     * @param brandIconUrl String of URL for the brand icon for this client ID.
     */
    @CalledByNative
    public ClientIdMetadata(GURL termsOfServiceUrl, GURL privacyPolicyUrl, String brandIconUrl) {
        mTermsOfServiceUrl = termsOfServiceUrl;
        mPrivacyPolicyUrl = privacyPolicyUrl;
        mBrandIconUrl = brandIconUrl;
    }

    public GURL getTermsOfServiceUrl() {
        return mTermsOfServiceUrl;
    }

    public GURL getPrivacyPolicyUrl() {
        return mPrivacyPolicyUrl;
    }

    public String getBrandIconUrl() {
        return mBrandIconUrl;
    }
}
