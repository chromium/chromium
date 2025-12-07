// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/**
 * Java class implementing the native functions in {@code
 * browser_window_interface_iterator_android.cc}.
 */
@NullMarked
final class BrowserWindowInterfaceIteratorAndroid {
    private BrowserWindowInterfaceIteratorAndroid() {}

    @CalledByNative
    @JniType("std::vector<int64_t>")
    private static long[] getAllBrowserWindowInterfaces() {
        return ChromeAndroidTaskTrackerImpl.getInstance().getAllNativeBrowserWindowPtrs();
    }

    @CalledByNative
    @JniType("std::vector<int64_t>")
    private static long[] getBrowserWindowInterfacesOrderedByActivation() {
        return ChromeAndroidTaskTrackerImpl.getInstance()
                .getNativeBrowserWindowPtrsOrderedByActivation();
    }
}
