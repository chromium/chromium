// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.one_time_tokens.backend.sms;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/**
 * Java-counterpart of the native AndroidSmsOtpFetchReceiverBridge. It's part of the OTP value
 * fetching backend that forwards operation callbacks to the native password manager.
 */
@NullMarked
@JNINamespace("one_time_tokens")
class AndroidSmsOtpFetchReceiverBridge {
    private long mNativeReceiverBridge;

    AndroidSmsOtpFetchReceiverBridge(long nativeReceiverBridge) {
        mNativeReceiverBridge = nativeReceiverBridge;
    }

    @CalledByNative
    static AndroidSmsOtpFetchReceiverBridge create(long nativeReceiverBridge) {
        return new AndroidSmsOtpFetchReceiverBridge(nativeReceiverBridge);
    }

    void onOtpValueRetrieved(String otpValue) {
        if (mNativeReceiverBridge == 0) return;
        AndroidSmsOtpFetchReceiverBridgeJni.get()
                .onOtpValueRetrieved(mNativeReceiverBridge, otpValue);
    }

    void onOtpValueRetrievalError(Exception exception) {
        if (mNativeReceiverBridge == 0) return;

        int errorCode = CommonStatusCodes.ERROR;
        if (exception instanceof ApiException) {
            errorCode = ((ApiException) exception).getStatusCode();
        }
        AndroidSmsOtpFetchReceiverBridgeJni.get()
                .onOtpValueRetrievalError(mNativeReceiverBridge, errorCode);
    }

    @CalledByNative
    private void destroy() {
        mNativeReceiverBridge = 0;
    }

    @NativeMethods
    interface Natives {
        @NativeClassQualifiedName("one_time_tokens::AndroidSmsOtpFetchReceiverBridge")
        void onOtpValueRetrieved(long nativeAndroidSmsOtpFetchReceiverBridge, String otpValue);

        @NativeClassQualifiedName("one_time_tokens::AndroidSmsOtpFetchReceiverBridge")
        void onOtpValueRetrievalError(
                long nativeAndroidSmsOtpFetchReceiverBridge, int apiErrorCode);
    }
}
