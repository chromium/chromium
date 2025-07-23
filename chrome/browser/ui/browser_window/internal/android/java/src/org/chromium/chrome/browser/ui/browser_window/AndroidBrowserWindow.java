// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Java class for communicating with the native {@code AndroidBrowserWindow}. */
@NullMarked
final class AndroidBrowserWindow {

    private final AndroidBaseWindow mAndroidBaseWindow;

    /** Address of the native {@code AndroidBrowserWindow}. */
    private long mNativeAndroidBrowserWindow;

    AndroidBrowserWindow(ChromeAndroidTask chromeAndroidTask) {
        mAndroidBaseWindow = new AndroidBaseWindow(chromeAndroidTask);
    }

    /**
     * Returns the address of the native {@code AndroidBrowserWindow}.
     *
     * <p>If the native {@code AndroidBrowserWindow} hasn't been created, this method will create it
     * before returning its address.
     */
    long getOrCreateNativePtr() {
        if (mNativeAndroidBrowserWindow == 0) {
            mNativeAndroidBrowserWindow = AndroidBrowserWindowJni.get().create(this);
        }
        return mNativeAndroidBrowserWindow;
    }

    /**
     * Returns the address of the native {@code AndroidBaseWindow}.
     *
     * @see AndroidBaseWindow#getOrCreateNativePtr()
     */
    @CalledByNative
    long getOrCreateNativeBaseWindowPtr() {
        return mAndroidBaseWindow.getOrCreateNativePtr();
    }

    /** Destroys all objects owned by this class. */
    void destroy() {
        mAndroidBaseWindow.destroy();

        if (mNativeAndroidBrowserWindow != 0) {
            AndroidBrowserWindowJni.get().destroy(mNativeAndroidBrowserWindow);
        }
    }

    long getNativePtrForTesting() {
        return mNativeAndroidBrowserWindow;
    }

    long getNativeBaseWindowPtrForTesting() {
        return mAndroidBaseWindow.getNativePtrForTesting();
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAndroidBrowserWindow = 0;
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code AndroidBrowserWindow}.
         *
         * @param caller The Java object calling this method.
         * @return The address of the native {@code AndroidBrowserWindow}.
         */
        long create(AndroidBrowserWindow caller);

        /**
         * Destroys the native {@code AndroidBrowserWindow}.
         *
         * @param nativeAndroidBrowserWindow The address of the native {@code AndroidBrowserWindow}.
         */
        void destroy(long nativeAndroidBrowserWindow);
    }
}
