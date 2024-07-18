// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import java.util.List;

@JNINamespace("plus_addresses")
public class AllPlusAddressesBottomSheetBridge {
    private long mNativeView;

    AllPlusAddressesBottomSheetBridge(long nativeView) {
        mNativeView = nativeView;
    }

    @CalledByNative
    private static AllPlusAddressesBottomSheetBridge create(long nativeView) {
        return new AllPlusAddressesBottomSheetBridge(nativeView);
    }

    @CalledByNative
    private void destroy() {
        mNativeView = 0;
    }

    @CalledByNative
    private void showPlusAddresses(@JniType("std::vector") List<PlusProfile> profiles) {
        // TODO: crbug.com/327838324 - Implement the bottom sheet UI.
    }
}
