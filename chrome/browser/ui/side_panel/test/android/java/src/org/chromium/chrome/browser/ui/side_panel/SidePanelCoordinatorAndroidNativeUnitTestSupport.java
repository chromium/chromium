// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.jni_zero.CalledByNativeForTesting;

import org.chromium.build.annotations.NullMarked;

/**
 * Supports {@code side_panel_coordinator_android_unittest.cc}.
 *
 * <p>The native unit test will use this class to:
 *
 * <ul>
 *   <li>Instantiate a Java {@link SidePanelCoordinatorAndroid} and its native {@code
 *       SidePanelCoordinatorAndroid}; and
 *   <li>Test the Java {@link SidePanelCoordinatorAndroid} methods.
 * </ul>
 */
@NullMarked
final class SidePanelCoordinatorAndroidNativeUnitTestSupport {
    private final SidePanelCoordinatorAndroidImpl mCoordinator;

    @CalledByNativeForTesting
    private SidePanelCoordinatorAndroidNativeUnitTestSupport() {
        mCoordinator = new SidePanelCoordinatorAndroidImpl();
    }

    @CalledByNativeForTesting
    private long invokeCreateNativePtr(long nativeBrowserWindowPtr) {
        mCoordinator.createNativePtr(nativeBrowserWindowPtr);
        return mCoordinator.getNativePtrForTesting();
    }

    @CalledByNativeForTesting
    private long invokeGetNativePtrForTesting() {
        return mCoordinator.getNativePtrForTesting();
    }

    @CalledByNativeForTesting
    private void invokeDestroyNativePtr() {
        mCoordinator.destroyNativePtr();
    }
}
