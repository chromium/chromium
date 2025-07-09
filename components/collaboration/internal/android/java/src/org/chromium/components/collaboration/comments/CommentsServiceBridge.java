// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.comments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Implementation of {@link CommentsService} that connects to the native counterpart. */
@JNINamespace("collaboration::comments::android")
@NullMarked
/*package*/ class CommentsServiceBridge implements CommentsService {

    private long mNativeCommentsServiceBridge;

    private CommentsServiceBridge(long nativeCommentsServiceBridge) {
        mNativeCommentsServiceBridge = nativeCommentsServiceBridge;
    }

    @CalledByNative
    private static CommentsServiceBridge create(long nativeCommentsServiceBridge) {
        return new CommentsServiceBridge(nativeCommentsServiceBridge);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeCommentsServiceBridge = 0;
    }

    @Override
    public boolean isInitialized() {
        return CommentsServiceBridgeJni.get().isInitialized(mNativeCommentsServiceBridge, this);
    }

    @Override
    public boolean isEmptyService() {
        return CommentsServiceBridgeJni.get().isEmptyService(mNativeCommentsServiceBridge, this);
    }

    @NativeMethods
    interface Natives {
        boolean isInitialized(long nativeCommentsServiceBridge, CommentsServiceBridge caller);

        boolean isEmptyService(long nativeCommentsServiceBridge, CommentsServiceBridge caller);
    }
}
