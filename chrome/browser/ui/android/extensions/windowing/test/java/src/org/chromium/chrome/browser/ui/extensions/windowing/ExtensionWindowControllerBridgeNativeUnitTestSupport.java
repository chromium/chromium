// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.windowing;

import android.graphics.Rect;

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
        // Create a real ChromeAndroidTask with mock dependencies, but don't mock @NativeMethods.
        // This way we can have real ChromeAndroidTask internals, such as the native
        // BrowserWindowInterface pointer that ExtensionWindowControllerBridge depends on.
        mChromeAndroidTask =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                                FAKE_CHROME_ANDROID_TASK_ID,
                                /* mockNatives= */ false,
                                /* isPendingTask= */ false)
                        .mChromeAndroidTask;

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

    @CalledByNative
    private long getNativeBrowserWindowPtr() {
        return mChromeAndroidTask.getOrCreateNativeBrowserWindowPtr();
    }
}
