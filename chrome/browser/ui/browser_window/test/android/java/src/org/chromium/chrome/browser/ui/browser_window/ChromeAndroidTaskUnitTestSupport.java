// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockingDetails;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.Pair;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.WindowMetrics;

import androidx.annotation.RequiresApi;
import androidx.core.view.WindowInsetsControllerCompat;

import org.jni_zero.JniPtr;
import org.jni_zero.JniUniquePtr;

import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.PopupCreator;
import org.chromium.chrome.browser.customtabs.PopupCreatorFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.NativeBrowserWindowInterface;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.mojom.WindowShowState;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Supports Robolectric and native unit tests relevant to {@link ChromeAndroidTask}. */
@NullMarked
public final class ChromeAndroidTaskUnitTestSupport {

    static final class FakeDecorView extends View {
        private int mAddOnLayoutChangeListenerCount;
        private int mRemoveOnLayoutChangeListenerCount;

        FakeDecorView(Context context) {
            super(context);
        }

        @Override
        public void getLocationOnScreen(int[] outLocation) {
            outLocation[0] = getLeft();
            outLocation[1] = getTop();
        }

        @Override
        public void addOnLayoutChangeListener(OnLayoutChangeListener listener) {
            super.addOnLayoutChangeListener(listener);
            mAddOnLayoutChangeListenerCount++;
        }

        @Override
        public void removeOnLayoutChangeListener(OnLayoutChangeListener listener) {
            super.removeOnLayoutChangeListener(listener);
            mRemoveOnLayoutChangeListenerCount++;
        }

        int getAddOnLayoutChangeListenerCount() {
            return mAddOnLayoutChangeListenerCount;
        }

        int getRemoveOnLayoutChangeListenerCount() {
            return mRemoveOnLayoutChangeListenerCount;
        }
    }

    /**
     * Fake {@link Activity} that implements {@link ActivityLifecycleDispatcherProvider} and
     * supplies test task ID, window, and services without requiring Robolectric (as this support
     * class is also used by native unit tests).
     */
    static final class FakeActivity extends Activity
            implements ActivityLifecycleDispatcherProvider {
        private final int mTaskId;
        private final Window mMockWindow;
        private final WindowManager mMockWindowManager;
        private final ActivityManager mMockActivityManager;
        private final ActivityLifecycleDispatcher mMockLifecycleDispatcher;
        private boolean mIsInMultiWindowMode;

        FakeActivity(
                int taskId,
                Window mockWindow,
                FakeDecorView fakeDecorView,
                WindowManager mockWindowManager,
                ActivityManager mockActivityManager,
                ActivityLifecycleDispatcher mockLifecycleDispatcher) {
            assert mockingDetails(mockWindow).isMock();
            assert mockingDetails(mockWindowManager).isMock();
            assert mockingDetails(mockActivityManager).isMock();
            assert mockingDetails(mockLifecycleDispatcher).isMock();

            attachBaseContext(ContextUtils.getApplicationContext());
            when(mockWindow.getDecorView()).thenReturn(fakeDecorView);

            mTaskId = taskId;
            mMockWindow = mockWindow;
            mMockWindowManager = mockWindowManager;
            mMockActivityManager = mockActivityManager;
            mMockLifecycleDispatcher = mockLifecycleDispatcher;
        }

        @Override
        public int getTaskId() {
            return mTaskId;
        }

        @Override
        public Window getWindow() {
            return mMockWindow;
        }

        @Override
        public WindowManager getWindowManager() {
            return mMockWindowManager;
        }

        @Override
        public @Nullable Object getSystemService(String name) {
            if (Context.ACTIVITY_SERVICE.equals(name)) {
                return mMockActivityManager;
            }
            return super.getSystemService(name);
        }

        @Override
        public ActivityLifecycleDispatcher getLifecycleDispatcher() {
            return mMockLifecycleDispatcher;
        }

        @Override
        public boolean isInMultiWindowMode() {
            return mIsInMultiWindowMode;
        }

        void setInMultiWindowModeForTesting(boolean isInMultiWindowMode) {
            mIsInMultiWindowMode = isInMultiWindowMode;
        }
    }

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
        public final ChromeAndroidTaskImpl mChromeAndroidTask;
        public final ChromeAndroidTask.ActivityScopedObjects mActivityScopedObjects;
        public final ActivityWindowAndroidMocks mActivityWindowAndroidMocks;
        public final Profile mMockProfile;
        public final AppTask mMockAppTask;
        public final AndroidTaskUtils.MoveTaskDelegate mMockMoveTaskDelegate;

        /** Mock {@link AndroidBrowserWindow.Natives}. */
        final AndroidBrowserWindow.@Nullable Natives mMockAndroidBrowserWindowNatives;

        ChromeAndroidTaskWithMockDeps(
                ChromeAndroidTaskImpl chromeAndroidTask,
                ChromeAndroidTask.ActivityScopedObjects activityScopedObjects,
                ActivityWindowAndroidMocks activityWindowAndroidMocks,
                Profile mockProfile,
                AppTask appTask,
                AndroidTaskUtils.MoveTaskDelegate moveTaskDelegate,
                AndroidBrowserWindow.@Nullable Natives mockAndroidBrowserWindowNatives) {
            mChromeAndroidTask = chromeAndroidTask;
            mActivityScopedObjects = activityScopedObjects;
            mActivityWindowAndroidMocks = activityWindowAndroidMocks;
            mMockProfile = mockProfile;
            mMockAppTask = appTask;
            mMockMoveTaskDelegate = moveTaskDelegate;
            mMockAndroidBrowserWindowNatives = mockAndroidBrowserWindowNatives;
        }
    }

    /** Holds mocks relevant to {@link ActivityWindowAndroid}. */
    public static final class ActivityWindowAndroidMocks {
        public final ActivityWindowAndroid mMockActivityWindowAndroid;
        public final FakeActivity mFakeActivity;
        public final ActivityLifecycleDispatcher mMockActivityLifecycleDispatcher;
        public final DisplayAndroid mMockDisplayAndroid;

        /** Decor {@link View} for {@link #mFakeActivity}. */
        public final View mDecorView;

        /** Mock {@link WindowManager} for {@link #mFakeActivity}. */
        public final WindowManager mMockWindowManager;

        /** Mock {@link Window} for {@link #mFakeActivity}. */
        public final Window mMockWindow;

        public ActivityWindowAndroidMocks(
                ActivityWindowAndroid mockActivityWindowAndroid,
                FakeActivity fakeActivity,
                ActivityLifecycleDispatcher mockActivityLifecycleDispatcher,
                DisplayAndroid mockDisplayAndroid,
                View decorView,
                WindowManager mockWindowManager,
                Window mockWindow) {
            mMockActivityWindowAndroid = mockActivityWindowAndroid;
            mFakeActivity = fakeActivity;
            mMockActivityLifecycleDispatcher = mockActivityLifecycleDispatcher;
            mMockDisplayAndroid = mockDisplayAndroid;
            mDecorView = decorView;
            mMockWindowManager = mockWindowManager;
            mMockWindow = mockWindow;
        }
    }

    /**
     * Fake native address of the pointer returned by {@link AndroidBrowserWindow.Natives#create},
     * as reported by {@link AndroidBrowserWindow.Natives#getLegacyNativePtr}.
     */
    public static final long FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR = 123456789L;

    public static final long FAKE_INCOGNITO_NATIVE_ANDROID_BROWSER_WINDOW_PTR = 987654321L;

    /** Mock pointer to its fake native address, removed when the pointer is destroyed. */
    private static final Map<JniUniquePtr<NativeBrowserWindowInterface>, Long>
            sFakeNativeAndroidBrowserWindowPtrs = new HashMap<>();

    /** Fake native address to the number of times a mock pointer with it was destroyed. */
    private static final Map<Long, Integer> sNativeAndroidBrowserWindowDestroyCounts =
            new HashMap<>();

    private static @Nullable PopupCreator sPopupCreator;
    private static @Nullable MultiInstanceOrchestrator sMultiInstanceOrchestrator;

    private ChromeAndroidTaskUnitTestSupport() {}

    /**
     * Creates a real {@link ChromeAndroidTask} with mock dependencies.
     *
     * @param taskId ID for {@link ChromeAndroidTask#getId()}.
     * @param isPendingTask If true, the returned {@link ChromeAndroidTask} will be in the pending
     *     state. The returned mock dependencies will not be connected with the pending {@link
     *     ChromeAndroidTask}. To connect the mocks with the pending {@link ChromeAndroidTask}, pass
     *     them to {@link ChromeAndroidTaskImpl#addActivityScopedObjects}.
     * @param isDesktopMode if true, mock the activity in a desktop mode with proper insets and
     *     screen bounds.
     * @return A new instance of {@link ChromeAndroidTaskWithMockDeps}.
     */
    public static ChromeAndroidTaskWithMockDeps createChromeAndroidTaskWithMockDeps(
            int taskId, boolean isPendingTask, boolean isDesktopMode) {
        return createChromeAndroidTaskWithMockDeps(
                taskId, isPendingTask, isDesktopMode, SupportedProfileType.REGULAR);
    }

    /**
     * Creates a real {@link ChromeAndroidTask} with mock dependencies.
     *
     * @param taskId ID for {@link ChromeAndroidTask#getId()}.
     * @param isPendingTask If true, the returned {@link ChromeAndroidTask} will be in the pending
     *     state. The returned mock dependencies will not be connected with the pending {@link
     *     ChromeAndroidTask}. To connect the mocks with the pending {@link ChromeAndroidTask}, pass
     *     them to {@link ChromeAndroidTaskImpl#addActivityScopedObjects}.
     * @param isDesktopMode if true, mock the activity in a desktop mode with proper insets and
     *     screen bounds.
     * @param profileType The {@link SupportedProfileType} for the task.
     * @return A new instance of {@link ChromeAndroidTaskWithMockDeps}.
     */
    public static ChromeAndroidTaskWithMockDeps createChromeAndroidTaskWithMockDeps(
            int taskId,
            boolean isPendingTask,
            boolean isDesktopMode,
            @SupportedProfileType int profileType) {
        Profile profile = mock(Profile.class);
        var activityWindowAndroidMocks = createActivityWindowAndroidMocks(taskId);
        if (isDesktopMode && VERSION.SDK_INT >= VERSION_CODES.R) {
            mockDesktopWindowingMode(activityWindowAndroidMocks);
        }
        var activityScopedObjects =
                createMockActivityScopedObjects(
                        activityWindowAndroidMocks.mMockActivityWindowAndroid,
                        profile,
                        BrowserWindowType.NORMAL,
                        profileType);
        var mockAndroidBrowserWindowNatives = createMockAndroidBrowserWindowNatives();

        var chromeAndroidTaskTracker = ChromeAndroidTaskTrackerImpl.getInstance();
        var chromeAndroidTask =
                isPendingTask
                        ? chromeAndroidTaskTracker.createPendingTask(
                                createMockAndroidBrowserWindowCreateParams(
                                        BrowserWindowType.NORMAL,
                                        new Rect(),
                                        WindowShowState.DEFAULT,
                                        profile),
                                null)
                        : (ChromeAndroidTaskImpl)
                                chromeAndroidTaskTracker.obtainTask(
                                        BrowserWindowType.NORMAL,
                                        activityScopedObjects,
                                        /* pendingId= */ null);
        assertNonNull(chromeAndroidTask);
        ResettersForTesting.register(chromeAndroidTaskTracker::removeAllForTesting);

        var mockAppTask = mock(AppTask.class);
        AndroidTaskUtils.setAppTaskForTesting(mockAppTask);

        var mockMoveTaskDelegate = mock(AndroidTaskUtils.MoveTaskDelegate.class);
        AndroidTaskUtils.setMoveTaskDelegateForTesting(mockMoveTaskDelegate);
        when(mockMoveTaskDelegate.moveTaskToWithPromise(any(), anyInt(), any()))
                .thenReturn(Promise.fulfilled(Pair.create(-1, new Rect())));

        return new ChromeAndroidTaskWithMockDeps(
                chromeAndroidTask,
                activityScopedObjects,
                activityWindowAndroidMocks,
                profile,
                mockAppTask,
                mockMoveTaskDelegate,
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
        return createMockActivityScopedObjects(taskId, profile, BrowserWindowType.NORMAL);
    }

    /**
     * Creates a {@link ChromeAndroidTask.ActivityScopedObjects} instance containing mock objects.
     *
     * @param taskId The Task ID of the {@code Activity} the mock objects are associated with.
     * @param profile The {@link Profile} the mock objects are associated with.
     * @param windowType The mock {@link BrowserWindowType} for this window.
     * @return The new {@link ChromeAndroidTask.ActivityScopedObjects} instance.
     */
    static ChromeAndroidTask.ActivityScopedObjects createMockActivityScopedObjects(
            int taskId, Profile profile, @BrowserWindowType int windowType) {
        var activityWindowAndroidMocks = createActivityWindowAndroidMocks(taskId);
        if (VERSION.SDK_INT >= VERSION_CODES.R) {
            mockDesktopWindowingMode(activityWindowAndroidMocks);
        }
        var activityWindowAndroid = activityWindowAndroidMocks.mMockActivityWindowAndroid;

        return createMockActivityScopedObjects(
                activityWindowAndroid, profile, windowType, SupportedProfileType.REGULAR);
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
        return createMockActivityScopedObjects(
                activityWindowAndroid,
                profile,
                BrowserWindowType.NORMAL,
                SupportedProfileType.REGULAR);
    }

    /**
     * Creates a {@link ChromeAndroidTask.ActivityScopedObjects} instance containing mock objects.
     *
     * @param activityWindowAndroid The {@link ActivityWindowAndroid} for the {@code Activity} the
     *     mock objects are associated with.
     * @param profile The {@link Profile} the mock objects are associated with.
     * @param windowType The mock {@link BrowserWindowType} for this window.
     * @param profileType The {@link SupportedProfileType} for this window.
     * @return The new {@link ChromeAndroidTask.ActivityScopedObjects} instance.
     */
    static ChromeAndroidTask.ActivityScopedObjects createMockActivityScopedObjects(
            ActivityWindowAndroid activityWindowAndroid,
            Profile profile,
            @BrowserWindowType int windowType,
            @SupportedProfileType int profileType) {
        assert mockingDetails(activityWindowAndroid).isMock();

        // TODO(http://crbug.com/454954191): Use the "MockTabModel" class.
        var mockTabModel = mock(TabModel.class);
        when(mockTabModel.getProfile()).thenReturn(profile);

        // Assume the regular tab model is always the current tab model.
        var mockTabModelSelector = mock(TabModelSelector.class);
        SettableMonotonicObservableSupplier<TabModel> tabModelSupplier =
                ObservableSuppliers.createMonotonic();
        tabModelSupplier.set(mockTabModel);
        when(mockTabModelSelector.getModel(false)).thenReturn(mockTabModel);
        when(mockTabModelSelector.getCurrentTabModelSupplier()).thenReturn(tabModelSupplier);
        when(mockTabModelSelector.getCurrentModel()).thenReturn(mockTabModel);

        // Even when we have a dedicated window per-profile both TabModels will still exist;
        // however, one will always be empty.
        var mockIncognitoTabModel = mock(IncognitoTabModel.class);
        when(mockTabModelSelector.getModel(true)).thenReturn(mockIncognitoTabModel);
        when(mockTabModelSelector.getModels())
                .thenReturn(List.of(mockTabModel, mockIncognitoTabModel));

        // Initially there is no incognito profile until an incognito tab is created.
        when(mockIncognitoTabModel.getProfile()).thenReturn(null);

        var mockDesktopWindowStateManager = mock(DesktopWindowStateManager.class);

        return new ChromeAndroidTask.ActivityScopedObjects(
                activityWindowAndroid,
                mockTabModelSelector,
                windowType,
                profileType,
                mockDesktopWindowStateManager);
    }

    /**
     * Creates an instance of {@link ActivityWindowAndroidMocks}.
     *
     * @param taskId Task ID for {@link ActivityWindowAndroidMocks#mFakeActivity}.
     */
    static ActivityWindowAndroidMocks createActivityWindowAndroidMocks(int taskId) {
        var mockActivityWindowAndroid = mock(ActivityWindowAndroid.class);
        var mockActivityLifecycleDispatcher = mock(ActivityLifecycleDispatcher.class);
        var mockWindow = mock(Window.class);
        var mockWindowManager = mock(WindowManager.class);
        var mockActivityManager = mock(ActivityManager.class);
        var mockDisplay = mock(DisplayAndroid.class);

        var fakeDecorView = new FakeDecorView(ContextUtils.getApplicationContext());
        var fakeActivity =
                new FakeActivity(
                        taskId,
                        mockWindow,
                        fakeDecorView,
                        mockWindowManager,
                        mockActivityManager,
                        mockActivityLifecycleDispatcher);

        when(mockDisplay.getDipScale()).thenReturn(1.0f);

        when(mockActivityWindowAndroid.getActivity()).thenReturn(new WeakReference<>(fakeActivity));
        when(mockActivityWindowAndroid.getDisplay()).thenReturn(mockDisplay);

        var mocks =
                new ActivityWindowAndroidMocks(
                        mockActivityWindowAndroid,
                        fakeActivity,
                        mockActivityLifecycleDispatcher,
                        mockDisplay,
                        fakeDecorView,
                        mockWindowManager,
                        mockWindow);
        sActivityWindowAndroidMocks.put(taskId, mocks);
        return mocks;
    }

    /**
     * Creates a mock {@link AndroidBrowserWindow.Natives} whose {@link
     * AndroidBrowserWindow.Natives#create} returns a fake {@link JniUniquePtr}. {@link
     * AndroidBrowserWindow.Natives#getLegacyNativePtr} reports the fake pointer as {@link
     * #FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR} or {@link
     * #FAKE_INCOGNITO_NATIVE_ANDROID_BROWSER_WINDOW_PTR}.
     *
     * <p>This method also sets the mock as the testing instance for {@link
     * AndroidBrowserWindowJni}.
     */
    public static AndroidBrowserWindow.Natives createMockAndroidBrowserWindowNatives() {
        ResettersForTesting.register(sFakeNativeAndroidBrowserWindowPtrs::clear);
        ResettersForTesting.register(sNativeAndroidBrowserWindowDestroyCounts::clear);

        var mockAndroidBrowserWindowNatives = mock(AndroidBrowserWindow.Natives.class);
        when(mockAndroidBrowserWindowNatives.create(
                        /* caller= */ any(),
                        /* browserWindowType= */ anyInt(),
                        /* profile= */ any()))
                .thenAnswer(
                        invocation -> {
                            Profile profile = invocation.getArgument(2);
                            long fakePtr =
                                    profile.isOffTheRecord()
                                            ? FAKE_INCOGNITO_NATIVE_ANDROID_BROWSER_WINDOW_PTR
                                            : FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR;
                            @SuppressWarnings("unchecked")
                            JniUniquePtr<NativeBrowserWindowInterface> ptr =
                                    mock(JniUniquePtr.class);
                            doAnswer(
                                            unused -> {
                                                sFakeNativeAndroidBrowserWindowPtrs.remove(ptr);
                                                sNativeAndroidBrowserWindowDestroyCounts.merge(
                                                        fakePtr, 1, Integer::sum);
                                                return null;
                                            })
                                    .when(ptr)
                                    .destroy();
                            sFakeNativeAndroidBrowserWindowPtrs.put(ptr, fakePtr);
                            return ptr;
                        });
        when(mockAndroidBrowserWindowNatives.getLegacyNativePtr(any()))
                .thenAnswer(
                        invocation ->
                                sFakeNativeAndroidBrowserWindowPtrs.getOrDefault(
                                        invocation.getArgument(0), 0L));

        AndroidBrowserWindowJni.setInstanceForTesting(mockAndroidBrowserWindowNatives);

        return mockAndroidBrowserWindowNatives;
    }

    /**
     * Returns how many times a pointer returned by the mock from {@link
     * #createMockAndroidBrowserWindowNatives()} with the given fake address was destroyed.
     */
    public static int getNativeAndroidBrowserWindowDestroyCount(long fakePtr) {
        return sNativeAndroidBrowserWindowDestroyCounts.getOrDefault(fakePtr, 0);
    }

    /**
     * Returns the fake address assigned to {@code ptr} by the mock from {@link
     * #createMockAndroidBrowserWindowNatives()}, or 0 if {@code ptr} is null or wasn't created by
     * that mock.
     */
    static long getFakeNativeAndroidBrowserWindowPtr(
            @Nullable JniPtr<NativeBrowserWindowInterface> ptr) {
        if (ptr == null) return 0;
        return sFakeNativeAndroidBrowserWindowPtrs.getOrDefault(ptr, 0L);
    }

    /** Mocks {@link PopupCreator}. */
    public static PopupCreator mockPopupCreator() {
        if (sPopupCreator != null) return sPopupCreator;
        sPopupCreator = mock(PopupCreator.class);
        when(sPopupCreator.createNewPopup(any(), anyBoolean(), any(), any())).thenReturn(true);
        when(sPopupCreator.createNewPopupFromWebContents(any(), any(), any(), any(), any()))
                .thenReturn(true);
        PopupCreatorFactory.setInstanceForTesting(sPopupCreator);
        ResettersForTesting.register(() -> sPopupCreator = null);
        return sPopupCreator;
    }

    /** Mocks {@link MultiInstanceOrchestrator}. */
    public static MultiInstanceOrchestrator mockMultiInstanceOrchestrator() {
        if (sMultiInstanceOrchestrator != null) return sMultiInstanceOrchestrator;
        sMultiInstanceOrchestrator = mock(MultiInstanceOrchestrator.class);
        when(sMultiInstanceOrchestrator.createNewWindow(
                        any(), anyBoolean(), any(), any(), anyInt()))
                .thenReturn(true);
        when(sMultiInstanceOrchestrator.createNewWindowFromWebContents(
                        any(), any(), any(), any(), any(), anyInt()))
                .thenReturn(true);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(sMultiInstanceOrchestrator);
        ResettersForTesting.register(() -> sMultiInstanceOrchestrator = null);
        return sMultiInstanceOrchestrator;
    }

    static PendingTaskInfo createPendingTaskInfo() {
        return createPendingTaskInfo(createMockAndroidBrowserWindowCreateParams());
    }

    static PendingTaskInfo createPendingTaskInfo(AndroidBrowserWindowCreateParams createParams) {
        JniOnceCallback<Long> mockCallback = mock();

        return new PendingTaskInfo(IdSequencer.next(), createParams, mockCallback);
    }

    /**
     * @see #createMockAndroidBrowserWindowCreateParams(int, Rect, int, Profile)
     */
    static AndroidBrowserWindowCreateParams createMockAndroidBrowserWindowCreateParams() {
        return createMockAndroidBrowserWindowCreateParams(
                BrowserWindowType.NORMAL, new Rect(), WindowShowState.DEFAULT);
    }

    /**
     * @see #createMockAndroidBrowserWindowCreateParams(int, Rect, int, Profile)
     */
    static AndroidBrowserWindowCreateParams createMockAndroidBrowserWindowCreateParams(
            @BrowserWindowType int windowType) {
        return createMockAndroidBrowserWindowCreateParams(
                windowType, new Rect(), WindowShowState.DEFAULT);
    }

    /**
     * @see #createMockAndroidBrowserWindowCreateParams(int, Rect, int, Profile)
     */
    static AndroidBrowserWindowCreateParams createMockAndroidBrowserWindowCreateParams(
            @BrowserWindowType int windowType,
            Rect launchBounds,
            @WindowShowState.EnumType int showState) {
        return createMockAndroidBrowserWindowCreateParams(
                windowType, launchBounds, showState, mock(Profile.class));
    }

    /**
     * Creates an {@link AndroidBrowserWindowCreateParams} mock.
     *
     * @param windowType The mock {@link BrowserWindowType} to set in the create params.
     * @param launchBounds The launch bounds to set in the create params.
     * @param showState The mock {@link WindowShowState} to set in the create params.
     * @param profile The {@link Profile} to set in the create params.
     * @return The {@link AndroidBrowserWindowCreateParams} mock.
     */
    static AndroidBrowserWindowCreateParams createMockAndroidBrowserWindowCreateParams(
            @BrowserWindowType int windowType,
            Rect launchBounds,
            @WindowShowState.EnumType int showState,
            Profile profile) {
        var mockParams = mock(AndroidBrowserWindowCreateParams.class);
        when(mockParams.getWindowType()).thenReturn(windowType);
        when(mockParams.getProfile()).thenReturn(profile);
        when(mockParams.getInitialBoundsInDp()).thenReturn(launchBounds);
        when(mockParams.getInitialShowState()).thenReturn(showState);
        when(mockParams.takeWebContents()).thenReturn(null);
        mockPopupCreator();
        mockMultiInstanceOrchestrator();

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
        var activity = activityWindowAndroidMocks.mFakeActivity;
        var mockWindowManager = activityWindowAndroidMocks.mMockWindowManager;

        // Activity should be in multi-window mode.
        // (Desktop windowing mode is a multi-window mode.)
        activity.setInMultiWindowModeForTesting(true);
        AppHeaderUtils.setAppInDesktopWindowForTesting(true);

        // Config system bars behavior.
        var mockWindow = activityWindowAndroidMocks.mMockWindow;
        var mockWindowInsetsController = mock(WindowInsetsController.class);
        when(mockWindowInsetsController.getSystemBarsBehavior())
                .thenReturn(WindowInsetsControllerCompat.BEHAVIOR_DEFAULT);
        when(mockWindow.getInsetsController()).thenReturn(mockWindowInsetsController);

        setDecorViewBounds(activityWindowAndroidMocks, currentWindowBoundsInPx);
        mockCurrentWindowMetrics(mockWindowManager, currentWindowBoundsInPx);
        mockMaxWindowMetrics(mockWindowManager, fullScreenWindowBoundsInPx, maxTappableInsetsInPx);
    }

    /**
     * Sets the decor {@link View} bounds for the provided {@link ActivityWindowAndroidMocks}, based
     * on the given {@code windowBoundsInPx}.
     *
     * @return The decor {@link View}.
     */
    static View setDecorViewBounds(
            ActivityWindowAndroidMocks activityWindowAndroidMocks, Rect windowBoundsInPx) {
        View decorView = activityWindowAndroidMocks.mDecorView;
        decorView.setLeft(windowBoundsInPx.left);
        decorView.setTop(windowBoundsInPx.top);
        decorView.setRight(windowBoundsInPx.right);
        decorView.setBottom(windowBoundsInPx.bottom);
        return decorView;
    }

    @RequiresApi(api = VERSION_CODES.R)
    static void mockCurrentWindowMetrics(
            WindowManager mockWindowManager, Rect currentWindowBoundsInPx) {
        assert mockingDetails(mockWindowManager).isMock();

        var currentWindowMetrics =
                new WindowMetrics(currentWindowBoundsInPx, new WindowInsets.Builder().build());

        when(mockWindowManager.getCurrentWindowMetrics()).thenReturn(currentWindowMetrics);
    }

    @RequiresApi(api = VERSION_CODES.R)
    static void mockMaxWindowMetrics(
            WindowManager mockWindowManager,
            Rect fullScreenWindowBoundsInPx,
            Insets maxTappableInsetsInPx) {
        assert mockingDetails(mockWindowManager).isMock();

        WindowInsets maxWindowInsets =
                new WindowInsets.Builder()
                        .setInsets(WindowInsets.Type.tappableElement(), maxTappableInsetsInPx)
                        .setVisible(WindowInsets.Type.statusBars(), true)
                        .build();
        var maxWindowMetrics = new WindowMetrics(fullScreenWindowBoundsInPx, maxWindowInsets);
        when(mockWindowManager.getMaximumWindowMetrics()).thenReturn(maxWindowMetrics);
    }
}
