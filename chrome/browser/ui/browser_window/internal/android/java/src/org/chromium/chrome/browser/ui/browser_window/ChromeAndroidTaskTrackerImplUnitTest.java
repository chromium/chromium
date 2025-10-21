// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTracker.EXTRA_PENDING_BROWSER_WINDOW_TASK_ID;

import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.browser_window.PendingActionManager.PendingAction;
import org.chromium.ui.mojom.WindowShowState;

/** Unit tests for {@link ChromeAndroidTaskTrackerImpl}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeAndroidTaskTrackerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    private Context mContext;

    private final ChromeAndroidTaskTrackerImpl mChromeAndroidTaskTracker =
            ChromeAndroidTaskTrackerImpl.getInstance();

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mContext = spy(ApplicationProvider.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @After
    public void tearDown() {
        mChromeAndroidTaskTracker.removeAllForTesting();
    }

    @Test
    public void createPendingTask_createsAndStoresPendingTask() {
        // Arrange.
        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();

        // Act.
        var task = mChromeAndroidTaskTracker.createPendingTask(mockParams, null);

        // Assert.
        assertNotNull(task);
        assertNotNull(task.getPendingId());
        assertNotNull(mChromeAndroidTaskTracker.getPendingTaskForTesting(task.getPendingId()));
        assertEquals(1, mChromeAndroidTaskTracker.getAllNativeBrowserWindowPtrs().length);
        assertNull(task.getId());
        assertEquals(mockParams.getProfile(), task.getProfile());
        assertEquals(mockParams.getWindowType(), task.getBrowserWindowType());
    }

    @Test
    public void createPendingTask_requestsUnsupportedShowState_throwsException() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, new Rect(), WindowShowState.INACTIVE);

        // Act and Assert.
        assertThrows(
                UnsupportedOperationException.class,
                () -> mChromeAndroidTaskTracker.createPendingTask(mockParams, null));
    }

    @Test
    public void createPendingTask_requestsUnsupportedWindowType_throwsException() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.APP_POPUP, new Rect(), WindowShowState.DEFAULT);

        // Act and Assert.
        assertThrows(
                UnsupportedOperationException.class,
                () -> mChromeAndroidTaskTracker.createPendingTask(mockParams, null));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void createPendingTask_requestsMaximizedShowState_createsPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, new Rect(), WindowShowState.MAXIMIZED);

        // Act.
        var task =
                (ChromeAndroidTaskImpl)
                        mChromeAndroidTaskTracker.createPendingTask(mockParams, null);

        // Assert.
        var pendingActionManager = task.getPendingActionManagerForTesting();
        assertTrue(pendingActionManager.isActionRequested(PendingAction.MAXIMIZE));
        assertEquals(PendingAction.MAXIMIZE, pendingActionManager.getPendingActionsForTesting()[0]);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void createPendingTask_requestsMinimizedShowState_createsPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, new Rect(), WindowShowState.MINIMIZED);

        // Act.
        var task =
                (ChromeAndroidTaskImpl)
                        mChromeAndroidTaskTracker.createPendingTask(mockParams, null);

        // Assert.
        var pendingActionManager = task.getPendingActionManagerForTesting();
        assertTrue(pendingActionManager.isActionRequested(PendingAction.MINIMIZE));
        assertEquals(PendingAction.MINIMIZE, pendingActionManager.getPendingActionsForTesting()[0]);
    }

    @Test
    public void createPendingTask_startsActivityWithIntent() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();

        // Act.
        mChromeAndroidTaskTracker.createPendingTask(mockParams, null);

        // Assert.
        var intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).startActivity(intentCaptor.capture());
        assertNotNull(intentCaptor.getValue().getComponent());
        assertEquals(
                "org.chromium.chrome.browser.ChromeTabbedActivity",
                intentCaptor.getValue().getComponent().getClassName());
        assertTrue(intentCaptor.getValue().hasExtra(EXTRA_PENDING_BROWSER_WINDOW_TASK_ID));
        assertTrue((intentCaptor.getValue().getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0);
        assertTrue((intentCaptor.getValue().getFlags() & Intent.FLAG_ACTIVITY_MULTIPLE_TASK) != 0);
    }

    @Test
    public void createPendingTask_setsLaunchBounds() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL,
                        new Rect(10, 20, 800, 600),
                        WindowShowState.DEFAULT);

        // Act.
        mChromeAndroidTaskTracker.createPendingTask(mockParams, null);

        // Assert.
        var bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mContext).startActivity(any(Intent.class), bundleCaptor.capture());
        Rect capturedBounds =
                bundleCaptor.getValue().getParcelable(ActivityOptions.KEY_LAUNCH_BOUNDS);
        assertEquals(mockParams.getInitialBounds(), capturedBounds);
    }

    @Test
    public void obtainTask_withPendingId_adoptsPendingTask() {
        // Arrange.
        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();

        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var pendingTask = mChromeAndroidTaskTracker.createPendingTask(mockParams, null);
        int pendingId = assertNonNull(pendingTask.getPendingId());

        int taskId = 123;
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));

        // Act.
        var task =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel, pendingId);

        // Assert.
        assertNull(mChromeAndroidTaskTracker.getPendingTaskForTesting(pendingId));
        assertEquals(1, mChromeAndroidTaskTracker.getAllNativeBrowserWindowPtrs().length);
        assertEquals(activityWindowAndroid, task.getActivityWindowAndroid());
        assertEquals("The pending task should be adopted.", pendingTask, task);
        assertEquals("Task ID should be updated.", taskId, (int) assertNonNull(task.getId()));
        assertNull("Pending ID should be cleared.", task.getPendingId());
    }

    @Test
    public void obtainTask_withPendingIdAndCallback_adoptsAndInvokesCallback() {
        // Arrange.
        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();

        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        JniOnceCallback<Long> mockCallback = mock();
        var pendingTask = mChromeAndroidTaskTracker.createPendingTask(mockParams, mockCallback);
        int pendingId = assertNonNull(pendingTask.getPendingId());

        int taskId = 123;
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));

        // Act.
        var task =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel, pendingId);

        // Assert.
        assertEquals("The pending task should be adopted.", pendingTask, task);
        var ptrCaptor = ArgumentCaptor.forClass(Long.class);
        verify(mockCallback).onResult(ptrCaptor.capture());
        assertEquals(
                ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR,
                (long) ptrCaptor.getValue());
    }

    @Test
    public void obtainTask_withInvalidPendingId_throwsException() {
        // Arrange.
        int taskId = 1;
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        int invalidPendingId = 12345;

        // Act and Assert.
        assertThrows(
                AssertionError.class,
                () ->
                        mChromeAndroidTaskTracker.obtainTask(
                                BrowserWindowType.NORMAL,
                                activityWindowAndroid,
                                tabModel,
                                invalidPendingId));
    }

    @Test
    public void obtainTask_activityWindowAndroidBelongsToNewTask_createsNewTask() {
        // Arrange.
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);

        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel, null);

        // Assert.
        assertEquals(1, (int) assertNonNull(chromeAndroidTask.getId()));
        assertEquals(activityWindowAndroid, chromeAndroidTask.getActivityWindowAndroid());
    }

    @Test
    public void
            obtainTask_activityWindowAndroidBelongsToExistingTask_sameBrowserWindowType_reusesExistingTask() {
        // Arrange.
        // (1) Create a new task.
        int taskId = 1;
        var activityWindowAndroid1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        var chromeAndroidTask1 =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel, null);

        // (2) Clear the ActivityWindowAndroid from the task.
        // This simulates the case where ChromeActivity is killed in the background, but the Task
        // (window) is still alive.
        chromeAndroidTask1.clearActivityWindowAndroid();

        // (3) Create another ActivityWindowAndroid that belongs to the same Task.
        // This can happen when ChromeActivity is recreated, e.g. after ChromeActivity is killed by
        // OS in the background, and the user later brings it back to foreground.
        var activityWindowAndroid2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);

        // Act.
        var chromeAndroidTask2 =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid2, tabModel, null);

        // Assert.
        assertEquals(chromeAndroidTask1, chromeAndroidTask2);
        assertEquals(taskId, (int) assertNonNull(chromeAndroidTask2.getId()));
        assertEquals(activityWindowAndroid2, chromeAndroidTask2.getActivityWindowAndroid());
    }

    @Test
    public void
            obtainTask_activityWindowAndroidBelongsToExistingTask_differentBrowserWindowType_throwsException() {
        // Arrange.
        // (1) Create a new task.
        int taskId = 1;
        var activityWindowAndroid1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel, null);

        // (2) Clear the ActivityWindowAndroid from the task.
        // This simulates the case where ChromeActivity is killed in the background, but the Task
        // (window) is still alive.
        chromeAndroidTask.clearActivityWindowAndroid();

        // (3) Create another ActivityWindowAndroid that belongs to the same Task.
        // This can happen when ChromeActivity is recreated, e.g. after ChromeActivity is killed by
        // OS in the background, and the user later brings it back to foreground.
        var activityWindowAndroid2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);

        // Act & Assert.
        // Note that we use a different browser window type here.
        assertThrows(
                AssertionError.class,
                () -> {
                    mChromeAndroidTaskTracker.obtainTask(
                            BrowserWindowType.POPUP, activityWindowAndroid2, tabModel, null);
                });
    }

    @Test
    public void get_taskExists_returnsTask() {
        // Arrange.
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel, null);

        // Act & Assert.
        assertEquals(chromeAndroidTask, mChromeAndroidTaskTracker.get(/* taskId= */ 1));
    }

    @Test
    public void get_taskDoesNotExist_returnsNull() {
        // Arrange.
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        mChromeAndroidTaskTracker.obtainTask(
                BrowserWindowType.NORMAL, activityWindowAndroid, tabModel, null);

        // Act & Assert.
        assertEquals(null, mChromeAndroidTaskTracker.get(/* taskId= */ 2));
    }

    @Test
    public void onActivityWindowAndroidDestroy_activityWindowAndroidHasDifferentTaskId_noOp() {
        // Arrange.
        var activityWindowAndroid1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var activityWindowAndroid2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 2);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel, null);

        // Act.
        mChromeAndroidTaskTracker.onActivityWindowAndroidDestroy(activityWindowAndroid2);

        // Assert.
        assertEquals(activityWindowAndroid1, chromeAndroidTask.getActivityWindowAndroid());
        assertFalse(chromeAndroidTask.isDestroyed());
        assertEquals(chromeAndroidTask, mChromeAndroidTaskTracker.get(/* taskId= */ 1));
    }

    @Test
    public void
            onActivityWindowAndroidDestroy_activityWindowAndroidHasSameTaskIdButIsDifferentInstance_noOp() {
        // Arrange.
        var activityWindowAndroid1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var activityWindowAndroid2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel, null);

        // Act.
        mChromeAndroidTaskTracker.onActivityWindowAndroidDestroy(activityWindowAndroid2);

        // Assert.
        assertEquals(activityWindowAndroid1, chromeAndroidTask.getActivityWindowAndroid());
        assertFalse(chromeAndroidTask.isDestroyed());
        assertEquals(chromeAndroidTask, mChromeAndroidTaskTracker.get(/* taskId= */ 1));
    }

    @Test
    public void
            onActivityWindowAndroidDestroy_activityWindowAndroidIsSameInstance_destroysAndRemovesTask() {
        // Arrange.
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel, null);

        // Act.
        mChromeAndroidTaskTracker.onActivityWindowAndroidDestroy(activityWindowAndroid);

        // Assert.
        assertNull(
                ((ChromeAndroidTaskImpl) chromeAndroidTask).getActivityWindowAndroidForTesting());
        assertTrue(chromeAndroidTask.isDestroyed());
        assertNull(mChromeAndroidTaskTracker.get(/* taskId= */ 1));
    }

    @Test
    public void remove_taskExists_destroysAndRemovesTask() {
        // Arrange.
        int taskId = 1;
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel, null);

        // Act.
        mChromeAndroidTaskTracker.remove(taskId);

        // Assert.
        assertTrue(chromeAndroidTask.isDestroyed());
        assertEquals(null, mChromeAndroidTaskTracker.get(taskId));
    }

    @Test
    public void remove_taskDoesNotExist_noOp() {
        // Arrange.
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel, null);

        // Act.
        mChromeAndroidTaskTracker.remove(/* taskId= */ 2);

        // Assert.
        assertFalse(chromeAndroidTask.isDestroyed());
        assertEquals(chromeAndroidTask, mChromeAndroidTaskTracker.get(/* taskId= */ 1));
    }

    @Test
    public void obtainAndRemoveTask_observerNotified() {
        // Arrange.
        var observer = mock(ChromeAndroidTaskTrackerObserver.class);
        mChromeAndroidTaskTracker.addObserver(observer);

        // Act (add task).
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel, null);

        // Assert (add task).
        verify(observer).onTaskAdded(chromeAndroidTask);

        // Act (remove task).
        mChromeAndroidTaskTracker.remove(assertNonNull(chromeAndroidTask.getId()));

        // Assert (remove task).
        verify(observer).onTaskRemoved(chromeAndroidTask);

        // Cleanup.
        mChromeAndroidTaskTracker.removeObserver(observer);
    }

    @Test
    public void activatePenultimatelyActivatedTask_oneTask_doesNothing() {
        // Arrange.
        var activityWindowAndroid1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));

        var task1 =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel, null);
        ((TopResumedActivityChangedWithNativeObserver) task1)
                .onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        ((TopResumedActivityChangedWithNativeObserver) task1)
                .onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ false);
        assertNotNull(activityWindowAndroid1.getActivity().get());
        var activityManager =
                (ActivityManager)
                        activityWindowAndroid1
                                .getActivity()
                                .get()
                                .getSystemService(Context.ACTIVITY_SERVICE);

        // Act.
        mChromeAndroidTaskTracker.activatePenultimatelyActivatedTask();

        // Assert.
        verify(activityManager, never()).moveTaskToFront(anyInt(), anyInt());
    }

    @Test
    public void activatePenultimatelyActivatedTask_twoTasks_activatesSecondToLast() {
        // Arrange.
        var activityWindowAndroid1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        var task1 =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel, null);
        ((TopResumedActivityChangedWithNativeObserver) task1)
                .onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        assertNotNull(activityWindowAndroid1.getActivity().get());
        var activityManager1 =
                (ActivityManager)
                        activityWindowAndroid1
                                .getActivity()
                                .get()
                                .getSystemService(Context.ACTIVITY_SERVICE);
        mFakeTime.advanceMillis(1);
        var activityWindowAndroid2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 2);
        var task2 =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityWindowAndroid2, tabModel, null);
        // Switch the order of activation.
        ((TopResumedActivityChangedWithNativeObserver) task1)
                .onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ false);
        ((TopResumedActivityChangedWithNativeObserver) task2)
                .onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        assertNotNull(activityWindowAndroid2.getActivity().get());
        var activityManager2 =
                (ActivityManager)
                        activityWindowAndroid2
                                .getActivity()
                                .get()
                                .getSystemService(Context.ACTIVITY_SERVICE);

        long task1LastActivatedTime = task1.getLastActivatedTimeMillis();
        long task2LastActivatedTime = task2.getLastActivatedTimeMillis();
        assertTrue(
                "Task 2 should have a later activation time",
                task2LastActivatedTime > task1LastActivatedTime);

        // Act.
        mChromeAndroidTaskTracker.activatePenultimatelyActivatedTask();

        // Assert.
        // task2 was the last activated, so task1 (the second to last) should be activated.
        verify(activityManager2, never()).moveTaskToFront(eq(task2.getId()), anyInt());
        verify(activityManager1).moveTaskToFront(eq(task1.getId()), anyInt());
    }

    @Test
    public void obtainTask_fromPendingState_dispatchesPendingShowInactive() {
        doTestDispatchPendingShowInactiveOrDeactivate(PendingAction.SHOW_INACTIVE);
    }

    @Test
    public void obtainTask_fromPendingState_dispatchesPendingDeactivate() {
        doTestDispatchPendingShowInactiveOrDeactivate(PendingAction.DEACTIVATE);
    }

    private void doTestDispatchPendingShowInactiveOrDeactivate(@PendingAction int action) {
        assert action == PendingAction.SHOW_INACTIVE || action == PendingAction.DEACTIVATE;
        // Arrange: Create live task and make it the top resumed task.
        int initialTopResumedTaskId = 0;
        var initialTopResumedTask =
                obtainTaskWithMockDeps(initialTopResumedTaskId, /* pendingId= */ null);
        initialTopResumedTask.onTopResumedActivityChangedWithNative(true);
        var mockWindowAndroid =
                assumeNonNull(initialTopResumedTask.getActivityWindowAndroidForTesting());
        var mockActivity = assumeNonNull(mockWindowAndroid.getActivity().get());
        var mockActivityManager =
                (ActivityManager) mockActivity.getSystemService(Context.ACTIVITY_SERVICE);
        // Arrange: Create pending task.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var pendingTask = mChromeAndroidTaskTracker.createPendingTask(mockParams, null);
        // Arrange: Request SHOW_INACTIVE or DEACTIVATE on the pending task.
        if (action == PendingAction.SHOW_INACTIVE) {
            pendingTask.showInactive();
        } else {
            pendingTask.deactivate();
        }

        // Act: Simulate newly created activity gains focus.
        mFakeTime.advanceMillis(100);
        var newTask = obtainTaskWithMockDeps(/* taskId= */ 2, pendingTask.getPendingId());
        newTask.onTopResumedActivityChangedWithNative(true);

        // Assert: Penultimately activated task gets activated.
        verify(mockActivityManager).moveTaskToFront(initialTopResumedTaskId, 0);
    }

    private ChromeAndroidTaskImpl obtainTaskWithMockDeps(int taskId, @Nullable Integer pendingId) {
        var mockActivityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var mockProfile = mock(Profile.class);
        var mockTabModel = mock(TabModel.class);
        when(mockTabModel.getProfile()).thenReturn(mockProfile);
        return (ChromeAndroidTaskImpl)
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL,
                        mockActivityWindowAndroid,
                        mockTabModel,
                        pendingId);
    }
}
