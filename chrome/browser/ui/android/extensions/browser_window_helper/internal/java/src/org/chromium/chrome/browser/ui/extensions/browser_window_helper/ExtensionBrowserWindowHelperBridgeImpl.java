// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.browser_window_helper;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature.InitInfo;

/** Implements {@link ExtensionBrowserWindowHelperBridge}. */
@NullMarked
final class ExtensionBrowserWindowHelperBridgeImpl implements ExtensionBrowserWindowHelperBridge {
    private long mNativeExtensionBrowserWindowHelperBridge;

    ExtensionBrowserWindowHelperBridgeImpl() {}

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        ThreadUtils.assertOnUiThread();
        assert mNativeExtensionBrowserWindowHelperBridge == 0
                : "ExtensionBrowserWindowHelperBridge is already added to a task.";

        mNativeExtensionBrowserWindowHelperBridge =
                ExtensionBrowserWindowHelperBridgeImplJni.get()
                        .create(/* caller= */ this, initInfo.nativeBrowserWindowPtr);
    }

    @Override
    public void onFeatureRemoved() {
        ThreadUtils.assertOnUiThread();
        destroyNativeExtensionBrowserWindowHelperBridge();
    }

    private void destroyNativeExtensionBrowserWindowHelperBridge() {
        if (mNativeExtensionBrowserWindowHelperBridge != 0) {
            ExtensionBrowserWindowHelperBridgeImplJni.get()
                    .destroy(mNativeExtensionBrowserWindowHelperBridge);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeExtensionBrowserWindowHelperBridge = 0;
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code ExtensionBrowserWindowHelperBridge}.
         *
         * @param caller The Java object calling this method.
         * @param nativeBrowserWindowPtr The address of a native {@code BrowserWindowInterface}.
         *     It's the caller's responsibility to ensure the validity of the address. Failure to do
         *     so will result in undefined behavior on the native side.
         */
        long create(ExtensionBrowserWindowHelperBridgeImpl caller, long nativeBrowserWindowPtr);

        void destroy(long nativeExtensionBrowserWindowHelperBridge);
    }
}
