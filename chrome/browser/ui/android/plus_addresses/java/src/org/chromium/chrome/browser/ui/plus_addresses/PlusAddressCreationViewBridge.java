// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/**
 * JNI wrapper for C++ PlusAddressCreationViewAndroid.
 */
@JNINamespace("plus_addresses")
public class PlusAddressCreationViewBridge implements PlusAddressCreationDelegate {
    private PlusAddressCreationPrompt mPlusAddressCreationPrompt;

    private long mNativePlusAddressCreationPromptAndroid;

    private PlusAddressCreationViewBridge(long nativePlusAddressCreationPromptAndroid) {
        mNativePlusAddressCreationPromptAndroid = nativePlusAddressCreationPromptAndroid;
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static PlusAddressCreationViewBridge create(long nativePlusAddressCreationPromptAndroid) {
        return new PlusAddressCreationViewBridge(nativePlusAddressCreationPromptAndroid);
    }

    @CalledByNative
    private void show(WindowAndroid windowAndroid, String primaryEmailAddress, String modalTitle) {
        Activity activity = windowAndroid.getActivity().get();
        mPlusAddressCreationPrompt =
                new PlusAddressCreationPrompt(this, activity, primaryEmailAddress, modalTitle);
        mPlusAddressCreationPrompt.show(windowAndroid.getModalDialogManager());
    }

    @Override
    public void onConfirmed() {
        PlusAddressCreationViewBridgeJni.get().onConfirmed(
                mNativePlusAddressCreationPromptAndroid, PlusAddressCreationViewBridge.this);
    }

    @Override
    public void onCanceled() {
        PlusAddressCreationViewBridgeJni.get().onCanceled(
                mNativePlusAddressCreationPromptAndroid, PlusAddressCreationViewBridge.this);
    }

    @Override
    public void onPromptDismissed() {
        PlusAddressCreationViewBridgeJni.get().promptDismissed(
                mNativePlusAddressCreationPromptAndroid, PlusAddressCreationViewBridge.this);
        mNativePlusAddressCreationPromptAndroid = 0;
    }

    @NativeMethods
    interface Natives {
        void onConfirmed(
                long nativePlusAddressCreationViewAndroid, PlusAddressCreationViewBridge caller);
        void onCanceled(
                long nativePlusAddressCreationViewAndroid, PlusAddressCreationViewBridge caller);
        void promptDismissed(
                long nativePlusAddressCreationViewAndroid, PlusAddressCreationViewBridge caller);
    }
}
