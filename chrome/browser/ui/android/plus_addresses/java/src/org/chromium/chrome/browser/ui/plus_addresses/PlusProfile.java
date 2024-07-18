// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

// Android plain old Java object class that is used to display the user's plus addresses in the all
// plus addresses bottom sheet.
@JNINamespace("plus_addresses")
class PlusProfile {
    // Plus address email address.
    private String mPlusAddress;
    // The domain where the plus address was created.
    private String mOrigin;

    @CalledByNative
    public PlusProfile(
            @JniType("std::string") String plusAddress, @JniType("std::string") String origin) {
        mPlusAddress = plusAddress;
        mOrigin = origin;
    }

    public String getPlusAddress() {
        return mPlusAddress;
    }

    public String getOrigin() {
        return mOrigin;
    }
}
