// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * ByteArrayCallbackListener uses JNI to notify native code when RPCs to the auth server return.
 *
 * <p>A Java ByteArrayCallbackListener object is created by the corresponding native object (see
 * byte_array_callback_listener.h). Whilst the native side will self-delete after the callback is
 * used, references to this Java object will outlive the corresponding native object.
 *
 * <p>The callback MUST only be called once.
 */
@JNINamespace("ip_protection::android")
final class ByteArrayCallbackListener implements IpProtectionByteArrayCallback {
    private long mNativeListener;

    @Override
    public void onResult(@NonNull byte[] response) {
        if (mNativeListener == 0) {
            throw new IllegalStateException("callback already used");
        }
        ByteArrayCallbackListenerJni.get().onResult(mNativeListener, response);
        mNativeListener = 0;
    }

    @Override
    public void onError(int authRequestError) {
        if (mNativeListener == 0) {
            throw new IllegalStateException("callback already used");
        }
        ByteArrayCallbackListenerJni.get().onError(mNativeListener, authRequestError);
        mNativeListener = 0;
    }

    @CalledByNative
    private ByteArrayCallbackListener(long nativeListener) {
        mNativeListener = nativeListener;
    }

    @NativeMethods
    interface Natives {
        void onResult(long nativeByteArrayCallbackListener, byte[] response);

        void onError(long nativeByteArrayCallbackListener, int authRequestError);
    }
}
