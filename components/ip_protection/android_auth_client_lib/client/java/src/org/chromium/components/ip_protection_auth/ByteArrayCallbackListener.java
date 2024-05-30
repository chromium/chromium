// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** ByteArrayCallbackListener uses JNI to notify native code when RPCs to the auth server return. */
@JNINamespace("ip_protection::android")
final class ByteArrayCallbackListener implements IpProtectionByteArrayCallback {
    private long mNativeListener;

    @Override
    public void onResult(byte[] response) {
        assert mNativeListener != 0;
        ByteArrayCallbackListenerJni.get().onResult(mNativeListener, response);
        mNativeListener = 0;
    }

    @Override
    public void onError(byte[] error) {
        assert mNativeListener != 0;
        ByteArrayCallbackListenerJni.get().onError(mNativeListener, error);
        mNativeListener = 0;
    }

    @CalledByNative
    private ByteArrayCallbackListener(long nativeListener) {
        mNativeListener = nativeListener;
    }

    @NativeMethods
    interface Natives {
        void onResult(long nativeByteArrayCallbackListener, byte[] response);

        void onError(long nativeByteArrayCallbackListener, byte[] error);
    }
}
