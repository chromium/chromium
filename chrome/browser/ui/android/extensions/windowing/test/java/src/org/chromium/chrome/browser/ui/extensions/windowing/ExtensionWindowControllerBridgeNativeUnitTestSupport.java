// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.windowing;

import android.graphics.Rect;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/**
 * Supports {@code extension_window_controller_bridge_unittest.cc}.
 *
 * <p>The native unit test will use this class to:
 *
 * <ul>
 *   <li>Instantiate a Java {@link ExtensionWindowControllerBridgeImpl} and its native counterpart;
 *       and
 *   <li>Test the Java methods in {@link ExtensionWindowControllerBridgeImpl}.
 * </ul>
 */
@NullMarked
final class ExtensionWindowControllerBridgeNativeUnitTestSupport {
    private final ExtensionWindowControllerBridgeImpl mExtensionWindowControllerBridge;

    @CalledByNative
    private ExtensionWindowControllerBridgeNativeUnitTestSupport() {
        mExtensionWindowControllerBridge = new ExtensionWindowControllerBridgeImpl();
    }

    @CalledByNative
    private void tearDown() {
        mExtensionWindowControllerBridge.onFeatureRemoved();
    }

    @CalledByNative
    private void invokeOnAddedToTask(long nativeBrowserWindowPtr) {
        mExtensionWindowControllerBridge.onAddedToTask(nativeBrowserWindowPtr);
    }

    @CalledByNative
    private void invokeOnFeatureRemoved() {
        mExtensionWindowControllerBridge.onFeatureRemoved();
    }

    @CalledByNative
    private void invokeOnTaskBoundsChanged() {
        mExtensionWindowControllerBridge.onTaskBoundsChanged(
                // Native code doesn't need the new bounds, so what we pass here doesn't matter.
                new Rect());
    }

    @CalledByNative
    private void invokeOnTaskFocusChanged(boolean hasFocus) {
        mExtensionWindowControllerBridge.onTaskFocusChanged(hasFocus);
    }

    @CalledByNative
    private long invokeGetNativePtrForTesting() {
        return mExtensionWindowControllerBridge.getNativePtrForTesting();
    }
}
