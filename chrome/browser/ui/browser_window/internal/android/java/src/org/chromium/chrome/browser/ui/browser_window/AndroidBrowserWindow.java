// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.NativeBrowserWindowInterface;
import org.chromium.ui.base.ActivityWindowAndroid;

/** Java class for communicating with the native {@code AndroidBrowserWindow}. */
@NullMarked
final class AndroidBrowserWindow {

    private final ChromeAndroidTask mChromeAndroidTask;
    private final Profile mProfile;
    private final @BrowserWindowType int mBrowserWindowType;
    private final AndroidBaseWindow mAndroidBaseWindow;
    private @Nullable ActivityWindowAndroid mActivityWindowAndroid;

    /** The native {@code AndroidBrowserWindow}, owned by this class. */
    private @Nullable JniUniquePtr<NativeBrowserWindowInterface> mNativeAndroidBrowserWindow;

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
     * Returns the native {@code AndroidBrowserWindow}.
     *
     * <p>If the native {@code AndroidBrowserWindow} hasn't been created, this method will create it
     * before returning it.
     */
    JniPtr<NativeBrowserWindowInterface> getOrCreateNativePtr() {
        if (mNativeAndroidBrowserWindow == null) {
            mNativeAndroidBrowserWindow =
                    AndroidBrowserWindowJni.get().create(this, mBrowserWindowType, mProfile);
        }
        return mNativeAndroidBrowserWindow;
    }

    /**
     * Returns the native {@code AndroidBrowserWindow}.
     *
     * <p>This method returns null if the native {@code AndroidBrowserWindow} hasn't been created.
     */
    @Nullable JniPtr<NativeBrowserWindowInterface> getNativePtr() {
        return mNativeAndroidBrowserWindow;
    }

    /**
     * Same as {@link #getOrCreateNativePtr()}, but returns the address of the native {@code
     * BrowserWindowInterface}.
     */
    // TODO(crbug.com/40794873): Remove once all callers take JniPtr<NativeBrowserWindowInterface>.
    long getOrCreateLegacyNativePtr() {
        return AndroidBrowserWindowJni.get().getLegacyNativePtr(getOrCreateNativePtr());
    }

    /**
     * Same as {@link #getNativePtr()}, but returns the address of the native {@code
     * BrowserWindowInterface}, or 0 if it hasn't been created.
     */
    // TODO(crbug.com/40794873): Remove once all callers take JniPtr<NativeBrowserWindowInterface>.
    long getLegacyNativePtr() {
        if (mNativeAndroidBrowserWindow == null) {
            return 0;
        }
        return AndroidBrowserWindowJni.get().getLegacyNativePtr(mNativeAndroidBrowserWindow);
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

        if (mNativeAndroidBrowserWindow != null) {
            mNativeAndroidBrowserWindow.destroy();
            mNativeAndroidBrowserWindow = null;
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
        if (mNativeAndroidBrowserWindow == null) {
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

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code AndroidBrowserWindow}.
         *
         * @param caller The Java object calling this method.
         * @param browserWindowType The browser window type as defined in the native {@code
         *     BrowserWindowInterface::Type} enum.
         * @param profile The {@link Profile} associated with the {@code AndroidBrowserWindow}.
         * @return The native {@code AndroidBrowserWindow}, owned by the caller.
         */
        JniUniquePtr<NativeBrowserWindowInterface> create(
                AndroidBrowserWindow caller,
                @BrowserWindowType int browserWindowType,
                @JniType("Profile*") Profile profile);

        /** Returns the address of the given native {@code BrowserWindowInterface}. */
        long getLegacyNativePtr(JniPtr<NativeBrowserWindowInterface> browserWindow);

        /**
         * Returns the {@code SessionID} as returned by the native function {@code
         * AndroidBrowserWindow::GetSessionID()}.
         */
        int getSessionIdForTesting(JniPtr<NativeBrowserWindowInterface> browserWindow);
    }
}
