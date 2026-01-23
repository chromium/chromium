// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

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
final class GlobalBrowserCollectionPlatformDelegate implements ChromeAndroidTaskTrackerObserver {
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
    private GlobalBrowserCollectionPlatformDelegate(long nativePointer) {
        mNativePointer = nativePointer;
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
        if (mNativePointer != 0) {
            GlobalBrowserCollectionPlatformDelegateJni.get()
                    .onBrowserCreated(mNativePointer, task.getOrCreateNativeBrowserWindowPtr());
        }
    }

    @Override
    public void onTaskRemoved(ChromeAndroidTask task) {
        if (mNativePointer != 0) {
            GlobalBrowserCollectionPlatformDelegateJni.get()
                    .onBrowserClosed(mNativePointer, task.getOrCreateNativeBrowserWindowPtr());
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
