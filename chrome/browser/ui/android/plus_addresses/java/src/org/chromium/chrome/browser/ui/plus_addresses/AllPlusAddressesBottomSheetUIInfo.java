// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import java.util.Collections;
import java.util.List;

@JNINamespace("plus_addresses")
class AllPlusAddressesBottomSheetUIInfo {
    private String mTitle;
    // The message displayed below the bottom sheet title.
    private String mWarning;
    private String mQueryHint;
    private List<PlusProfile> mProfiles;

    @CalledByNative
    AllPlusAddressesBottomSheetUIInfo() {}

    @CalledByNative
    void setTitle(@JniType("std::u16string") String title) {
        mTitle = title;
    }

    @CalledByNative
    void setWarning(@JniType("std::u16string") String warning) {
        mWarning = warning;
    }

    @CalledByNative
    void setQueryHint(@JniType("std::u16string") String queryHint) {
        mQueryHint = queryHint;
    }

    @CalledByNative
    void setPlusProfiles(@JniType("std::vector") List<PlusProfile> profiles) {
        mProfiles = profiles;
    }

    String getTitle() {
        return mTitle;
    }

    String getWarning() {
        return mWarning;
    }

    String getQueryHint() {
        return mQueryHint;
    }

    List<PlusProfile> getPlusProfiles() {
        return Collections.unmodifiableList(mProfiles);
    }
}
