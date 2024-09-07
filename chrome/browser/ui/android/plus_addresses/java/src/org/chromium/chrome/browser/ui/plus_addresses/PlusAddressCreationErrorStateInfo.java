// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

@JNINamespace("plus_addresses")
/** Contains necessary information to show a meaningful error message to the user. */
class PlusAddressCreationErrorStateInfo {
    private final String mTitle;
    private final String mDescription;
    private final String mOkText;
    private final String mCancelText;

    @VisibleForTesting
    @CalledByNative
    PlusAddressCreationErrorStateInfo(
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String description,
            @JniType("std::u16string") String okText,
            @JniType("std::u16string") String cancelText) {
        mTitle = title;
        mDescription = description;
        mOkText = okText;
        mCancelText = cancelText;
    }

    String getTitle() {
        return mTitle;
    }

    String getDescription() {
        return mDescription;
    }

    String getOkText() {
        return mOkText;
    }

    String getCancelText() {
        return mCancelText;
    }
}
