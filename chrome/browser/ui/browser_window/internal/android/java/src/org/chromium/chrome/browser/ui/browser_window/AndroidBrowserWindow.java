// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.ActivityWindowAndroid;

/** Java class for communicating with the native {@code AndroidBrowserWindow}. */
@NullMarked
final class AndroidBrowserWindow {

    private final ChromeAndroidTask mChromeAndroidTask;
    private final Profile mProfile;
    private final @BrowserWindowType int mBrowserWindowType;
    private final AndroidBaseWindow mAndroidBaseWindow;
    private @Nullable ActivityWindowAndroid mActivityWindowAndroid;

    /** Address of the native {@code AndroidBrowserWindow}. */
    private long mNativeAndroidBrowserWindow;

    /**
     * Supports the native implementation of {@code BrowserWindowInterface::IsDeleteScheduled()}.
     * Please see the native code for documentation.
     */
    private boolean mIsDeleteScheduled;

    AndroidBrowserWindow(
            ChromeAndroidTask chromeAndroidTask,
            Profile profile,
            @BrowserWindowType int browserWindowType,
            @Nullable ActivityWindowAndroid activityWindowAndroid) {
        mChromeAndroidTask = chromeAndroidTask;
        mProfile = profile;
        mBrowserWindowType = browserWindowType;
        mAndroidBaseWindow = new AndroidBaseWindow(this);
        mActivityWindowAndroid = activityWindowAndroid;
    }

    /** Returns the {@link ChromeAndroidTask} that owns this window. */
    ChromeAndroidTask getTask() {
        return mChromeAndroidTask;
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
                    AndroidBrowserWindowJni.get().create(this, mBrowserWindowType, mProfile);
        }
        return mNativeAndroidBrowserWindow;
    }

    /**
     * Returns the address of the native {@code AndroidBrowserWindow}.
     *
     * <p>This method returns 0 if the native {@code AndroidBrowserWindow}
     * hasn't been created.
     */
    long getNativePtr() {
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
        mIsDeleteScheduled = true;
        mAndroidBaseWindow.destroy();

        if (mNativeAndroidBrowserWindow != 0) {
            AndroidBrowserWindowJni.get().destroy(mNativeAndroidBrowserWindow);
        }
    }

    @CalledByNative
    @VisibleForTesting
    boolean isDeleteScheduled() {
        return mIsDeleteScheduled;
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

    Profile getProfile() {
        return mProfile;
    }

    void setActivityWindowAndroid(ActivityWindowAndroid activityWindowAndroid) {
        assert mActivityWindowAndroid == null
                : "An Activity is already associated with this AndroidBrowserWindow";
        mActivityWindowAndroid = activityWindowAndroid;
    }

    @Nullable ActivityWindowAndroid getActivityWindowAndroid() {
        return mActivityWindowAndroid;
    }

    @CalledByNative
    @Nullable Activity getActivity() {
        return mActivityWindowAndroid == null ? null : mActivityWindowAndroid.getActivity().get();
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
         * @param browserWindowType The browser window type as defined in the native {@code
         *     BrowserWindowInterface::Type} enum.
         * @param profile The {@link Profile} associated with the {@code AndroidBrowserWindow}.
         * @return The address of the native {@code AndroidBrowserWindow}.
         */
        long create(
                AndroidBrowserWindow caller,
                @BrowserWindowType int browserWindowType,
                @JniType("Profile*") Profile profile);

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
