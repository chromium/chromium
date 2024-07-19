// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

@JNINamespace("plus_addresses")
public class AllPlusAddressesBottomSheetBridge {
    private long mNativeView;
    private final AllPlusAddressesBottomSheetCoordinator mAllPlusAddressesBottomSheetCoordinator;

    AllPlusAddressesBottomSheetBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        mAllPlusAddressesBottomSheetCoordinator =
                new AllPlusAddressesBottomSheetCoordinator(
                        windowAndroid.getActivity().get(),
                        BottomSheetControllerProvider.from(windowAndroid));
    }

    @CalledByNative
    private static AllPlusAddressesBottomSheetBridge create(
            long nativeView, WindowAndroid windowAndroid) {
        return new AllPlusAddressesBottomSheetBridge(nativeView, windowAndroid);
    }

    @CalledByNative
    private void destroy() {
        mNativeView = 0;
    }

    @CalledByNative
    private void showPlusAddresses(AllPlusAddressesBottomSheetUIInfo uiInfo) {
        mAllPlusAddressesBottomSheetCoordinator.showPlusProfiles(uiInfo);
    }
}
