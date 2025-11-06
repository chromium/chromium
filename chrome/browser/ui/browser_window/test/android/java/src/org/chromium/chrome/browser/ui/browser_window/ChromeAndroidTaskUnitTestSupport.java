// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockingDetails;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Intent;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.util.Pair;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.WindowMetrics;

import androidx.annotation.RequiresApi;
import androidx.core.view.WindowInsetsControllerCompat;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.Promise;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.mojom.WindowShowState;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/** Supports Robolectric and native unit tests relevant to {@link ChromeAndroidTask}. */
@NullMarked
public final class ChromeAndroidTaskUnitTestSupport {

    /**
     * Default bounds (in pixels) intended for {@link WindowManager#getCurrentWindowMetrics()}.
     *
     * @see #mockDesktopWindowingMode
     */
    public static final Rect DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX = new Rect(10, 20, 800, 600);

    /**
     * Default bounds (in pixels) intended for {@link WindowManager#getMaximumWindowMetrics()}.
     *
     * @see #mockDesktopWindowingMode
     */
    public static final Rect DEFAULT_FULL_SCREEN_BOUNDS_IN_PX = new Rect(0, 0, 1920, 1080);

    /**
     * Default tappable {@link Insets} (in pixels) intended for max window metrics in {@link
     * WindowManager}.
     *
     * @see #mockDesktopWindowingMode
     */
    public static final Insets DEFAULT_MAX_TAPPABLE_INSETS_IN_PX = Insets.of(0, 10, 0, 20);

    /**
     * Default maximized window bounds (in desktop-windowing mode) when {@link WindowManager} is
     * configured using {@link #DEFAULT_FULL_SCREEN_BOUNDS_IN_PX} and {@link
     * #DEFAULT_MAX_TAPPABLE_INSETS_IN_PX}.
     *
     * @see #mockDesktopWindowingMode
     */
    public static final Rect DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX =
            new Rect(
                    0,
                    DEFAULT_MAX_TAPPABLE_INSETS_IN_PX.top,
                    DEFAULT_FULL_SCREEN_BOUNDS_IN_PX.right,
                    DEFAULT_FULL_SCREEN_BOUNDS_IN_PX.bottom
                            - DEFAULT_MAX_TAPPABLE_INSETS_IN_PX.bottom);

    /**
     * It's common for callers of {@link #createMockActivityWindowAndroid} or {@link
     * #createActivityWindowAndroidMocks} to pass the resulting mocks into a {@link
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
        public final ChromeAndroidTask.ActivityScopedObjects mActivityScopedObjects;
        public final ActivityWindowAndroidMocks mActivityWindowAndroidMocks;
        public final Profile mMockProfile;
        public final AppTask mMockAppTask;
        public final AconfigFlaggedApiDelegate mMockAconfigFlaggedApiDelegate;

        /**
         * Mock {@link AndroidBrowserWindow.Natives}.
         *
         * <p>This is {@code null} if {@link #createChromeAndroidTaskWithMockDeps(int, boolean,
         * boolean)} was called with {@code mockNatives} set to false, in which case the test is
         * expected to run the real native code.
         */
        final AndroidBrowserWindow.@Nullable Natives mMockAndroidBrowserWindowNatives;

        ChromeAndroidTaskWithMockDeps(
                ChromeAndroidTask chromeAndroidTask,
                ChromeAndroidTask.ActivityScopedObjects activityScopedObjects,
                ActivityWindowAndroidMocks activityWindowAndroidMocks,
                Profile mockProfile,
                AppTask appTask,
                AconfigFlaggedApiDelegate aconfigFlaggedApiDelegate,
                AndroidBrowserWindow.@Nullable Natives mockAndroidBrowserWindowNatives) {
            mChromeAndroidTask = chromeAndroidTask;
            mActivityScopedObjects = activityScopedObjects;
            mActivityWindowAndroidMocks = activityWindowAndroidMocks;
            mMockProfile = mockProfile;
            mMockAppTask = appTask;
            mMockAconfigFlaggedApiDelegate = aconfigFlaggedApiDelegate;
            mMockAndroidBrowserWindowNatives = mockAndroidBrowserWindowNatives;
        }
    }

    /** Holds mocks relevant to {@link ActivityWindowAndroid}. */
    public static final class ActivityWindowAndroidMocks {
        public final ActivityWindowAndroid mMockActivityWindowAndroid;
        public final Activity mMockActivity;
        public final ActivityLifecycleDispatcher mMockActivityLifecycleDispatcher;
        public final DisplayAndroid mMockDisplayAndroid;

        /** Mock {@link WindowManager} for {@link #mMockActivity}. */
        public final WindowManager mMockWindowManager;

        public ActivityWindowAndroidMocks(
                ActivityWindowAndroid mockActivityWindowAndroid,
                Activity mockActivity,
                ActivityLifecycleDispatcher mockActivityLifecycleDispatcher,
                DisplayAndroid mockDisplayAndroid,
                WindowManager mockWindowManager) {
            mMockActivityWindowAndroid = mockActivityWindowAndroid;
            mMockActivity = mockActivity;
            mMockActivityLifecycleDispatcher = mockActivityLifecycleDispatcher;
            mMockDisplayAndroid = mockDisplayAndroid;
            mMockWindowManager = mockWindowManager;
        }
    }

    /** Fake native pointer value returned by {@link AndroidBrowserWindow.Natives#create}. */
    public static final long FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR = 123456789L;

    private ChromeAndroidTaskUnitTestSupport() {}

    /**
     * Creates a real {@link ChromeAndroidTask} with mock dependencies.
     *
     * @param taskId ID for {@link ChromeAndroidTask#getId()}.
     * @param mockNatives Whether to mock {@code @NativeMethods}. Set this to false if the test
     *     needs to run native code, such as in .cc unit tests. Tests that set this to false must
     *     initialize a Native ProfileManager with a valid profile.
     * @param isPendingTask If true, the returned {@link ChromeAndroidTask} will be in the pending
     *     state. The returned mock dependencies will not be connected with the pending {@link
     *     ChromeAndroidTask}. To connect the mocks with the pending {@link ChromeAndroidTask}, pass
     *     them to {@link ChromeAndroidTask#setActivityScopedObjects}.
     * @return A new instance of {@link ChromeAndroidTaskWithMockDeps}.
     */
    public static ChromeAndroidTaskWithMockDeps createChromeAndroidTaskWithMockDeps(
            int taskId, boolean mockNatives, boolean isPendingTask) {
        Profile profile =
                mockNatives ? mock(Profile.class) : ProfileManager.getLastUsedRegularProfile();
        var activityWindowAndroidMocks = createActivityWindowAndroidMocks(taskId);
        var activityScopedObjects =
                createMockActivityScopedObjects(
                        activityWindowAndroidMocks.mMockActivityWindowAndroid, profile);
        var mockAndroidBrowserWindowNatives =
                mockNatives ? createMockAndroidBrowserWindowNatives() : null;

        ChromeAndroidTask chromeAndroidTask =
                isPendingTask
                        ? new ChromeAndroidTaskImpl(createPendingTaskInfo())
                        : new ChromeAndroidTaskImpl(
                                BrowserWindowType.NORMAL, activityScopedObjects);

        var mockAppTask = mock(AppTask.class);
        AndroidTaskUtils.setAppTaskForTesting(mockAppTask);

        var mockApiDelegate = mock(AconfigFlaggedApiDelegate.class);
        AconfigFlaggedApiDelegate.setInstanceForTesting(mockApiDelegate);
        when(mockApiDelegate.isTaskMoveAllowedOnDisplay(any(), anyInt())).thenReturn(true);
        when(mockApiDelegate.moveTaskToWithPromise(any(), anyInt(), any()))
                .thenReturn(Promise.fulfilled(Pair.create(-1, new Rect())));

        return new ChromeAndroidTaskWithMockDeps(
                chromeAndroidTask,
                activityScopedObjects,
                activityWindowAndroidMocks,
                profile,
                mockAppTask,
                mockApiDelegate,
                mockAndroidBrowserWindowNatives);
    }

    /**
     * @see #createMockActivityScopedObjects(int, Profile)
     */
    static ChromeAndroidTask.ActivityScopedObjects createMockActivityScopedObjects(int taskId) {
        return createMockActivityScopedObjects(taskId, mock(Profile.class));
    }

    /**
     * Creates a {@link ChromeAndroidTask.ActivityScopedObjects} instance containing mock objects.
     *
     * @param taskId The Task ID of the {@code Activity} the mock objects are associated with.
     * @param profile The {@link Profile} the mock objects are associated with.
     * @return The new {@link ChromeAndroidTask.ActivityScopedObjects} instance.
     */
    static ChromeAndroidTask.ActivityScopedObjects createMockActivityScopedObjects(
            int taskId, Profile profile) {
        var activityWindowAndroid =
                createActivityWindowAndroidMocks(taskId).mMockActivityWindowAndroid;
        return createMockActivityScopedObjects(activityWindowAndroid, profile);
    }

    /**
     * Creates a {@link ChromeAndroidTask.ActivityScopedObjects} instance containing mock objects.
     *
     * @param activityWindowAndroid The {@link ActivityWindowAndroid} for the {@code Activity} the
     *     mock objects are associated with.
     * @param profile The {@link Profile} the mock objects are associated with.
     * @return The new {@link ChromeAndroidTask.ActivityScopedObjects} instance.
     */
    static ChromeAndroidTask.ActivityScopedObjects createMockActivityScopedObjects(
            ActivityWindowAndroid activityWindowAndroid, Profile profile) {
        assert mockingDetails(activityWindowAndroid).isMock();

        // TODO(http://crbug.com/454954191): Use the "MockTabModel" class.
        var mockTabModel = mock(TabModel.class);
        when(mockTabModel.getProfile()).thenReturn(profile);

        var mockMultiInstanceManager = createMockMultiInstanceManager();

        return new ChromeAndroidTask.ActivityScopedObjects(
                activityWindowAndroid, mockTabModel, mockMultiInstanceManager);
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
        var mockDisplay = mock(DisplayAndroid.class);
        var mockInsetObserver = mock(InsetObserver.class);

        when(mockActivity.getTaskId()).thenReturn(taskId);
        when(mockActivity.getWindowManager()).thenReturn(mockWindowManager);
        when(mockActivity.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mockActivityManager);
        when(((ActivityLifecycleDispatcherProvider) mockActivity).getLifecycleDispatcher())
                .thenReturn(mockActivityLifecycleDispatcher);

        when(mockDisplay.getDipScale()).thenReturn(1.0f);

        when(mockActivityWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mockActivity));
        when(mockActivityWindowAndroid.getDisplay()).thenReturn(mockDisplay);
        when(mockActivityWindowAndroid.getInsetObserver()).thenReturn(mockInsetObserver);

        var mocks =
                new ActivityWindowAndroidMocks(
                        mockActivityWindowAndroid,
                        mockActivity,
                        mockActivityLifecycleDispatcher,
                        mockDisplay,
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
    public static AndroidBrowserWindow.Natives createMockAndroidBrowserWindowNatives() {
        var mockAndroidBrowserWindowNatives = mock(AndroidBrowserWindow.Natives.class);
        when(mockAndroidBrowserWindowNatives.create(
                        /* caller= */ any(),
                        /* browserWindowType= */ anyInt(),
                        /* profile= */ any()))
                .thenReturn(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        AndroidBrowserWindowJni.setInstanceForTesting(mockAndroidBrowserWindowNatives);

        return mockAndroidBrowserWindowNatives;
    }

    static ChromeAndroidTask.PendingTaskInfo createPendingTaskInfo() {
        return createPendingTaskInfo(createMockAndroidBrowserWindowCreateParams());
    }

    static ChromeAndroidTask.PendingTaskInfo createPendingTaskInfo(
            AndroidBrowserWindowCreateParams createParams) {
        JniOnceCallback<Long> mockCallback = mock();

        return new ChromeAndroidTask.PendingTaskInfo(
                IdSequencer.next(), createParams, new Intent(), mockCallback);
    }

    /**
     * @see #createMockAndroidBrowserWindowCreateParams(int, Rect, int)
     */
    static AndroidBrowserWindowCreateParams createMockAndroidBrowserWindowCreateParams() {
        return createMockAndroidBrowserWindowCreateParams(
                BrowserWindowType.NORMAL, new Rect(), WindowShowState.DEFAULT);
    }

    /**
     * @see #createMockAndroidBrowserWindowCreateParams(int, Rect, int)
     */
    static AndroidBrowserWindowCreateParams createMockAndroidBrowserWindowCreateParams(
            @BrowserWindowType int windowType) {
        return createMockAndroidBrowserWindowCreateParams(
                windowType, new Rect(), WindowShowState.DEFAULT);
    }

    /**
     * Creates an {@link AndroidBrowserWindowCreateParams} mock.
     *
     * @param windowType The mock {@link BrowserWindowType} to set in the create params.
     * @param launchBounds The launch bounds to set in the create params.
     * @param showState The mock {@link WindowShowState} to set in the create params.
     * @return The {@link AndroidBrowserWindowCreateParams} mock.
     */
    static AndroidBrowserWindowCreateParams createMockAndroidBrowserWindowCreateParams(
            @BrowserWindowType int windowType,
            Rect launchBounds,
            @WindowShowState.EnumType int showState) {
        var mockParams = mock(AndroidBrowserWindowCreateParams.class);
        when(mockParams.getWindowType()).thenReturn(windowType);
        Profile mockProfile = mock(Profile.class);
        when(mockParams.getProfile()).thenReturn(mockProfile);
        when(mockParams.getInitialBounds()).thenReturn(launchBounds);
        when(mockParams.getInitialShowState()).thenReturn(showState);

        return mockParams;
    }

    /** See {@link #mockDesktopWindowingMode(ActivityWindowAndroidMocks, Rect, Rect, Insets)}. */
    @RequiresApi(api = VERSION_CODES.R)
    static void mockDesktopWindowingMode(ActivityWindowAndroidMocks activityWindowAndroidMocks) {
        mockDesktopWindowingMode(
                activityWindowAndroidMocks,
                DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX,
                DEFAULT_FULL_SCREEN_BOUNDS_IN_PX,
                DEFAULT_MAX_TAPPABLE_INSETS_IN_PX);
    }

    /**
     * Configures the provided {@code ActivityWindowAndroidMocks} to meet the expectations of
     * desktop windowing mode.
     *
     * <p>Only use this in Robolectric tests. Native unit tests run on an emulator, and Mockito will
     * fail to mock "final" framework classes like {@link WindowMetrics}.
     *
     * @param activityWindowAndroidMocks The mocks to configure.
     * @param currentWindowBoundsInPx Bounds (in pixels) intended for {@link
     *     WindowManager#getCurrentWindowMetrics()}.
     * @param fullScreenWindowBoundsInPx Bounds (in pixels) intended for {@link
     *     WindowManager#getMaximumWindowMetrics()}.
     * @param maxTappableInsetsInPx {@link Insets} (in pixels) intended for max window metrics in
     *     {@link WindowManager}.
     */
    @RequiresApi(api = VERSION_CODES.R)
    static void mockDesktopWindowingMode(
            ActivityWindowAndroidMocks activityWindowAndroidMocks,
            Rect currentWindowBoundsInPx,
            Rect fullScreenWindowBoundsInPx,
            Insets maxTappableInsetsInPx) {
        var mockActivity = activityWindowAndroidMocks.mMockActivity;
        var mockWindowManager = activityWindowAndroidMocks.mMockWindowManager;

        // Activity should be in multi-window mode.
        // (Desktop windowing mode is a multi-window mode.)
        when(mockActivity.isInMultiWindowMode()).thenReturn(true);

        // Config system bars behavior.
        var mockWindow = mock(Window.class);
        var mockWindowInsetsController = mock(WindowInsetsController.class);
        when(mockWindowInsetsController.getSystemBarsBehavior())
                .thenReturn(WindowInsetsControllerCompat.BEHAVIOR_DEFAULT);
        when(mockWindow.getInsetsController()).thenReturn(mockWindowInsetsController);
        when(mockActivity.getWindow()).thenReturn(mockWindow);

        mockCurrentWindowMetrics(mockWindowManager, currentWindowBoundsInPx);
        mockMaxWindowMetrics(mockWindowManager, fullScreenWindowBoundsInPx, maxTappableInsetsInPx);

        // Connect mock WindowManager to mock Activity.
        when(mockActivity.getWindowManager()).thenReturn(mockWindowManager);
    }

    @RequiresApi(api = VERSION_CODES.R)
    static void mockCurrentWindowMetrics(
            WindowManager mockWindowManager, Rect currentWindowBoundsInPx) {
        assert mockingDetails(mockWindowManager).isMock();

        var currentWindowMetrics = mock(WindowMetrics.class);
        when(currentWindowMetrics.getBounds()).thenReturn(currentWindowBoundsInPx);
        when(mockWindowManager.getCurrentWindowMetrics()).thenReturn(currentWindowMetrics);
    }

    @RequiresApi(api = VERSION_CODES.R)
    static void mockMaxWindowMetrics(
            WindowManager mockWindowManager,
            Rect fullScreenWindowBoundsInPx,
            Insets maxTappableInsetsInPx) {
        assert mockingDetails(mockWindowManager).isMock();

        var maxWindowInsets = mock(WindowInsets.class);
        when(maxWindowInsets.isVisible(WindowInsets.Type.statusBars())).thenReturn(true);
        when(maxWindowInsets.getInsets(WindowInsets.Type.tappableElement()))
                .thenReturn(maxTappableInsetsInPx);
        var maxWindowMetrics = mock(WindowMetrics.class);
        when(maxWindowMetrics.getBounds()).thenReturn(fullScreenWindowBoundsInPx);
        when(maxWindowMetrics.getWindowInsets()).thenReturn(maxWindowInsets);
        when(mockWindowManager.getMaximumWindowMetrics()).thenReturn(maxWindowMetrics);
    }

    private static MultiInstanceManager createMockMultiInstanceManager() {
        var mockMultiInstanceManager = mock(MultiInstanceManager.class);

        // Unit tests don't need to care what the Intent is. They only need to verify the correct
        // MultiInstanceManager API is called.
        //
        // The Intent here is the bare minimum to ensure unit tests pass:
        // (1) The Intent is not null; and
        // (2) The Intent has the FLAG_ACTIVITY_NEW_TASK flag to avoid the "background Activity
        // launch" error in unit tests.
        var intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        when(mockMultiInstanceManager.createNewWindowIntent(anyBoolean())).thenReturn(intent);
        return mockMultiInstanceManager;
    }
}
