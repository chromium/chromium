// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/** Holds data used to represent an error for display in the "error" dialog. */
@NullMarked
public class IdentityCredentialTokenError {
    private final String mCode;
    private final GURL mUrl;

    @CalledByNative
    public IdentityCredentialTokenError(
            @JniType("std::string") String code, @JniType("GURL") GURL url) {
        mCode = code;
        mUrl = url;
    }

    public String getCode() {
        return mCode;
    }

    public GURL getUrl() {
        return mUrl;
    }
}
