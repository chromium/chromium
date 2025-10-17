// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.util.Pair;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.net.Proxy;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

@JNINamespace("cronet")
final class ProxyCallbackRequestImpl extends Proxy.HttpConnectCallback.Request {
    private final long mProxyCallbackRequestAdapter;
    private boolean mIsConsumed;

    @CalledByNative
    ProxyCallbackRequestImpl(long proxyCallbackRequestAdapter) {
        mProxyCallbackRequestAdapter = proxyCallbackRequestAdapter;
    }

    @Override
    public void proceed(@NonNull List<Pair<String, String>> extraHeaders) {
        if (mIsConsumed) {
            throw new IllegalStateException(
                    "This request has already been consumed: either proceed or close has"
                            + " already been called");
        }
        Objects.requireNonNull(extraHeaders);

        List<String> output = new ArrayList<String>();
        for (Pair<String, String> header : extraHeaders) {
            output.add(header.first);
            output.add(header.second);
        }
        if (!ProxyCallbackRequestImplJni.get()
                .proceed(mProxyCallbackRequestAdapter, output.toArray(new String[output.size()]))) {
            throw new IllegalArgumentException("One of the headers is invalid");
        }
        mIsConsumed = true;
    }

    @Override
    public void close() {
        if (mIsConsumed) {
            return;
        }
        ProxyCallbackRequestImplJni.get().cancel(mProxyCallbackRequestAdapter);
        mIsConsumed = true;
    }

    @NativeMethods
    interface Natives {
        @JniType("bool")
        boolean proceed(
                long nativeProxyCallbackRequestAdapter,
                @JniType("std::vector<std::string>") String[] extraHeaders);

        void cancel(long nativeProxyCallbackRequestAdapter);
    }
}
