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
                ExtensionWindowControllerBridgeImplJni.get()
                        .create(
                                /* caller= */ this,
                                mChromeAndroidTask.getOrCreateNativeBrowserWindowPtr());
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
        /**
         * Creates a native {@code ExtensionWindowControllerBridge}.
         *
         * @param caller The Java object calling this method.
         * @param nativeBrowserWindowPtr The address of a native {@code BrowserWindowInterface}.
         *     It's the caller's responsibility to ensure the validity of the address. Failure to do
         *     so will result in undefined behavior on the native side.
         */
        long create(ExtensionWindowControllerBridgeImpl caller, long nativeBrowserWindowPtr);

        void destroy(long nativeExtensionWindowControllerBridge);
    }
}
