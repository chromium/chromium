// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * JNI glue for the {@link OtpVerificationDialogView}. This allows the native code to show/dismiss
 * the OTP verification dialog and also show an error message when OTP verification fails.
 */
@JNINamespace("autofill")
class OtpVerificationDialogBridge implements OtpVerificationDialogCoordinator.Delegate {
    private final long mNativeOtpVerificationDialogView;
    private OtpVerificationDialogCoordinator mDialogCoordinator;

    OtpVerificationDialogBridge(
            long nativeOtpVerificationDialogView,
            Context context,
            ModalDialogManager modalDialogManager) {
        this.mNativeOtpVerificationDialogView = nativeOtpVerificationDialogView;
        mDialogCoordinator =
                OtpVerificationDialogCoordinator.create(context, modalDialogManager, this);
    }

    /**
     * Create an instance of the {@link OtpVerificationDialogBridge} that can be used by the native
     * code to call different actions on.
     *
     * @param nativeOtpVerificationDialogView The pointer to the native object.
     * @param windowAndroid The current {@link WindowAndroid} object.
     */
    @CalledByNative
    static @Nullable OtpVerificationDialogBridge create(
            long nativeOtpVerificationDialogView, WindowAndroid windowAndroid) {
        Context context = windowAndroid.getActivity().get();
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (context == null || modalDialogManager == null) {
            return null;
        }
        return new OtpVerificationDialogBridge(
                nativeOtpVerificationDialogView, context, modalDialogManager);
    }

    @Override
    public void onConfirm(String otp) {
        OtpVerificationDialogBridgeJni.get().onConfirm(mNativeOtpVerificationDialogView, otp);
    }

    @Override
    public void onNewOtpRequested() {
        OtpVerificationDialogBridgeJni.get().onNewOtpRequested(mNativeOtpVerificationDialogView);
    }

    @Override
    public void onDialogDismissed() {
        OtpVerificationDialogBridgeJni.get().onDialogDismissed(mNativeOtpVerificationDialogView);
    }

    /**
     * Show the OTP verification dialog to allow the user to input an OTP.
     *
     * @param otpLength The expected length of the OTP. This is used for showing a hint in the input
     *         field as well as some basic error handling.
     */
    @CalledByNative
    void showDialog(int otpLength) {
        mDialogCoordinator.show(otpLength);
    }

    /**
     * Show an error message after the user submitted an OTP.
     *
     * @param errorMessage The error message to be displayed below the OTP input field.
     */
    @CalledByNative
    void showOtpErrorMessage(String errorMessage) {
        mDialogCoordinator.showOtpErrorMessage(errorMessage);
    }

    /** Dismiss the dialog if it is already showing. */
    @CalledByNative
    void dismissDialog() {
        mDialogCoordinator.dismissDialog();
    }

    @CalledByNative
    void showConfirmationAndDismissDialog(String confirmationMessage) {
        mDialogCoordinator.showConfirmationAndDismissDialog(confirmationMessage);
    }

    @NativeMethods
    interface Natives {
        void onConfirm(long nativeOtpVerificationDialogViewAndroid, String otp);

        void onNewOtpRequested(long nativeOtpVerificationDialogViewAndroid);

        void onDialogDismissed(long nativeOtpVerificationDialogViewAndroid);
    }
}
