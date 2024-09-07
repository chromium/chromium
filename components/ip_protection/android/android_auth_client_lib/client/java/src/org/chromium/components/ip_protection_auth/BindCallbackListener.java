// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

/** BindCallbackListener uses JNI to notify native code when the IP Protection service is bound. */
@JNINamespace("ip_protection::android")
final class BindCallbackListener implements IpProtectionAuthServiceCallback {
    private long mNativeListener;

    @Override
    public void onResult(IpProtectionAuthClient client) {
        if (mNativeListener == 0) {
            throw new IllegalStateException("callback already used");
        }
        BindCallbackListenerJni.get().onResult(mNativeListener, client);
        mNativeListener = 0;
    }

    @Override
    public void onError(String error) {
        if (mNativeListener == 0) {
            throw new IllegalStateException("callback already used");
        }
        BindCallbackListenerJni.get().onError(mNativeListener, error);
        mNativeListener = 0;
    }

    @CalledByNative
    private BindCallbackListener(long nativeListener) {
        mNativeListener = nativeListener;
    }

    @NativeMethods
    interface Natives {
        void onResult(long nativeBindCallbackListener, IpProtectionAuthClient client);

        void onError(long nativeBindCallbackListener, @JniType("std::string") String error);
    }
}
