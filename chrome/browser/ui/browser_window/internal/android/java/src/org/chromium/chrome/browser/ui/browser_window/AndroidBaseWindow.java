// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;

/** Java class for communicating with the native {@code AndroidBaseWindow}. */
@NullMarked
final class AndroidBaseWindow {

    /** Supports windowing functionalities of the native {@code AndroidBaseWindow}. */
    @SuppressWarnings("UnusedVariable")
    private final ChromeAndroidTask mChromeAndroidTask;

    AndroidBaseWindow(ChromeAndroidTask chromeAndroidTask) {
        mChromeAndroidTask = chromeAndroidTask;
    }
}
