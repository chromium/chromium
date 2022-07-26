// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.qr_code;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;

/**
 * Delegate for Assistant QR Code Scan actions which forwards events to a native counterpart.
 */
@JNINamespace("autofill_assistant")
public class AssistantQrCodeNativeDelegate implements AssistantQrCodeDelegate {
    private long mNativeAssistantQrCodeNativeDelegate;

    @CalledByNative
    private AssistantQrCodeNativeDelegate(long nativeAssistantQrCodeDelegate) {
        mNativeAssistantQrCodeNativeDelegate = nativeAssistantQrCodeDelegate;
    }

    @Override
    public void onScanResult(String value) {
        if (mNativeAssistantQrCodeNativeDelegate != 0) {
            AssistantQrCodeNativeDelegateJni.get().onScanResult(
                    mNativeAssistantQrCodeNativeDelegate, AssistantQrCodeNativeDelegate.this,
                    value);
        }
    }

    @Override
    public void onScanCancelled() {
        if (mNativeAssistantQrCodeNativeDelegate != 0) {
            AssistantQrCodeNativeDelegateJni.get().onScanCancelled(
                    mNativeAssistantQrCodeNativeDelegate, AssistantQrCodeNativeDelegate.this);
        }
    }

    @Override
    public void onScanFailure() {
        if (mNativeAssistantQrCodeNativeDelegate != 0) {
            AssistantQrCodeNativeDelegateJni.get().onScanFailure(
                    mNativeAssistantQrCodeNativeDelegate, AssistantQrCodeNativeDelegate.this);
        }
    }

    @Override
    public void onCameraError() {
        if (mNativeAssistantQrCodeNativeDelegate != 0) {
            AssistantQrCodeNativeDelegateJni.get().onCameraError(
                    mNativeAssistantQrCodeNativeDelegate, AssistantQrCodeNativeDelegate.this);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantQrCodeNativeDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        void onScanResult(long nativeAssistantQrCodeNativeDelegate,
                AssistantQrCodeNativeDelegate caller, String value);
        void onScanCancelled(
                long nativeAssistantQrCodeNativeDelegate, AssistantQrCodeNativeDelegate caller);
        void onScanFailure(
                long nativeAssistantQrCodeNativeDelegate, AssistantQrCodeNativeDelegate caller);
        void onCameraError(
                long nativeAssistantQrCodeNativeDelegate, AssistantQrCodeNativeDelegate caller);
    }
}
