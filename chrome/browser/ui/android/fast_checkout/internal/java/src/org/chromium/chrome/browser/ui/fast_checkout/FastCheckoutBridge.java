// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

import java.util.Arrays;
import java.util.List;

/**
 * This bridge creates and initializes a {@link FastCheckoutComponent} on construction and forwards
 * native calls to it.
 */
class FastCheckoutBridge implements FastCheckoutComponent.Delegate {
    private long mNativeFastCheckoutBridge;
    private final FastCheckoutComponent mFastCheckoutComponent;

    private FastCheckoutBridge(
            long nativeBridge,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController) {
        mNativeFastCheckoutBridge = nativeBridge;
        mFastCheckoutComponent = new FastCheckoutCoordinator();
        mFastCheckoutComponent.initialize(
                windowAndroid.getContext().get(), bottomSheetController, this);
    }

    @CalledByNative
    private static @Nullable FastCheckoutBridge create(
            long nativeBridge, WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        return new FastCheckoutBridge(nativeBridge, windowAndroid, bottomSheetController);
    }

    @CalledByNative
    private void showBottomSheet(
            @JniType("std::vector") Object[] profiles,
            @JniType("std::vector") Object[] creditCards) {
        mFastCheckoutComponent.showOptions(
                (List<FastCheckoutAutofillProfile>) (List<?>) Arrays.asList(profiles),
                (List<FastCheckoutCreditCard>) (List<?>) Arrays.asList(creditCards));
    }

    @CalledByNative
    private void destroy() {
        mNativeFastCheckoutBridge = 0;
        mFastCheckoutComponent.destroy();
    }

    @Override
    public void onDismissed() {
        if (mNativeFastCheckoutBridge != 0) {
            FastCheckoutBridgeJni.get().onDismiss(mNativeFastCheckoutBridge);
        }
    }

    @Override
    public void onOptionsSelected(
            FastCheckoutAutofillProfile profile, FastCheckoutCreditCard creditCard) {
        if (mNativeFastCheckoutBridge != 0) {
            FastCheckoutBridgeJni.get()
                    .onOptionsSelected(mNativeFastCheckoutBridge, profile, creditCard);
        }
    }

    @Override
    public void openAutofillProfileSettings() {
        if (mNativeFastCheckoutBridge != 0) {
            FastCheckoutBridgeJni.get().openAutofillProfileSettings(mNativeFastCheckoutBridge);
        }
    }

    @Override
    public void openCreditCardSettings() {
        if (mNativeFastCheckoutBridge != 0) {
            FastCheckoutBridgeJni.get().openCreditCardSettings(mNativeFastCheckoutBridge);
        }
    }

    @NativeMethods
    interface Natives {
        void onOptionsSelected(
                long nativeFastCheckoutViewImpl,
                FastCheckoutAutofillProfile profile,
                FastCheckoutCreditCard creditCard);

        void onDismiss(long nativeFastCheckoutViewImpl);

        void openAutofillProfileSettings(long nativeFastCheckoutViewImpl);

        void openCreditCardSettings(long nativeFastCheckoutViewImpl);
    }
}
