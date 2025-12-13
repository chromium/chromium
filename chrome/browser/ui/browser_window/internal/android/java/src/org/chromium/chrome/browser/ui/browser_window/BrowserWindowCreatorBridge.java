// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.JniOnceCallback;
import org.chromium.build.annotations.NullMarked;

/** Java class that will be used by the native {@code #CreateBrowserWindow()} function. */
@NullMarked
final class BrowserWindowCreatorBridge {
    private BrowserWindowCreatorBridge() {}

    @CalledByNative
    @VisibleForTesting
    static long createBrowserWindow(AndroidBrowserWindowCreateParams createParams) {
        ChromeAndroidTaskTracker tracker = ChromeAndroidTaskTrackerFactory.getInstance();
        assert tracker != null;
        ChromeAndroidTask task = tracker.createPendingTask(createParams, null);
        return task == null ? 0L : task.getOrCreateNativeBrowserWindowPtr();
    }

    @CalledByNative
    @VisibleForTesting
    static void createBrowserWindowAsync(
            AndroidBrowserWindowCreateParams createParams, JniOnceCallback<Long> callback) {
        ChromeAndroidTaskTracker tracker = ChromeAndroidTaskTrackerFactory.getInstance();
        assert tracker != null;
        tracker.createPendingTask(createParams, callback);
    }
}
