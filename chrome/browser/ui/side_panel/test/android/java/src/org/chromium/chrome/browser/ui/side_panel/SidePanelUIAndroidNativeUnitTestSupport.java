// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.jni_zero.CalledByNativeForTesting;

import org.chromium.build.annotations.NullMarked;

/**
 * Supports {@code side_panel_ui_android_unittest.cc}.
 *
 * <p>The native unit test will use this class to:
 *
 * <ul>
 *   <li>Instantiate a Java {@link SidePanelUIAndroidBridge} and its native {@code
 *       SidePanelUIAndroid}; and
 *   <li>Test the Java {@link SidePanelUIAndroidBridge} methods.
 * </ul>
 */
@NullMarked
final class SidePanelUIAndroidNativeUnitTestSupport {
    private final SidePanelUIAndroidBridge mBridge;

    @CalledByNativeForTesting
    private SidePanelUIAndroidNativeUnitTestSupport() {
        mBridge = new SidePanelUIAndroidBridge();
    }

    @CalledByNativeForTesting
    private long invokeCreateNativePtr() {
        mBridge.createNativePtr();
        return mBridge.getNativePtrForTesting();
    }

    @CalledByNativeForTesting
    private long invokeGetNativePtrForTesting() {
        return mBridge.getNativePtrForTesting();
    }

    @CalledByNativeForTesting
    private void invokeDestroy() {
        mBridge.destroy();
    }
}
