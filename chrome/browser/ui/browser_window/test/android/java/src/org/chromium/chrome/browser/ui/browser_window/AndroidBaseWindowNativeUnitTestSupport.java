// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockingDetails;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Rect;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/**
 * Supports {@code android_base_window_unittest.cc}.
 *
 * <p>The native unit test will use this class to:
 *
 * <ul>
 *   <li>Instantiate a Java {@code AndroidBaseWindow} and its native counterpart; and
 *   <li>Test the Java {@code AndroidBaseWindow} methods.
 * </ul>
 */
@NullMarked
final class AndroidBaseWindowNativeUnitTestSupport {
    private final AndroidBaseWindow mAndroidBaseWindow;
    private final ChromeAndroidTask mChromeAndroidTask;

    @CalledByNative
    private AndroidBaseWindowNativeUnitTestSupport() {
        mChromeAndroidTask = mock(ChromeAndroidTask.class);
        mAndroidBaseWindow = new AndroidBaseWindow(mChromeAndroidTask);
    }

    @CalledByNative
    private long invokeGetOrCreateNativePtr() {
        return mAndroidBaseWindow.getOrCreateNativePtr();
    }

    @CalledByNative
    private long invokeGetNativePtrForTesting() {
        return mAndroidBaseWindow.getNativePtrForTesting();
    }

    @CalledByNative
    private void invokeDestroy() {
        mAndroidBaseWindow.destroy();
    }

    @CalledByNative
    private void verifyBoundsToSet(int left, int top, int right, int bottom) {
        assert mockingDetails(mChromeAndroidTask).isMock();
        verify(mChromeAndroidTask).setBoundsInDp(new Rect(left, top, right, bottom));
    }

    @CalledByNative
    private void setFakeBounds(int left, int top, int right, int bottom) {
        assert mockingDetails(mChromeAndroidTask).isMock();
        when(mChromeAndroidTask.getBoundsInDp()).thenReturn(new Rect(left, top, right, bottom));
    }
}
