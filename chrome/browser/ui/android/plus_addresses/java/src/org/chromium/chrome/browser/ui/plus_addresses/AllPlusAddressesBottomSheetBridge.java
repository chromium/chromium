// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

@JNINamespace("plus_addresses")
public class AllPlusAddressesBottomSheetBridge
        implements AllPlusAddressesBottomSheetCoordinator.Delegate {
    private long mNativeView;
    private final AllPlusAddressesBottomSheetCoordinator mAllPlusAddressesBottomSheetCoordinator;

    AllPlusAddressesBottomSheetBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        mAllPlusAddressesBottomSheetCoordinator =
                new AllPlusAddressesBottomSheetCoordinator(
                        windowAndroid.getActivity().get(),
                        BottomSheetControllerProvider.from(windowAndroid),
                        /* delegate= */ this);
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

    @Override
    public void onPlusAddressSelected(String plusAddress) {
        assert mNativeView != 0 : "The native side is already dismissed";
        AllPlusAddressesBottomSheetBridgeJni.get().onPlusAddressSelected(mNativeView, plusAddress);
    }

    @Override
    public void onDismissed() {
        // When the plus address is selected and passed back to the C++ backend via
        // `onPlusAddressSelected()`, the native object is destroyed. The bottom sheet state change
        // listener can be triggered afterwards, which should not result is any native method calls.
        if (mNativeView != 0) {
            AllPlusAddressesBottomSheetBridgeJni.get().onDismissed(mNativeView);
        }
    }

    @NativeMethods
    interface Natives {
        void onPlusAddressSelected(
                long nativeAllPlusAddressesBottomSheetView,
                @JniType("std::string") String plusAddress);

        void onDismissed(long nativeAllPlusAddressesBottomSheetView);
    }
}
