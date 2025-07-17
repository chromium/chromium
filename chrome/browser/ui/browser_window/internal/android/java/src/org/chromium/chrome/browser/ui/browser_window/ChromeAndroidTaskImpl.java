// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.List;

/** Implements {@link ChromeAndroidTask}. */
@NullMarked
final class ChromeAndroidTaskImpl implements ChromeAndroidTask {

    @SuppressWarnings("UnusedVariable")
    private final AndroidBrowserWindow mAndroidBrowserWindow;

    /**
     * Contains all {@link ChromeAndroidTaskFeature}s associated with this {@link
     * ChromeAndroidTask}.
     */
    @SuppressWarnings("UnusedVariable")
    private final List<ChromeAndroidTaskFeature> mFeatures = new ArrayList<>();

    ChromeAndroidTaskImpl() {
        mAndroidBrowserWindow = new AndroidBrowserWindow(/* chromeAndroidTask= */ this);
    }
}
