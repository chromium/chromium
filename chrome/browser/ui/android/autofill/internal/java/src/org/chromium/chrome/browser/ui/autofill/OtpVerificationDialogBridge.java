// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * JNI glue for the {@link OtpVerificationDialog}. This allows the native code to show/dismiss the
 * OTP verification dialog and also show an error message when OTP verification fails.
 */
@JNINamespace("autofill")
public class OtpVerificationDialogBridge implements OtpVerificationDialog.Listener {
    private final long mNativeOtpVerificationDialogView;
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private OtpVerificationDialog mOtpVerificationDialog;

    public OtpVerificationDialogBridge(long nativeOtpVerificationDialogView, Context context,
            ModalDialogManager modalDialogManager) {
        this.mNativeOtpVerificationDialogView = nativeOtpVerificationDialogView;
        this.mModalDialogManager = modalDialogManager;
        this.mContext = context;
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
     * Create an instance of the {@link OtpVerificationDialogBridge} that can be used by the native
     * code to call different actions on.
     *
     * @param nativeOtpVerificationDialogView The pointer to the native object.
     * @param windowAndroid The current {@link WindowAndroid} object.
     * @return
     */
    @CalledByNative
    @Nullable
    public static OtpVerificationDialogBridge create(
            long nativeOtpVerificationDialogView, WindowAndroid windowAndroid) {
        Context context = windowAndroid.getActivity().get();
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (context == null || modalDialogManager == null) {
            return null;
        }
        return new OtpVerificationDialogBridge(
                nativeOtpVerificationDialogView, context, modalDialogManager);
    }

    /**
     * Show the OTP verification dialog to allow the user to input an OTP.
     *
     * @param otpLength The expected length of the OTP. This is used for showing a hint in the input
     *         field as well as some basic error handling.
     */
    @CalledByNative
    public void showDialog(int otpLength) {
        mOtpVerificationDialog = new OtpVerificationDialog(mContext, this, mModalDialogManager);
        mOtpVerificationDialog.show(otpLength);
    }

    /**
     * Show an error message after the user submitted an OTP.
     *
     * @param errorMessage The error message to be displayed below the OTP input field.
     */
    @CalledByNative
    public void showOtpErrorMessage(String errorMessage) {
        mOtpVerificationDialog.showOtpErrorMessage(errorMessage);
    }

    /**
     * Dismiss the dialog if it is already showing.
     */
    @CalledByNative
    public void dismissDialog() {
        mOtpVerificationDialog.dismissDialog();
    }

    @NativeMethods
    interface Natives {
        void onConfirm(long nativeOtpVerificationDialogViewAndroid, String otp);
        void onNewOtpRequested(long nativeOtpVerificationDialogViewAndroid);
        void onDialogDismissed(long nativeOtpVerificationDialogViewAndroid);
    }
}
