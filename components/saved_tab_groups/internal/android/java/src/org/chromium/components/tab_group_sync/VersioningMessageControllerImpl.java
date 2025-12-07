// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

@JNINamespace("tab_groups")
@NullMarked
public class VersioningMessageControllerImpl implements VersioningMessageController {
    private long mNativePtr;

    @CalledByNative
    private static VersioningMessageControllerImpl create(long nativePtr) {
        return new VersioningMessageControllerImpl(nativePtr);
    }

    private VersioningMessageControllerImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public boolean isInitialized() {
        if (mNativePtr == 0) return false;
        return VersioningMessageControllerImplJni.get().isInitialized(mNativePtr, this);
    }

    @Override
    public boolean shouldShowMessageUi(int messageType) {
        if (mNativePtr == 0) return false;
        return VersioningMessageControllerImplJni.get()
                .shouldShowMessageUi(mNativePtr, this, messageType);
    }

    @Override
    public void shouldShowMessageUiAsync(@MessageType int messageType, Callback<Boolean> callback) {
        if (mNativePtr == 0) {
            callback.onResult(false);
            return;
        }
        VersioningMessageControllerImplJni.get()
                .shouldShowMessageUiAsync(mNativePtr, this, messageType, callback);
    }

    @Override
    public void onMessageUiShown(@MessageType int messageType) {
        if (mNativePtr == 0) return;
        VersioningMessageControllerImplJni.get().onMessageUiShown(mNativePtr, this, messageType);
    }

    @Override
    public void onMessageUiDismissed(@MessageType int messageType) {
        if (mNativePtr == 0) return;
        VersioningMessageControllerImplJni.get()
                .onMessageUiDismissed(mNativePtr, this, messageType);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        boolean isInitialized(
                long nativeVersioningMessageControllerAndroid,
                VersioningMessageControllerImpl caller);

        boolean shouldShowMessageUi(
                long nativeVersioningMessageControllerAndroid,
                VersioningMessageControllerImpl caller,
                int messageType);

        void shouldShowMessageUiAsync(
                long nativeVersioningMessageControllerAndroid,
                VersioningMessageControllerImpl caller,
                int messageType,
                Callback<Boolean> callback);

        void onMessageUiShown(
                long nativeVersioningMessageControllerAndroid,
                VersioningMessageControllerImpl caller,
                int messageType);

        void onMessageUiDismissed(
                long nativeVersioningMessageControllerAndroid,
                VersioningMessageControllerImpl caller,
                int messageType);
    }
}
