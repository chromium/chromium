// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Implements {@link WindowScopedSidePanelRegistryBridge}. */
@NullMarked
final class WindowScopedSidePanelRegistryBridgeImpl implements WindowScopedSidePanelRegistryBridge {
    private static final String TAG = "WindowScopedSidePanelRegistryBridgeImpl";

    private long mNativeWindowScopedSidePanelRegistryBridge;

    WindowScopedSidePanelRegistryBridgeImpl() {
        log(TAG, "constructor");
    }

    @Override
    public void onAddedToTask(long nativeBrowserWindowPtr) {
        log(TAG, "onAddedToTask", nativeBrowserWindowPtr);
        createNativePtr(nativeBrowserWindowPtr);
    }

    @Override
    public void onFeatureRemoved() {
        log(TAG, "onFeatureRemoved");
        destroyNativePtr();
    }

    @VisibleForTesting
    long createNativePtr(long nativeBrowserWindowPtr) {
        log(TAG, "createNativePtr", nativeBrowserWindowPtr);
        assert nativeBrowserWindowPtr != 0
                : "Native BrowserWindowInterface pointer shouldn't be null. Is the"
                        + " ChromeAndroidTaskFeatureKey correct?";
        assert mNativeWindowScopedSidePanelRegistryBridge == 0
                : "Native WindowScopedSidePanelRegistryBridge already exists";

        mNativeWindowScopedSidePanelRegistryBridge =
                WindowScopedSidePanelRegistryBridgeImplJni.get()
                        .create(this, nativeBrowserWindowPtr);
        return mNativeWindowScopedSidePanelRegistryBridge;
    }

    @VisibleForTesting
    void destroyNativePtr() {
        log(TAG, "destroyNativePtr");
        if (mNativeWindowScopedSidePanelRegistryBridge != 0) {
            WindowScopedSidePanelRegistryBridgeImplJni.get()
                    .destroy(mNativeWindowScopedSidePanelRegistryBridge);
        }
    }

    long getNativePtrForTesting() {
        return mNativeWindowScopedSidePanelRegistryBridge;
    }

    @CalledByNative
    private void clearNativePtr() {
        log(TAG, "clearNativePtr");
        mNativeWindowScopedSidePanelRegistryBridge = 0;
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code SidePanelRegistry} that's BrowserWindowInterface-scoped.
         *
         * @param caller The Java object calling this method.
         * @param nativeBrowserWindowPtr The pointer to the native {@code BrowserWindowInterface}.
         * @return The address of the native {@code SidePanelRegistry}.
         */
        long create(WindowScopedSidePanelRegistryBridgeImpl caller, long nativeBrowserWindowPtr);

        /**
         * Destroys the native {@code SidePanelRegistry}.
         *
         * @param nativeWindowScopedSidePanelRegistryBridge The address of the native {@code
         *     SidePanelRegistry}.
         */
        void destroy(long nativeWindowScopedSidePanelRegistryBridge);
    }
}
