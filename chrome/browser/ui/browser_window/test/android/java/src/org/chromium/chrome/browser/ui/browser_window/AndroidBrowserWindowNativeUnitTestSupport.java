// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

/**
 * Supports {@code android_browser_window_unittest.cc}.
 *
 * <p>The native unit test will use this class to:
 *
 * <ul>
 *   <li>Instantiate a Java {@code AndroidBrowserWindow} and its native counterpart; and
 *   <li>Test the Java {@code AndroidBrowserWindow} methods.
 * </ul>
 */
@NullMarked
final class AndroidBrowserWindowNativeUnitTestSupport {
    private final AndroidBrowserWindow mAndroidBrowserWindow;
    private final ChromeAndroidTask mMockChromeAndroidTask;

    @CalledByNative
    private AndroidBrowserWindowNativeUnitTestSupport(Profile profile) {
        this(BrowserWindowType.NORMAL, profile);
    }

    @CalledByNative
    private AndroidBrowserWindowNativeUnitTestSupport(
            @BrowserWindowType int browserWindowType, Profile profile) {
        mMockChromeAndroidTask = mock(ChromeAndroidTask.class);
        when(mMockChromeAndroidTask.getBrowserWindowType()).thenReturn(browserWindowType);
        mAndroidBrowserWindow = new AndroidBrowserWindow(mMockChromeAndroidTask);

        ProfileManager.setLastUsedProfileForTesting(profile);
        setProfileForTesting(profile);
    }

    @CalledByNative
    private long invokeGetOrCreateNativePtr() {
        return mAndroidBrowserWindow.getOrCreateNativePtr();
    }

    @CalledByNative
    private long invokeGetOrCreateNativeBaseWindowPtr() {
        return mAndroidBrowserWindow.getOrCreateNativeBaseWindowPtr();
    }

    @CalledByNative
    private long invokeGetNativePtrForTesting() {
        return mAndroidBrowserWindow.getNativePtrForTesting();
    }

    @CalledByNative
    private long invokeGetNativeBaseWindowPtrForTesting() {
        return mAndroidBrowserWindow.getNativeBaseWindowPtrForTesting();
    }

    @CalledByNative
    private void invokeResetAndDestroy() {
        mAndroidBrowserWindow.destroy();
        ProfileManager.resetForTesting();
    }

    @CalledByNative
    private void setProfileForTesting(Profile profile) {
        when(mMockChromeAndroidTask.getProfile()).thenReturn(profile);
    }
}
