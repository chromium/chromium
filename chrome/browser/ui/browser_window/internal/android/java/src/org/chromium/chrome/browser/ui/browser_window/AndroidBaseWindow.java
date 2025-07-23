// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Java class for communicating with the native {@code AndroidBaseWindow}. */
@NullMarked
final class AndroidBaseWindow {

    /** Supports windowing functionalities of the native {@code AndroidBaseWindow}. */
    @SuppressWarnings("UnusedVariable")
    private final ChromeAndroidTask mChromeAndroidTask;

    /** Address of the native {@code AndroidBaseWindow}. */
    private long mNativeAndroidBaseWindow;

    AndroidBaseWindow(ChromeAndroidTask chromeAndroidTask) {
        mChromeAndroidTask = chromeAndroidTask;
    }

    /**
     * Returns the address of the native {@code AndroidBaseWindow}.
     *
     * <p>If the native {@code AndroidBaseWindow} hasn't been created, this method will create it
     * before returning its address.
     */
    long getOrCreateNativePtr() {
        if (mNativeAndroidBaseWindow == 0) {
            mNativeAndroidBaseWindow = AndroidBaseWindowJni.get().create(this);
        }
        return mNativeAndroidBaseWindow;
    }

    /** Destroys all objects owned by this class. */
    void destroy() {
        if (mNativeAndroidBaseWindow != 0) {
            AndroidBaseWindowJni.get().destroy(mNativeAndroidBaseWindow);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAndroidBaseWindow = 0;
    }

    long getNativePtrForTesting() {
        return mNativeAndroidBaseWindow;
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code AndroidBaseWindow}.
         *
         * @param caller The Java object calling this method.
         * @return The address of the native {@code AndroidBaseWindow}.
         */
        long create(AndroidBaseWindow caller);

        /**
         * Destroys the native {@code AndroidBaseWindow}.
         *
         * @param nativeAndroidBaseWindow The address of the native {@code AndroidBaseWindow}.
         */
        void destroy(long nativeAndroidBaseWindow);
    }
}
