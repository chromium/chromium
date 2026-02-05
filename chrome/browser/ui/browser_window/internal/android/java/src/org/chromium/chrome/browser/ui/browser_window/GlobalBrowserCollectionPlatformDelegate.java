// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/**
 * This is the Java half of
 * chrome/browser/ui/browser_window/public/global_browser_collection_platform_delegate.h, which
 * listens for browser create, close, activate, and deactivate events in order to keep lists of
 * browsers sorted by creation and activation time.
 */
@NullMarked
final class GlobalBrowserCollectionPlatformDelegate
        implements AndroidBrowserWindowObserver, ChromeAndroidTaskTrackerObserver {
    /** Pointer to the native GlobalBrowserCollectionPlatformDelegate object. */
    private long mNativePointer;

    /**
     * Calls into the private constructor to create a new instance.
     *
     * @param nativePointer A pointer to the native GlobalBrowserCollectionPlatformDelegate object.
     */
    @CalledByNative
    private static GlobalBrowserCollectionPlatformDelegate create(long nativePointer) {
        return new GlobalBrowserCollectionPlatformDelegate(nativePointer);
    }

    /**
     * Create an instance of this object and add it as an observer of the
     * ChromeAndroidTaskTrackerImpl instance.
     *
     * @param nativePointer A pointer to the native GlobalBrowserCollectionPlatformDelegate object.
     */
    @VisibleForTesting
    GlobalBrowserCollectionPlatformDelegate(long nativePointer) {
        mNativePointer = nativePointer;
        var tracker = ChromeAndroidTaskTrackerImpl.getInstance();
        // If there are any existing tasks, add observers to them now as the implementation of
        // ChromeAndroidTaskTrackerObserver only receives signals on future events.
        for (ChromeAndroidTask task : tracker.getAllTasks()) {
            onTaskAdded(task);
        }
        tracker.addObserver(this);
    }

    /**
     * Cleans up this object and remove it from observing the ChromeAndroidTaskTrackerImpl instance.
     */
    @CalledByNative
    @VisibleForTesting
    void destroy() {
        ChromeAndroidTaskTrackerImpl.getInstance().removeObserver(this);
        mNativePointer = 0;
    }

    @Override
    public void onTaskAdded(ChromeAndroidTask task) {
        // If there are any existing windows in the task, count them now as the implementation of
        // AndroidBrowserWindowObserver only receives signals on future events.
        assert !task.hasAndroidBrowserWindowObserver(this);
        for (long androidBrowserWindowPtr : task.getAllNativeBrowserWindowPtrs()) {
            onBrowserWindowAdded(androidBrowserWindowPtr);
        }
        task.addAndroidBrowserWindowObserver(this);
    }

    @Override
    public void onTaskRemoved(ChromeAndroidTask task) {
        task.removeAndroidBrowserWindowObserver(this);
        // In the event there were still windows in the task, remove them as the observer has been
        // removed and will no longer receive signals.
        for (long androidBrowserWindowPtr : task.getAllNativeBrowserWindowPtrs()) {
            onBrowserWindowRemoved(androidBrowserWindowPtr);
        }
    }

    @Override
    public void onBrowserWindowAdded(long androidBrowserWindowPtr) {
        if (mNativePointer != 0) {
            GlobalBrowserCollectionPlatformDelegateJni.get()
                    .onBrowserCreated(mNativePointer, androidBrowserWindowPtr);
        }
    }

    @Override
    public void onBrowserWindowRemoved(long androidBrowserWindowPtr) {
        if (mNativePointer != 0) {
            GlobalBrowserCollectionPlatformDelegateJni.get()
                    .onBrowserClosed(mNativePointer, androidBrowserWindowPtr);
        }
    }

    @NativeMethods
    interface Natives {
        void onBrowserCreated(
                long nativeGlobalBrowserCollectionPlatformDelegate,
                long nativeAndroidBrowserWindow);

        void onBrowserClosed(
                long nativeGlobalBrowserCollectionPlatformDelegate,
                long nativeAndroidBrowserWindow);
    }
}
