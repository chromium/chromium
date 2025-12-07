// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/** Supports {@code browser_window_interface_iterator_android_unittest.cc}. */
@NullMarked
final class BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport {

    private BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport() {}

    @CalledByNative
    private static long createBrowserWindow(int taskId, Profile profile) {
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId, profile);
        var chromeAndroidTask =
                ChromeAndroidTaskTrackerImpl.getInstance()
                        .obtainTask(
                                BrowserWindowType.NORMAL,
                                activityScopedObjects,
                                /* pendingId= */ null);
        return chromeAndroidTask.getOrCreateNativeBrowserWindowPtr();
    }

    /**
     * This function simulates Android OS behavior to activate a browser window. It calls into a
     * task's onTopResumedActivityChangedWithNative() function to make sure its
     * mLastActivatedTimeMillis is positive, which is a requirement for being able to call into
     * ChromeAndroidTaskTrackerImpl.getNativeBrowserWindowPtrsOrderedByActivation(). Without this,
     * |mLastActivatedTimeMillis| will always be -1 (invalid value) as the window isn't activated.
     */
    @CalledByNative
    private static void activateBrowserWindow(int taskId) {
        ChromeAndroidTaskImpl task =
                (ChromeAndroidTaskImpl) ChromeAndroidTaskTrackerImpl.getInstance().get(taskId);
        assert task != null;
        task.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
    }

    @CalledByNative
    private static void invokeOnTopResumedActivityChanged(
            int taskId, boolean isTopResumedActivity) {
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) ChromeAndroidTaskTrackerImpl.getInstance().get(taskId);
        assert chromeAndroidTask != null : "Task doesn't exist";

        chromeAndroidTask.onTopResumedActivityChangedWithNative(isTopResumedActivity);
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
