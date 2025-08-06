// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/** Supports {@code browser_window_interface_iterator_android_unittest.cc}. */
@NullMarked
final class BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport {

    private BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport() {}

    @CalledByNative
    private static long createBrowserWindow(int taskId) {
        var mockActivityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var chromeAndroidTask =
                ChromeAndroidTaskTrackerImpl.getInstance().obtainTask(mockActivityWindowAndroid);
        return chromeAndroidTask.getOrCreateNativeBrowserWindowPtr();
    }

    @CalledByNative
    private static void destroyBrowserWindow(int taskId) {
        ChromeAndroidTaskTrackerImpl.getInstance().remove(taskId);
    }

    @CalledByNative
    private static void destroyAllBrowserWindows() {
        ChromeAndroidTaskTrackerImpl.getInstance().removeAllForTesting();
    }
}
