// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.jni_zero.CalledByNativeForTesting;

/**
 * Supports {@code window_scoped_side_panel_registry_bridge_unittest.cc}.
 *
 * <p>The native unit test will use this class to:
 *
 * <ul>
 *   <li>Instantiate a Java {@link WindowScopedSidePanelRegistryBridge} and its native {@code
 *       SidePanelRegistry}; and
 *   <li>Test the Java {@link WindowScopedSidePanelRegistryBridge} methods.
 * </ul>
 */
final class WindowScopedSidePanelRegistryBridgeNativeUnitTestSupport {
    private final WindowScopedSidePanelRegistryBridgeImpl mBridge;

    @CalledByNativeForTesting
    private WindowScopedSidePanelRegistryBridgeNativeUnitTestSupport() {
        mBridge = new WindowScopedSidePanelRegistryBridgeImpl();
    }

    @CalledByNativeForTesting
    public long invokeCreateNativePtr(long nativeBrowserWindowPtr) {
        return mBridge.createNativePtr(nativeBrowserWindowPtr);
    }

    @CalledByNativeForTesting
    private long invokeGetNativePtrForTesting() {
        return mBridge.getNativePtrForTesting();
    }

    @CalledByNativeForTesting
    private void invokeDestroyNativePtr() {
        mBridge.destroyNativePtr();
    }
}
