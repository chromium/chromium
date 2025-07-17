// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;

/** Java class for communicating with the native {@code AndroidBrowserWindow}. */
@NullMarked
final class AndroidBrowserWindow {

    @SuppressWarnings("UnusedVariable")
    private final AndroidBaseWindow mAndroidBaseWindow;

    AndroidBrowserWindow(ChromeAndroidTask chromeAndroidTask) {
        mAndroidBaseWindow = new AndroidBaseWindow(chromeAndroidTask);
    }
}
