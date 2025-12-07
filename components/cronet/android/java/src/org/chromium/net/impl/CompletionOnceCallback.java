// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.net.NetError;

/**
 * Holds a net::CompletionOnceCallback that is waiting for something owned by a Java object.
 * TODO(https://crbug.com/442514750): Consider moving to org.chromium.base.JniOnceCallback instead.
 */
@JNINamespace("cronet")
final class CompletionOnceCallback implements AutoCloseable {
    private final long mCompletionOnceCallbackAdapter;
    private boolean mIsConsumed;

    @CalledByNative
    CompletionOnceCallback(long completionOnceCallbackAdapter) {
        mCompletionOnceCallbackAdapter = completionOnceCallbackAdapter;
    }

    /**
     * Runs the associated net::CompletionOnceCallback. For more info refer to
     * cronet::CompletionOnceCallbackAdapter's documentation.
     */
    public void run(@NetError int result) {
        if (mIsConsumed) {
            throw new IllegalStateException("This callback can only be run once");
        }
        CompletionOnceCallbackJni.get().run(mCompletionOnceCallbackAdapter, result);
        mIsConsumed = true;
    }

    /** Frees the associated native resources. */
    @Override
    public void close() {
        if (!mIsConsumed) {
            // This can only be reached when `run` was not called first. This always represents a
            // bug within Cronet's code. We could call `run` with a different error code, but we are
            // already in an undefined state, so crash instead.
            throw new AssertionError("run should always be called prior to close");
        }
    }

    @NativeMethods
    interface Natives {
        void run(
                long nativeCompletionOnceCallbackAdapter,
                @JniType("net::Error") @NetError int result);
    }
}
