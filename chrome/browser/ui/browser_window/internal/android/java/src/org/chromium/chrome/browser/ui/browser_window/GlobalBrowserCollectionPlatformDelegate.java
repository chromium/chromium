// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

import java.util.Comparator;
import java.util.List;

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
        assert nativePointer != 0;
        mNativePointer = nativePointer;
        var tracker = ChromeAndroidTaskTrackerImpl.getInstance();
        List<ChromeAndroidTask> existingTasks = tracker.getAllTasks();

        // Invoke onBrowserCreated() for existing windows and subscribe to each task:
        // ChromeAndroidTaskTrackerObserver and AndroidBrowserWindowObserver only notify on future
        // events, so pre-existing tasks and windows must be registered now in creation order.
        for (ChromeAndroidTask task : existingTasks) {
            assert !task.hasAndroidBrowserWindowObserver(this);
            task.addAndroidBrowserWindowObserver(this);
            for (long androidBrowserWindowPtr : task.getAllNativeBrowserWindowPtrs()) {
                GlobalBrowserCollectionPlatformDelegateJni.get()
                        .onBrowserCreated(mNativePointer, androidBrowserWindowPtr);
            }
        }

        // Invoke onBrowserActivated() to seed initial MRU activation order:
        // AndroidBrowserWindowObserver only receives future activation events, so historical and
        // current activation state must be replayed explicitly. Because native
        // GlobalBrowserCollection::OnBrowserActivated() rotates each activated browser to the
        // front of `browsers_activation_order_`, replaying from oldest-activated to
        // currently-active (and secondary windows in reverse before `activePtr`) leaves the active
        // window at the front and never-activated windows untouched at the back in creation order:
        //
        //   Initial (after onBrowserCreated for T1=never, T2=t200, T3=t100, T4=active):
        //     [T1, T2, T3, T4]
        //   1. Replay oldest inactive T3 (t100):  [T3, T1, T2, T4]
        //   2. Replay newer inactive  T2 (t200):  [T2, T3, T1, T4]
        //   3. Replay currently active T4:        [T4, T2, T3, T1]  (MRU -> never-activated)
        existingTasks.sort(
                Comparator.comparingInt(
                                (ChromeAndroidTask task) ->
                                        task.getActiveNativeBrowserWindowPtr() != 0 ? 1 : 0)
                        .thenComparingLong(ChromeAndroidTask::getLastActivatedTimeMillis));
        for (ChromeAndroidTask task : existingTasks) {
            long activePtr = task.getActiveNativeBrowserWindowPtr();
            if (activePtr == 0 && task.getLastActivatedTimeMillis() <= 0) {
                continue;
            }
            List<Long> ptrs = task.getAllNativeBrowserWindowPtrs();
            for (int i = ptrs.size() - 1; i >= 0; i--) {
                long ptr = ptrs.get(i);
                if (ptr != activePtr) {
                    GlobalBrowserCollectionPlatformDelegateJni.get()
                            .onBrowserActivated(mNativePointer, ptr);
                }
            }
            if (activePtr != 0) {
                GlobalBrowserCollectionPlatformDelegateJni.get()
                        .onBrowserActivated(mNativePointer, activePtr);
            }
        }
        tracker.addObserver(this);
    }

    /**
     * Cleans up this object and remove it from observing the ChromeAndroidTaskTrackerImpl instance.
     */
    @CalledByNative
    @VisibleForTesting
    void destroy() {
        var tracker = ChromeAndroidTaskTrackerImpl.getInstance();
        tracker.removeObserver(this);
        for (ChromeAndroidTask task : tracker.getAllTasks()) {
            task.removeAndroidBrowserWindowObserver(this);
        }
        mNativePointer = 0;
    }

    @Override
    public void onTaskAdded(ChromeAndroidTask task) {
        // If there are any existing windows in the task, count them now as the implementation of
        // AndroidBrowserWindowObserver only receives signals on future events.
        assert !task.hasAndroidBrowserWindowObserver(this);
        task.addAndroidBrowserWindowObserver(this);
        if (mNativePointer != 0) {
            for (long androidBrowserWindowPtr : task.getAllNativeBrowserWindowPtrs()) {
                GlobalBrowserCollectionPlatformDelegateJni.get()
                        .onBrowserCreated(mNativePointer, androidBrowserWindowPtr);
            }
            long activePtr = task.getActiveNativeBrowserWindowPtr();
            if (activePtr != 0) {
                GlobalBrowserCollectionPlatformDelegateJni.get()
                        .onBrowserActivated(mNativePointer, activePtr);
            }
        }
    }

    @Override
    public void onTaskRemoved(ChromeAndroidTask task) {
        task.removeAndroidBrowserWindowObserver(this);
        // In the event there were still windows in the task, remove them as the observer has been
        // removed and will no longer receive signals.
        if (mNativePointer != 0) {
            for (long androidBrowserWindowPtr : task.getAllNativeBrowserWindowPtrs()) {
                GlobalBrowserCollectionPlatformDelegateJni.get()
                        .onBrowserClosed(mNativePointer, androidBrowserWindowPtr);
            }
        }
    }

    @Override
    public void onBrowserWindowAdded(AndroidBrowserWindowInfo windowInfo) {
        if (mNativePointer != 0) {
            GlobalBrowserCollectionPlatformDelegateJni.get()
                    .onBrowserCreated(mNativePointer, windowInfo.mBrowserWindowPtr);
        }
    }

    @Override
    public void onBrowserWindowRemoved(AndroidBrowserWindowInfo windowInfo) {
        if (mNativePointer != 0) {
            GlobalBrowserCollectionPlatformDelegateJni.get()
                    .onBrowserClosed(mNativePointer, windowInfo.mBrowserWindowPtr);
        }
    }

    @Override
    public void onBrowserWindowActivated(AndroidBrowserWindowInfo windowInfo) {
        if (mNativePointer != 0) {
            GlobalBrowserCollectionPlatformDelegateJni.get()
                    .onBrowserActivated(mNativePointer, windowInfo.mBrowserWindowPtr);
        }
    }

    @Override
    public void onBrowserWindowDeactivated(AndroidBrowserWindowInfo windowInfo) {
        if (mNativePointer != 0) {
            GlobalBrowserCollectionPlatformDelegateJni.get()
                    .onBrowserDeactivated(mNativePointer, windowInfo.mBrowserWindowPtr);
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

        void onBrowserActivated(
                long nativeGlobalBrowserCollectionPlatformDelegate,
                long nativeAndroidBrowserWindow);

        void onBrowserDeactivated(
                long nativeGlobalBrowserCollectionPlatformDelegate,
                long nativeAndroidBrowserWindow);
    }
}
