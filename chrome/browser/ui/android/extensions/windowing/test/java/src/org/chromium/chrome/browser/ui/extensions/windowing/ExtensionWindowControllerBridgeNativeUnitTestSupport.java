// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.windowing;

import static org.mockito.Mockito.mock;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

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
    private final ChromeAndroidTask mChromeAndroidTask;
    private final ExtensionWindowControllerBridgeImpl mExtensionWindowControllerBridge;

    @CalledByNative
    private ExtensionWindowControllerBridgeNativeUnitTestSupport() {
        mChromeAndroidTask = mock(ChromeAndroidTask.class);
        mExtensionWindowControllerBridge =
                new ExtensionWindowControllerBridgeImpl(mChromeAndroidTask);
    }

    @CalledByNative
    private void invokeOnAddedToTask() {
        mExtensionWindowControllerBridge.onAddedToTask();
    }

    @CalledByNative
    private void invokeOnTaskRemoved() {
        mExtensionWindowControllerBridge.onTaskRemoved();
    }

    @CalledByNative
    private long invokeGetNativePtrForTesting() {
        return mExtensionWindowControllerBridge.getNativePtrForTesting();
    }
}
