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
    private boolean isActive() {
        return mChromeAndroidTask.isActive();
    }

    @CalledByNative
    private boolean isMaximized() {
        return mChromeAndroidTask.isMaximized();
    }

    @CalledByNative
    private boolean isMinimized() {
        return mChromeAndroidTask.isMinimized();
    }

    @CalledByNative
    private boolean isFullscreen() {
        return mChromeAndroidTask.isFullscreen();
    }

    @CalledByNative
    @JniType("std::vector<int>")
    private int[] getRestoredBounds() {
        Rect bounds = mChromeAndroidTask.getRestoredBoundsInDp();
        return new int[] {bounds.left, bounds.top, bounds.width(), bounds.height()};
    }

    @CalledByNative
    @JniType("std::vector<int>")
    private int[] getBounds() {
        Rect bounds = mChromeAndroidTask.getBoundsInDp();
        return new int[] {bounds.left, bounds.top, bounds.width(), bounds.height()};
    }

    @CalledByNative
    private void show() {
        mChromeAndroidTask.show();
    }

    @CalledByNative
    private boolean isVisible() {
        return mChromeAndroidTask.isVisible();
    }

    @CalledByNative
    private void showInactive() {
        mChromeAndroidTask.showInactive();
    }

    @CalledByNative
    private void close() {
        mChromeAndroidTask.close();
    }

    @CalledByNative
    private void activate() {
        mChromeAndroidTask.activate();
    }

    @CalledByNative
    private void deactivate() {
        mChromeAndroidTask.deactivate();
    }

    @CalledByNative
    private void maximize() {
        mChromeAndroidTask.maximize();
    }

    @CalledByNative
    private void minimize() {
        mChromeAndroidTask.minimize();
    }

    @CalledByNative
    private void restore() {
        mChromeAndroidTask.restore();
    }

    @CalledByNative
    private void setBounds(int left, int top, int right, int bottom) {
        mChromeAndroidTask.setBoundsInDp(new Rect(left, top, right, bottom));
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAndroidBaseWindow = 0;
    }

    @CalledByNative
    private @Nullable WindowAndroid getWindowAndroid() {
        return mChromeAndroidTask.getActivityWindowAndroid();
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
