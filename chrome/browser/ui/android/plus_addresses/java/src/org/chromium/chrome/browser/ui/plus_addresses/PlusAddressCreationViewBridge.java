// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.WindowAndroid;

/** JNI wrapper for C++ PlusAddressCreationViewAndroid. */
@JNINamespace("plus_addresses")
public class PlusAddressCreationViewBridge extends EmptyBottomSheetObserver
        implements PlusAddressCreationDelegate {
    private long mNativePlusAddressCreationPromptAndroid;
    private Activity mActivity;
    private BottomSheetController mBottomSheetController;
    private PlusAddressCreationBottomSheetContent mBottomSheetContent;

    private PlusAddressCreationViewBridge(
            long nativePlusAddressCreationPromptAndroid, WindowAndroid window) {
        mNativePlusAddressCreationPromptAndroid = nativePlusAddressCreationPromptAndroid;
        mActivity = window.getActivity().get();
        mBottomSheetController = BottomSheetControllerProvider.from(window);
        mBottomSheetController.addObserver(this);
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static PlusAddressCreationViewBridge create(
            long nativePlusAddressCreationPromptAndroid, WindowAndroid window) {
        return new PlusAddressCreationViewBridge(nativePlusAddressCreationPromptAndroid, window);
    }

    @CalledByNative
    void show(
            String modalTitle,
            String plusAddressDescription,
            String proposedPlusAddressPlaceholder,
            String plusAddressModalOkText,
            String plusAddressModalCancelText) {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            mBottomSheetContent =
                    new PlusAddressCreationBottomSheetContent(
                            this,
                            mActivity,
                            modalTitle,
                            plusAddressDescription,
                            proposedPlusAddressPlaceholder,
                            plusAddressModalOkText,
                            plusAddressModalCancelText);

            mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
        }
    }

    // Hide the bottom sheet (if showing) and clean up observers.
    @CalledByNative
    void destroy() {
        mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ false);
        mBottomSheetController.removeObserver(this);
        mNativePlusAddressCreationPromptAndroid = 0;
    }

    // EmptyBottomSheetObserver overridden methods:
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        this.onPromptDismissed();
    }

    // PlusAddressCreationDelegate implementation:
    @Override
    public void onConfirmed() {
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .onConfirmed(
                            mNativePlusAddressCreationPromptAndroid,
                            PlusAddressCreationViewBridge.this);
        }
    }

    @Override
    public void onCanceled() {
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .onCanceled(
                            mNativePlusAddressCreationPromptAndroid,
                            PlusAddressCreationViewBridge.this);
        }
    }

    @Override
    public void onPromptDismissed() {
        if (mNativePlusAddressCreationPromptAndroid != 0) {
            PlusAddressCreationViewBridgeJni.get()
                    .promptDismissed(
                            mNativePlusAddressCreationPromptAndroid,
                            PlusAddressCreationViewBridge.this);
            mNativePlusAddressCreationPromptAndroid = 0;
        }
    }

    @VisibleForTesting
    public PlusAddressCreationBottomSheetContent getBottomSheetContent() {
        return mBottomSheetContent;
    }

    public void setActivityForTesting(Activity activity) {
        mActivity = activity;
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
