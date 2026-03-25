// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.jni_zero.CalledByNativeForTesting;

/**
 * Supports {@code tab_scoped_side_panel_registry_bridge_unittest.cc}.
 *
 * <p>The native unit test will use this class to:
 *
 * <ul>
 *   <li>Instantiate a Java {@link TabScopedSidePanelRegistryBridge} and its native {@code
 *       SidePanelRegistry}; and
 *   <li>Test the Java {@link TabScopedSidePanelRegistryBridge} methods.
 * </ul>
 */
final class TabScopedSidePanelRegistryBridgeNativeUnitTestSupport {
    private final TabScopedSidePanelRegistryBridgeImpl mBridge;

    @CalledByNativeForTesting
    private TabScopedSidePanelRegistryBridgeNativeUnitTestSupport() {
        mBridge = new TabScopedSidePanelRegistryBridgeImpl();
    }

    @CalledByNativeForTesting
    public long invokeCreateNativePtrForTesting(long nativeTabInterfacePtr) {
        return mBridge.createNativePtrForTesting(nativeTabInterfacePtr);
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
