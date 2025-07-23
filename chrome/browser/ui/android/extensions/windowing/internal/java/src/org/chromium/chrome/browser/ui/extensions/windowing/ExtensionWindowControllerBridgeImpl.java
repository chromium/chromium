// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.windowing;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

/** Implements {@link ExtensionWindowControllerBridge}. */
@NullMarked
final class ExtensionWindowControllerBridgeImpl implements ExtensionWindowControllerBridge {

    @SuppressWarnings("UnusedVariable")
    private final ChromeAndroidTask mChromeAndroidTask;

    private long mNativeExtensionWindowControllerBridge;

    ExtensionWindowControllerBridgeImpl(ChromeAndroidTask chromeAndroidTask) {
        mChromeAndroidTask = chromeAndroidTask;
    }

    @Override
    public void onAddedToTask() {
        assert mNativeExtensionWindowControllerBridge == 0
                : "ExtensionWindowControllerBridge is already added to a task.";

        mNativeExtensionWindowControllerBridge =
                ExtensionWindowControllerBridgeImplJni.get().create(this);
    }

    @Override
    public void onTaskRemoved() {
        if (mNativeExtensionWindowControllerBridge != 0) {
            ExtensionWindowControllerBridgeImplJni.get()
                    .destroy(mNativeExtensionWindowControllerBridge);
        }
    }

    long getNativePtrForTesting() {
        return mNativeExtensionWindowControllerBridge;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeExtensionWindowControllerBridge = 0;
    }

    @NativeMethods
    interface Natives {
        long create(ExtensionWindowControllerBridgeImpl caller);

        void destroy(long nativeExtensionWindowControllerBridge);
    }
}
