// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/**
 * This is the Java half of
 * chrome/browser/ui/browser_window/internal/android/android_browser_window_enumerator.h, which
 * enumerates each Android browser window in the system in a way that is resilient to additions or
 * removals during iteration.
 */
@NullMarked
final class AndroidBrowserWindowEnumerator implements ChromeAndroidTaskTrackerObserver {
    /** Pointer to the native AndroidBrowserWindowEnumerator object. */
    private long mNativePointer;

    /** Whether or not to track newly-created browser windows while iterating. */
    private final boolean mEnumerateNewBrowserWindows;

    /**
     * Calls into the private constructor to create a new instance.
     *
     * @param nativePointer A pointer to the native AndroidBrowserWindowEnumerator object.
     * @param enumerateNewBrowserWindows Whether or not to track newly created browser windows.
     */
    @CalledByNative
    private static AndroidBrowserWindowEnumerator create(
            long nativePointer, boolean enumerateNewBrowserWindows) {
        return new AndroidBrowserWindowEnumerator(nativePointer, enumerateNewBrowserWindows);
    }

    /**
     * Create an instance of this object and add it as an observer of the
     * ChromeAndroidTaskTrackerImpl instance.
     *
     * @param nativePointer A pointer to the native AndroidBrowserWindowEnumerator object.
     * @param enumerateNewBrowserWindows Whether or not to track newly created browser windows.
     */
    private AndroidBrowserWindowEnumerator(long nativePointer, boolean enumerateNewBrowserWindows) {
        mNativePointer = nativePointer;
        mEnumerateNewBrowserWindows = enumerateNewBrowserWindows;
        ChromeAndroidTaskTrackerImpl.getInstance().addObserver(this);
    }

    /**
     * Cleans up this object and remove it from observing the ChromeAndroidTaskTrackerImpl instance.
     */
    @CalledByNative
    private void destroy() {
        ChromeAndroidTaskTrackerImpl.getInstance().removeObserver(this);
        mNativePointer = 0;
    }

    @Override
    public void onTaskAdded(ChromeAndroidTask task) {
        if (mEnumerateNewBrowserWindows && mNativePointer != 0) {
            AndroidBrowserWindowEnumeratorJni.get()
                    .onBrowserWindowAdded(mNativePointer, task.getOrCreateNativeBrowserWindowPtr());
        }
    }

    @Override
    public void onTaskRemoved(ChromeAndroidTask task) {
        if (mNativePointer != 0) {
            AndroidBrowserWindowEnumeratorJni.get()
                    .onBrowserWindowRemoved(
                            mNativePointer, task.getOrCreateNativeBrowserWindowPtr());
        }
    }

    @NativeMethods
    interface Natives {
        void onBrowserWindowAdded(
                long nativeAndroidBrowserWindowEnumerator, long nativeAndroidBrowserWindow);

        void onBrowserWindowRemoved(
                long nativeAndroidBrowserWindowEnumerator, long nativeAndroidBrowserWindow);
    }
}
