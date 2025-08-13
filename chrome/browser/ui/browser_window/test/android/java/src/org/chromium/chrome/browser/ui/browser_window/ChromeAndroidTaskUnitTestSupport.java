// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

import android.app.Activity;
import android.view.WindowManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
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
        public final ActivityWindowAndroidMocks mActivityWindowAndroidMocks;

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
                ActivityWindowAndroidMocks activityWindowAndroidMocks,
                AndroidBrowserWindow.@Nullable Natives mockAndroidBrowserWindowNatives) {
            mChromeAndroidTask = chromeAndroidTask;
            mActivityWindowAndroidMocks = activityWindowAndroidMocks;
            mMockAndroidBrowserWindowNatives = mockAndroidBrowserWindowNatives;
        }
    }

    /** Holds mocks relevant to {@link ActivityWindowAndroid}. */
    public static final class ActivityWindowAndroidMocks {
        public final ActivityWindowAndroid mMockActivityWindowAndroid;
        public final Activity mMockActivity;
        public final ActivityLifecycleDispatcher mMockActivityLifecycleDispatcher;

        /** Mock {@link WindowManager} for {@link #mMockActivity}. */
        public final WindowManager mMockWindowManager;

        public ActivityWindowAndroidMocks(
                ActivityWindowAndroid mockActivityWindowAndroid,
                Activity mockActivity,
                ActivityLifecycleDispatcher mockActivityLifecycleDispatcher,
                WindowManager mockWindowManager) {
            mMockActivityWindowAndroid = mockActivityWindowAndroid;
            mMockActivity = mockActivity;
            mMockActivityLifecycleDispatcher = mockActivityLifecycleDispatcher;
            mMockWindowManager = mockWindowManager;
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
        var activityWindowAndroidMocks = createActivityWindowAndroidMocks(taskId);
        var mockAndroidBrowserWindowNatives =
                mockNatives ? createMockAndroidBrowserWindowNatives() : null;
        var chromeAndroidTask =
                new ChromeAndroidTaskImpl(activityWindowAndroidMocks.mMockActivityWindowAndroid);

        return new ChromeAndroidTaskWithMockDeps(
                chromeAndroidTask, activityWindowAndroidMocks, mockAndroidBrowserWindowNatives);
    }

    /** See {@link #createActivityWindowAndroidMocks(int)}. */
    static ActivityWindowAndroid createMockActivityWindowAndroid(int taskId) {
        return createActivityWindowAndroidMocks(taskId).mMockActivityWindowAndroid;
    }

    /**
     * Creates an instance of {@link ActivityWindowAndroidMocks}.
     *
     * @param taskId Task ID for {@link ActivityWindowAndroidMocks#mMockActivity}.
     */
    static ActivityWindowAndroidMocks createActivityWindowAndroidMocks(int taskId) {
        var mockActivityWindowAndroid = mock(ActivityWindowAndroid.class);
        var mockActivity =
                mock(
                        Activity.class,
                        withSettings().extraInterfaces(ActivityLifecycleDispatcherProvider.class));
        var mockActivityLifecycleDispatcher = mock(ActivityLifecycleDispatcher.class);
        var mockWindowManager = mock(WindowManager.class);

        when(mockActivity.getTaskId()).thenReturn(taskId);
        when(mockActivity.getWindowManager()).thenReturn(mockWindowManager);
        when(((ActivityLifecycleDispatcherProvider) mockActivity).getLifecycleDispatcher())
                .thenReturn(mockActivityLifecycleDispatcher);
        when(mockActivityWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mockActivity));

        return new ActivityWindowAndroidMocks(
                mockActivityWindowAndroid,
                mockActivity,
                mockActivityLifecycleDispatcher,
                mockWindowManager);
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
