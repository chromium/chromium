// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.lang.ref.WeakReference;

/** Supports Robolectric and native unit tests relevant to {@link ChromeAndroidTask}. */
@NullMarked
public final class ChromeAndroidTaskUnitTestSupport {

    /**
     * Holds a real {@link ChromeAndroidTask} and its mock dependencies.
     *
     * <p>Unit tests can obtain an instance of this class via {@link
     * #createChromeAndroidTaskWithMockDeps}.
     */
    public static final class ChromeAndroidTaskWithMockDeps {
        public final ChromeAndroidTask mChromeAndroidTask;
        public final ActivityWindowAndroid mMockActivityWindowAndroid;

        /**
         * Mock {@link AndroidBrowserWindow.Natives}.
         *
         * <p>This is {@code null} if {@link #createChromeAndroidTaskWithMockDeps(int, boolean)} was
         * called with {@code mockNatives} set to false, in which case the test is expected to run
         * the real native code.
         */
        final AndroidBrowserWindow.@Nullable Natives mMockAndroidBrowserWindowNatives;

        ChromeAndroidTaskWithMockDeps(
                ChromeAndroidTask chromeAndroidTask,
                ActivityWindowAndroid mockActivityWindowAndroid,
                AndroidBrowserWindow.@Nullable Natives mockAndroidBrowserWindowNatives) {
            mChromeAndroidTask = chromeAndroidTask;
            mMockActivityWindowAndroid = mockActivityWindowAndroid;
            mMockAndroidBrowserWindowNatives = mockAndroidBrowserWindowNatives;
        }
    }

    /** Fake native pointer value returned by {@link AndroidBrowserWindow.Natives#create}. */
    public static final long FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR = 123456789L;

    private ChromeAndroidTaskUnitTestSupport() {}

    /** See {@link #createChromeAndroidTaskWithMockDeps(int, boolean)}. */
    public static ChromeAndroidTaskWithMockDeps createChromeAndroidTaskWithMockDeps(int taskId) {
        return createChromeAndroidTaskWithMockDeps(taskId, /* mockNatives= */ true);
    }

    /**
     * Creates a real {@link ChromeAndroidTask} with mock dependencies.
     *
     * @param taskId ID for {@link ChromeAndroidTask#getId()}.
     * @param mockNatives Whether to mock {@code @NativeMethods}. Set this to false if the test
     *     needs to run native code, such as in .cc unit tests.
     * @return A new instance of {@link ChromeAndroidTaskWithMockDeps}.
     */
    public static ChromeAndroidTaskWithMockDeps createChromeAndroidTaskWithMockDeps(
            int taskId, boolean mockNatives) {
        var mockActivityWindowAndroid = createMockActivityWindowAndroid(taskId);
        var mockAndroidBrowserWindowNatives =
                mockNatives ? createMockAndroidBrowserWindowNatives() : null;
        var chromeAndroidTask = new ChromeAndroidTaskImpl(mockActivityWindowAndroid);

        return new ChromeAndroidTaskWithMockDeps(
                chromeAndroidTask, mockActivityWindowAndroid, mockAndroidBrowserWindowNatives);
    }

    /**
     * Creates a mock {@link ActivityWindowAndroid}, which has a mock {@link Activity} with the
     * given {@code taskId}.
     */
    static ActivityWindowAndroid createMockActivityWindowAndroid(int taskId) {
        var mockActivityWindowAndroid = mock(ActivityWindowAndroid.class);
        var mockActivity = mock(Activity.class);

        when(mockActivity.getTaskId()).thenReturn(taskId);
        when(mockActivityWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mockActivity));
        return mockActivityWindowAndroid;
    }

    /**
     * Creates a mock {@link AndroidBrowserWindow.Natives} that returns {@link
     * #FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR} when {@link AndroidBrowserWindow.Natives#create} is
     * called.
     *
     * <p>This method also sets the mock as the testing instance for {@link
     * AndroidBrowserWindowJni}.
     */
    private static AndroidBrowserWindow.Natives createMockAndroidBrowserWindowNatives() {
        var mockAndroidBrowserWindowNatives = mock(AndroidBrowserWindow.Natives.class);
        when(mockAndroidBrowserWindowNatives.create(any()))
                .thenReturn(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        AndroidBrowserWindowJni.setInstanceForTesting(mockAndroidBrowserWindowNatives);

        return mockAndroidBrowserWindowNatives;
    }
}
