// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

@JNINamespace("plus_addresses")
public class AllPlusAddressesBottomSheetBridge
        implements AllPlusAddressesBottomSheetCoordinator.Delegate {
    private long mNativeView;

    private final AllPlusAddressesBottomSheetCoordinator mAllPlusAddressesBottomSheetCoordinator;

    AllPlusAddressesBottomSheetBridge(
            long nativeView,
            Context context,
            BottomSheetController bottomSheetController,
            Profile profile) {
        mNativeView = nativeView;
        mAllPlusAddressesBottomSheetCoordinator =
                new AllPlusAddressesBottomSheetCoordinator(
                        context,
                        bottomSheetController,
                        /* delegate= */ this,
                        FaviconHelper.create(context, profile));
    }

    @CalledByNative
    private static @Nullable AllPlusAddressesBottomSheetBridge create(
            long nativeView, @Nullable WindowAndroid windowAndroid, Profile profile) {
        if (windowAndroid == null) {
            return null;
        }
        Context context = windowAndroid.getActivity().get();
        if (context == null) {
            return null;
        }
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) {
            return null;
        }
        return new AllPlusAddressesBottomSheetBridge(
                nativeView, context, bottomSheetController, profile);
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
        if (mNativeView != 0) { // The native side is already dismissed.
            AllPlusAddressesBottomSheetBridgeJni.get()
                    .onPlusAddressSelected(mNativeView, plusAddress);
        }
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
