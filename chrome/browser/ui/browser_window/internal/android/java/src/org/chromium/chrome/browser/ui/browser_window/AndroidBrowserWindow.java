// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.ActivityWindowAndroid;

/** Java class for communicating with the native {@code AndroidBrowserWindow}. */
@NullMarked
final class AndroidBrowserWindow {

    private final ChromeAndroidTask mChromeAndroidTask;
    private final AndroidBaseWindow mAndroidBaseWindow;

    /** Address of the native {@code AndroidBrowserWindow}. */
    private long mNativeAndroidBrowserWindow;

    AndroidBrowserWindow(ChromeAndroidTask chromeAndroidTask) {
        mChromeAndroidTask = chromeAndroidTask;
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
            mNativeAndroidBrowserWindow =
                    AndroidBrowserWindowJni.get()
                            .create(
                                    this,
                                    mChromeAndroidTask.getBrowserWindowType(),
                                    mChromeAndroidTask.getProfile());
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

    @Nullable Integer getNativeSessionIdForTesting() {
        if (mNativeAndroidBrowserWindow == 0) {
            return null;
        }

        return AndroidBrowserWindowJni.get().getSessionIdForTesting(mNativeAndroidBrowserWindow);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAndroidBrowserWindow = 0;
    }

    @CalledByNative
    @Nullable Activity getActivity() {
        ActivityWindowAndroid activityWindowAndroid = mChromeAndroidTask.getActivityWindowAndroid();
        if (activityWindowAndroid == null) return null;
        return activityWindowAndroid.getActivity().get();
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code AndroidBrowserWindow}.
         *
         * @param caller The Java object calling this method.
         * @param browserWindowType The browser window type as defined in the native {@code
         *     BrowserWindowInterface::Type} enum.
         * @param profile The {@link Profile} associated with the {@code AndroidBrowserWindow}.
         * @return The address of the native {@code AndroidBrowserWindow}.
         */
        long create(
                AndroidBrowserWindow caller,
                @BrowserWindowType int browserWindowType,
                Profile profile);

        /**
         * Destroys the native {@code AndroidBrowserWindow}.
         *
         * @param nativeAndroidBrowserWindow The address of the native {@code AndroidBrowserWindow}.
         */
        void destroy(long nativeAndroidBrowserWindow);

        /**
         * Returns the {@code SessionID} as returned by the native function {@code
         * AndroidBrowserWindow::GetSessionID()}.
         */
        int getSessionIdForTesting(long nativeAndroidBrowserWindow);
    }
}
