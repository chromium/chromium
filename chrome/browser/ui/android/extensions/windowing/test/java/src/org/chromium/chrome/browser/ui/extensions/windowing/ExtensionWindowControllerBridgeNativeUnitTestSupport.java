// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.windowing;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport;

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
    private static final int FAKE_CHROME_ANDROID_TASK_ID = 0;

    private final ChromeAndroidTask mChromeAndroidTask;
    private final ExtensionWindowControllerBridgeImpl mExtensionWindowControllerBridge;

    @CalledByNative
    private ExtensionWindowControllerBridgeNativeUnitTestSupport() {
        // Create a real ChromeAndroidTask with mock dependencies so that we can have real
        // ChromeAndroidTask internals, such as the native BrowserWindowInterface pointer that
        // ExtensionWindowControllerBridge depends on.
        mChromeAndroidTask =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        FAKE_CHROME_ANDROID_TASK_ID);

        mExtensionWindowControllerBridge =
                new ExtensionWindowControllerBridgeImpl(mChromeAndroidTask);
    }

    @CalledByNative
    private void tearDown() {
        mExtensionWindowControllerBridge.onTaskRemoved();
        mChromeAndroidTask.destroy();
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
