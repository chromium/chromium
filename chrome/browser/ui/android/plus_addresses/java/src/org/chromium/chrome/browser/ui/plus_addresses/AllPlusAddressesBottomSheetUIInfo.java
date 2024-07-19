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
    private List<PlusProfile> mProfiles;

    @CalledByNative
    AllPlusAddressesBottomSheetUIInfo() {}

    @CalledByNative
    void setPlusProfiles(@JniType("std::vector") List<PlusProfile> profiles) {
        mProfiles = profiles;
    }

    List<PlusProfile> getPlusProfiles() {
        return Collections.unmodifiableList(mProfiles);
    }
}
