// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.graphics.Rect;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** Java class for communicating with the native {@code AndroidBaseWindow}. */
@NullMarked
final class AndroidBaseWindow {
    private final AndroidBrowserWindow mAndroidBrowserWindow;

    /** Address of the native {@code AndroidBaseWindow}. */
    private long mNativeAndroidBaseWindow;

    AndroidBaseWindow(AndroidBrowserWindow androidBrowserWindow) {
        mAndroidBrowserWindow = androidBrowserWindow;
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
    private boolean isActive() {
        return mAndroidBrowserWindow.getTask().isActive();
    }

    @CalledByNative
    private boolean isMaximized() {
        return mAndroidBrowserWindow.getTask().isMaximized();
    }

    @CalledByNative
    private boolean isMinimized() {
        return mAndroidBrowserWindow.getTask().isMinimized();
    }

    @CalledByNative
    private boolean isFullscreen() {
        return mAndroidBrowserWindow.getTask().isFullscreen();
    }

    @CalledByNative
    @JniType("std::vector<int>")
    private int[] getRestoredBounds() {
        Rect bounds = mAndroidBrowserWindow.getTask().getRestoredBoundsInDp();
        return new int[] {bounds.left, bounds.top, bounds.width(), bounds.height()};
    }

    @CalledByNative
    @JniType("std::vector<int>")
    private int[] getBounds() {
        Rect bounds = mAndroidBrowserWindow.getTask().getBoundsInDp();
        return new int[] {bounds.left, bounds.top, bounds.width(), bounds.height()};
    }

    @CalledByNative
    private void show() {
        mAndroidBrowserWindow.getTask().show();
    }

    @CalledByNative
    private boolean isVisible() {
        return mAndroidBrowserWindow.getTask().isVisible();
    }

    @CalledByNative
    private void showInactive() {
        mAndroidBrowserWindow.getTask().showInactive();
    }

    @CalledByNative
    private void close() {
        mAndroidBrowserWindow.getTask().close();
    }

    @CalledByNative
    private void activate() {
        mAndroidBrowserWindow.getTask().activate();
    }

    @CalledByNative
    private void deactivate() {
        mAndroidBrowserWindow.getTask().deactivate();
    }

    @CalledByNative
    private void maximize() {
        mAndroidBrowserWindow.getTask().maximize();
    }

    @CalledByNative
    private void minimize() {
        mAndroidBrowserWindow.getTask().minimize();
    }

    @CalledByNative
    private void restore() {
        mAndroidBrowserWindow.getTask().restore();
    }

    @CalledByNative
    private void setBounds(int left, int top, int right, int bottom) {
        mAndroidBrowserWindow.getTask().setBoundsInDp(new Rect(left, top, right, bottom));
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAndroidBaseWindow = 0;
    }

    @CalledByNative
    private @Nullable WindowAndroid getWindowAndroid() {
        return mAndroidBrowserWindow.getActivityWindowAndroid();
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
