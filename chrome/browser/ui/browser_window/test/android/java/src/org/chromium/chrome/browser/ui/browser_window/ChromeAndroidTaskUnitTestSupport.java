// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.view.WindowManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/** Supports Robolectric and native unit tests relevant to {@link ChromeAndroidTask}. */
@NullMarked
public final class ChromeAndroidTaskUnitTestSupport {

    /**
     * It's common for callers of {@link #createMockActivityWindowAndroid()} or {@link
     * #createActivityWindowAndroidMocks()} to pass the resulting mocks into a {@link
     * ChromeAndroidTaskImpl}, which only holds it as a weak reference. Pinning the mocks here
     * ensures that they don't get garbage collected in the middle of a unit test.
     *
     * <p>The key to the map is an Android Task ID.
     */
    private static final Map<Integer, ActivityWindowAndroidMocks> sActivityWindowAndroidMocks =
            new HashMap<>();

    /**
     * Holds a real {@link ChromeAndroidTask} and its mock dependencies.
     *
     * <p>Unit tests can obtain an instance of this class via {@link
     * #createChromeAndroidTaskWithMockDeps}.
     */
    public static final class ChromeAndroidTaskWithMockDeps {
        public final ChromeAndroidTask mChromeAndroidTask;
        public final ActivityWindowAndroidMocks mActivityWindowAndroidMocks;
        public final Profile mMockProfile;
        public final TabModel mMockTabModel;

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
                Profile mockProfile,
                TabModel mockTabModel,
                AndroidBrowserWindow.@Nullable Natives mockAndroidBrowserWindowNatives) {
            mChromeAndroidTask = chromeAndroidTask;
            mActivityWindowAndroidMocks = activityWindowAndroidMocks;
            mMockProfile = mockProfile;
            mMockTabModel = mockTabModel;
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
     *     needs to run native code, such as in .cc unit tests. Tests that set this to false must
     *     initialize a Native ProfileManager with a valid profile.
     * @return A new instance of {@link ChromeAndroidTaskWithMockDeps}.
     */
    public static ChromeAndroidTaskWithMockDeps createChromeAndroidTaskWithMockDeps(
            int taskId, boolean mockNatives) {
        Profile profile =
                mockNatives ? mock(Profile.class) : ProfileManager.getLastUsedRegularProfile();
        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(profile);

        var activityWindowAndroidMocks = createActivityWindowAndroidMocks(taskId);
        var mockAndroidBrowserWindowNatives =
                mockNatives ? createMockAndroidBrowserWindowNatives() : null;
        var chromeAndroidTask =
                new ChromeAndroidTaskImpl(
                        BrowserWindowType.NORMAL,
                        activityWindowAndroidMocks.mMockActivityWindowAndroid,
                        tabModel);

        return new ChromeAndroidTaskWithMockDeps(
                chromeAndroidTask,
                activityWindowAndroidMocks,
                profile,
                tabModel,
                mockAndroidBrowserWindowNatives);
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
        var mockActivityManager = mock(ActivityManager.class);

        when(mockActivity.getTaskId()).thenReturn(taskId);
        when(mockActivity.getWindowManager()).thenReturn(mockWindowManager);
        when(mockActivity.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mockActivityManager);
        when(((ActivityLifecycleDispatcherProvider) mockActivity).getLifecycleDispatcher())
                .thenReturn(mockActivityLifecycleDispatcher);
        when(mockActivityWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mockActivity));

        var mocks =
                new ActivityWindowAndroidMocks(
                        mockActivityWindowAndroid,
                        mockActivity,
                        mockActivityLifecycleDispatcher,
                        mockWindowManager);
        sActivityWindowAndroidMocks.put(taskId, mocks);
        return mocks;
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
        when(mockAndroidBrowserWindowNatives.create(
                        /* caller= */ any(),
                        /* browserWindowType= */ anyInt(),
                        /* profile= */ any()))
                .thenReturn(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        AndroidBrowserWindowJni.setInstanceForTesting(mockAndroidBrowserWindowNatives);

        return mockAndroidBrowserWindowNatives;
    }

    /**
     * Creates an {@link AndroidBrowserWindowCreateParams} mock.
     *
     * @return The {@link AndroidBrowserWindowCreateParams} mock.
     */
    static AndroidBrowserWindowCreateParams createMockAndroidBrowserWindowCreateParams() {
        var mockParams = mock(AndroidBrowserWindowCreateParams.class);
        when(mockParams.getWindowType()).thenReturn(BrowserWindowType.NORMAL);
        Profile mockProfile = mock(Profile.class);
        when(mockParams.getProfile()).thenReturn(mockProfile);

        return mockParams;
    }
}
