// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * JNI glue for the {@link OtpVerificationDialogView}. This allows the native code to show/dismiss
 * the OTP verification dialog and also show an error message when OTP verification fails.
 */
@JNINamespace("autofill")
@NullMarked
class OtpVerificationDialogBridge implements OtpVerificationDialogCoordinator.Delegate {
    @JniType("autofill::OtpVerificationDialogViewAndroid")
    interface NativeOtpVerificationDialogViewAndroid extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeOtpVerificationDialogViewAndroid>
            mNativeOtpVerificationDialogView;
    private final OtpVerificationDialogCoordinator mDialogCoordinator;

    OtpVerificationDialogBridge(
            @Nullable JniRawPtr<NativeOtpVerificationDialogViewAndroid>
                    nativeOtpVerificationDialogView,
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
            JniRawPtr<NativeOtpVerificationDialogViewAndroid> nativeOtpVerificationDialogView,
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid) {
        Context context = windowAndroid.getActivity().get();
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (context == null || modalDialogManager == null) {
            nativeOtpVerificationDialogView.release();
            return null;
        }
        return new OtpVerificationDialogBridge(
                nativeOtpVerificationDialogView, context, modalDialogManager);
    }

    @Override
    public void onConfirm(String otp) {
        if (mNativeOtpVerificationDialogView == null) return;
        OtpVerificationDialogBridgeJni.get().onConfirm(mNativeOtpVerificationDialogView, otp);
    }

    @Override
    public void onNewOtpRequested() {
        if (mNativeOtpVerificationDialogView == null) return;
        OtpVerificationDialogBridgeJni.get().onNewOtpRequested(mNativeOtpVerificationDialogView);
    }

    @Override
    public void onDialogDismissed() {
        if (mNativeOtpVerificationDialogView == null) return;
        OtpVerificationDialogBridgeJni.get().onDialogDismissed(mNativeOtpVerificationDialogView);
        clearNativePtr();
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
    void showOtpErrorMessage(@JniType("std::u16string") String errorMessage) {
        mDialogCoordinator.showOtpErrorMessage(errorMessage);
    }

    /** Dismiss the dialog if it is already showing. */
    @CalledByNative
    void dismissDialog() {
        mDialogCoordinator.dismissDialog();
    }

    @CalledByNative
    void showConfirmationAndDismissDialog(@JniType("std::u16string") String confirmationMessage) {
        mDialogCoordinator.showConfirmationAndDismissDialog(confirmationMessage);
    }

    @CalledByNative
    void clearNativePtr() {
        if (mNativeOtpVerificationDialogView != null) {
            mNativeOtpVerificationDialogView.release();
            mNativeOtpVerificationDialogView = null;
        }
    }

    @NativeMethods
    interface Natives {
        void onConfirm(
                JniPtr<NativeOtpVerificationDialogViewAndroid> self,
                @JniType("std::u16string") String otp);

        void onNewOtpRequested(JniPtr<NativeOtpVerificationDialogViewAndroid> self);

        void onDialogDismissed(JniPtr<NativeOtpVerificationDialogViewAndroid> self);
    }
}
