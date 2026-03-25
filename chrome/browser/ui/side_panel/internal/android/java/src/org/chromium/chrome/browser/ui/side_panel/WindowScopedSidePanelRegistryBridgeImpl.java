// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Implements {@link WindowScopedSidePanelRegistryBridge}. */
@NullMarked
final class WindowScopedSidePanelRegistryBridgeImpl implements WindowScopedSidePanelRegistryBridge {

    private long mNativeWindowScopedSidePanelRegistryBridge;

    WindowScopedSidePanelRegistryBridgeImpl() {}

    @Override
    public void onAddedToTask(long nativeBrowserWindowPtr) {
        createNativePtr(nativeBrowserWindowPtr);
    }

    @Override
    public void onFeatureRemoved() {
        destroyNativePtr();
    }

    @VisibleForTesting
    long createNativePtr(long nativeBrowserWindowPtr) {
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
