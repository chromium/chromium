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
import org.chromium.chrome.browser.ui.browser_window.PendingActionManager.PendingAction;
import org.chromium.ui.display.DisplayAndroid;
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

        DisplayAndroid mockDisplay = mock(DisplayAndroid.class);
        when(mockDisplay.getDipScale()).thenReturn(1.0f);
        DisplayAndroid.setNonMultiDisplayForTesting(mockDisplay);

        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();
    }

    @After
    public void tearDown() {
        mChromeAndroidTaskTracker.removeAllForTesting();
    }

    @Test
    public void
            createPendingTask_normalType_noExistingTask_returnsNullAndInvokesCallbackWithNullPtrValue() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL);
        JniOnceCallback<Long> mockCallback = mock();

        // Act.
        var pendingTask = mChromeAndroidTaskTracker.createPendingTask(mockParams, mockCallback);

        // Assert.
        assertNull(pendingTask);
        verify(mockCallback).onResult(0L);
    }

    @Test
    public void createPendingTask_createsAndStoresPendingTask() {
        // Arrange & Act.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL);
        var task = createPendingTaskWithExistingTask(mockParams);

        // Assert.
        assertNotNull(task);
        assertNotNull(task.getPendingTaskInfo());
        assertNotNull(
                mChromeAndroidTaskTracker.getPendingTaskForTesting(
                        task.getPendingTaskInfo().mPendingTaskId));
        assertEquals(
                // Creating a pending Task of "NORMAL" type requires an existing ChromeAndroidTask.
                // Therefore, getAllNativeBrowserWindowPtrs().length should be 2:
                // one pointer is the existing Task and the other is the pending Task.
                2, mChromeAndroidTaskTracker.getAllNativeBrowserWindowPtrs().length);
        assertNull(task.getId());
        assertEquals(mockParams.getWindowType(), task.getBrowserWindowType());
    }

    @Test
    public void createPendingTask_popupType_createsAndStoresPendingTask() {
        // Arrange & Act.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.POPUP);
        var task = createPendingTaskWithExistingTask(mockParams);

        // Assert.
        assertNotNull(task);
        assertNotNull(task.getPendingTaskInfo());
        assertNotNull(
                mChromeAndroidTaskTracker.getPendingTaskForTesting(
                        task.getPendingTaskInfo().mPendingTaskId));
        assertEquals(
                // Creating a pending Task of "POPUP" type requires an existing ChromeAndroidTask.
                // Therefore, getAllNativeBrowserWindowPtrs().length should be 2:
                // one pointer is the existing Task and the other is the pending Task.
                2, mChromeAndroidTaskTracker.getAllNativeBrowserWindowPtrs().length);
        assertNull(task.getId());
        assertEquals(mockParams.getWindowType(), task.getBrowserWindowType());

        var intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).startActivity(intentCaptor.capture());
        Intent intent = intentCaptor.getValue();
        assertNotNull(intent);
        assertTrue(intent.hasExtra(EXTRA_PENDING_BROWSER_WINDOW_TASK_ID));
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
                () -> createPendingTaskWithExistingTask(mockParams));
    }

    @Test
    public void createPendingTask_requestsUnsupportedWindowType_throwsException() {
        // Arrange.
        AndroidBrowserWindowCreateParams mockCreateParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.APP, new Rect(), WindowShowState.DEFAULT);

        // Act and Assert.
        assertThrows(
                UnsupportedOperationException.class,
                () -> createPendingTaskWithExistingTask(mockCreateParams));
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
                        assertNonNull(createPendingTaskWithExistingTask(mockParams));

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
                        assertNonNull(createPendingTaskWithExistingTask(mockParams));

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
        ChromeAndroidTask pendingTask = createPendingTaskWithExistingTask(mockParams);

        // Assert.
        assertNotNull(pendingTask);

        var intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).startActivity(intentCaptor.capture());
        assertTrue(intentCaptor.getValue().hasExtra(EXTRA_PENDING_BROWSER_WINDOW_TASK_ID));
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
        createPendingTaskWithExistingTask(mockParams);

        // Assert.
        var bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mContext).startActivity(any(Intent.class), bundleCaptor.capture());
        Rect capturedBounds =
                bundleCaptor.getValue().getParcelable(ActivityOptions.KEY_LAUNCH_BOUNDS);
        assertEquals(mockParams.getInitialBoundsInDp(), capturedBounds);
    }

    @Test
    public void createPendingTask_convertsDpToPx() {
        // Arrange.
        float dipScale = 2.0f;
        DisplayAndroid mockDisplay = mock(DisplayAndroid.class);
        when(mockDisplay.getDipScale()).thenReturn(dipScale);
        DisplayAndroid.setNonMultiDisplayForTesting(mockDisplay);

        Rect boundsInDp = new Rect(10, 20, 800, 600);
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        org.chromium.chrome.browser.ui.browser_window.BrowserWindowType.NORMAL,
                        boundsInDp,
                        WindowShowState.DEFAULT);

        // Act.
        createPendingTaskWithExistingTask(mockParams);

        // Assert.
        var bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mContext).startActivity(any(Intent.class), bundleCaptor.capture());
        Rect capturedBounds =
                bundleCaptor.getValue().getParcelable(ActivityOptions.KEY_LAUNCH_BOUNDS);

        Rect expectedBoundsInPx =
                new Rect(
                        (int) (boundsInDp.left * dipScale),
                        (int) (boundsInDp.top * dipScale),
                        (int) (boundsInDp.right * dipScale),
                        (int) (boundsInDp.bottom * dipScale));
        assertEquals(
                "Bounds should be converted from Dp to Px.", expectedBoundsInPx, capturedBounds);
    }

    @Test
    public void obtainTask_withPendingId_adoptsPendingTask() {
        // No tasks yet.
        assertEquals(0, mChromeAndroidTaskTracker.getAllNativeBrowserWindowPtrs().length);

        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL);
        var pendingTask =
                (ChromeAndroidTaskImpl)
                        assertNonNull(createPendingTaskWithExistingTask(mockParams));
        int pendingId = assertNonNull(pendingTask.getPendingTaskInfo()).mPendingTaskId;
        assertEquals(
                // Creating a pending Task of "NORMAL" type requires an existing ChromeAndroidTask.
                // Therefore, getAllNativeBrowserWindowPtrs().length should be 2:
                // one pointer is the existing Task and the other is the pending Task.
                2, mChromeAndroidTaskTracker.getAllNativeBrowserWindowPtrs().length);

        int taskId = IdSequencer.next();
        var newActivityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(
                        taskId, mockParams.getProfile());

        // Act.
        var task =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, newActivityScopedObjects, pendingId);
        pendingTask.onTopResumedActivityChangedWithNative(true);
        pendingTask.onActivityTopResumedChanged(true);

        // Assert.
        assertNull(mChromeAndroidTaskTracker.getPendingTaskForTesting(pendingId));
        assertEquals(
                // As above, only two native browser window pointers are expected despite the new
                // ActivityScopedObjects.
                2, mChromeAndroidTaskTracker.getAllNativeBrowserWindowPtrs().length);
        assertEquals(
                newActivityScopedObjects.mActivityWindowAndroid,
                task.getTopActivityWindowAndroid());
        assertEquals("The pending task should be adopted.", pendingTask, task);
        assertEquals("Task ID should be updated.", taskId, (int) assertNonNull(task.getId()));
        assertNull("PendingTaskInfo should be cleared.", task.getPendingTaskInfo());
    }

    @Test
    public void obtainTask_withPendingIdAndCallback_adoptsAndInvokesCallback() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        JniOnceCallback<Long> mockCallback = mock();
        var pendingTask =
                assertNonNull(createPendingTaskWithExistingTask(mockParams, mockCallback));
        int pendingId = assertNonNull(pendingTask.getPendingTaskInfo()).mPendingTaskId;

        int taskId = IdSequencer.next();
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);

        // Act.
        var task =
                (ChromeAndroidTaskImpl)
                        mChromeAndroidTaskTracker.obtainTask(
                                BrowserWindowType.NORMAL, activityScopedObjects, pendingId);
        task.onTopResumedActivityChangedWithNative(true);
        task.onActivityTopResumedChanged(true);

        // Assert.
        assertEquals("The pending task should be adopted.", pendingTask, task);
        verify(mockCallback)
                .onResult(ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }

    @Test
    public void obtainTask_withInvalidPendingId_throwsException() {
        // Arrange.
        int taskId = 1;
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        int invalidPendingId = 12345;

        // Act and Assert.
        assertThrows(
                AssertionError.class,
                () ->
                        mChromeAndroidTaskTracker.obtainTask(
                                BrowserWindowType.NORMAL, activityScopedObjects, invalidPendingId));
    }

    @Test
    public void obtainTask_activityScopedObjectsBelongToNewTask_createsNewTask() {
        // Arrange.
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 1);
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);

        // Assert.
        assertEquals(1, (int) assertNonNull(chromeAndroidTask.getId()));
        assertEquals(
                activityScopedObjects.mActivityWindowAndroid,
                chromeAndroidTask.getTopActivityWindowAndroid());
    }

    @Test
    public void
            obtainTask_activityScopedObjectsBelongToExistingTask_sameBrowserWindowType_addActivityScopedObjecsToExistingTask() {
        // Arrange: Create a new task.
        int taskId = 1;
        var activityScopedObjects1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        var chromeAndroidTask1 =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects1, /* pendingId= */ null);

        // Arrange: Create another ActivityScopedObjects that belongs to the same Task.
        var activityScopedObjects2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);

        // Act.
        var chromeAndroidTask2 =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects2, /* pendingId= */ null);

        // Assert.
        assertEquals(chromeAndroidTask1, chromeAndroidTask2);
        assertEquals(
                activityScopedObjects2.mActivityWindowAndroid,
                chromeAndroidTask2.getTopActivityWindowAndroid());
    }

    @Test
    public void
            obtainTask_activityScopedObjectsBelongToExistingTask_differentBrowserWindowType_throwsException() {
        // Arrange.
        // (1) Create a new task.
        int taskId = 1;
        var activityScopedObjects1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        mChromeAndroidTaskTracker.obtainTask(
                BrowserWindowType.NORMAL, activityScopedObjects1, /* pendingId= */ null);

        // (2) Create another ActivityScopedObjects that belongs to the same Task.
        var activityScopedObjects2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);

        // Act & Assert.
        // Note that we use a different browser window type here.
        assertThrows(
                AssertionError.class,
                () ->
                        mChromeAndroidTaskTracker.obtainTask(
                                BrowserWindowType.POPUP,
                                activityScopedObjects2,
                                /* pendingId= */ null));
    }

    @Test
    public void get_taskExists_returnsTask() {
        // Arrange.
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 1);
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);

        // Act & Assert.
        assertEquals(chromeAndroidTask, mChromeAndroidTaskTracker.get(/* taskId= */ 1));
    }

    @Test
    public void get_taskDoesNotExist_returnsNull() {
        // Arrange.
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 1);
        mChromeAndroidTaskTracker.obtainTask(
                BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);

        // Act & Assert.
        assertNull(mChromeAndroidTaskTracker.get(/* taskId= */ 2));
    }

    @Test
    public void onActivityWindowAndroidDestroy_activityWindowAndroidHasDifferentTaskId_noOp() {
        // Arrange.
        var activityScopedObjects1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 1);
        var activityScopedObjects2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 2);
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects1, /* pendingId= */ null);

        // Act.
        mChromeAndroidTaskTracker.onActivityWindowAndroidDestroy(
                activityScopedObjects2.mActivityWindowAndroid);

        // Assert.
        assertEquals(
                activityScopedObjects1.mActivityWindowAndroid,
                chromeAndroidTask.getTopActivityWindowAndroid());
        assertFalse(chromeAndroidTask.isDestroyed());
        assertEquals(chromeAndroidTask, mChromeAndroidTaskTracker.get(/* taskId= */ 1));
    }

    @Test
    public void
            onActivityWindowAndroidDestroy_activityWindowAndroidHasSameTaskIdButIsNotTracked_noOp() {
        // Arrange: Note that activityScopedObjects2 isn't tracked by chromeAndroidTask.
        int taskId = 1;
        var activityScopedObjects1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        var activityScopedObjects2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects1, /* pendingId= */ null);

        // Act.
        mChromeAndroidTaskTracker.onActivityWindowAndroidDestroy(
                activityScopedObjects2.mActivityWindowAndroid);

        // Assert.
        assertEquals(
                activityScopedObjects1.mActivityWindowAndroid,
                chromeAndroidTask.getTopActivityWindowAndroid());
        assertFalse(chromeAndroidTask.isDestroyed());
        assertEquals(chromeAndroidTask, mChromeAndroidTaskTracker.get(taskId));
    }

    @Test
    public void
            onActivityWindowAndroidDestroy_activityWindowAndroidHasSameTaskId_taskHasOtherActivity_doesNotDestroyTask() {
        // Arrange.
        int taskId = 1;
        var activityScopedObjects1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        var activityScopedObjects2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects1, /* pendingId= */ null);
        var chromeAndroidTask2 =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects2, /* pendingId= */ null);
        assertEquals(chromeAndroidTask, chromeAndroidTask2);
        assertEquals(
                activityScopedObjects2.mActivityWindowAndroid,
                chromeAndroidTask.getTopActivityWindowAndroid());

        // Act.
        mChromeAndroidTaskTracker.onActivityWindowAndroidDestroy(
                activityScopedObjects2.mActivityWindowAndroid);

        // Assert.
        assertEquals(
                activityScopedObjects1.mActivityWindowAndroid,
                chromeAndroidTask.getTopActivityWindowAndroid());
        assertFalse(chromeAndroidTask.isDestroyed());
        assertEquals(chromeAndroidTask, mChromeAndroidTaskTracker.get(taskId));
    }

    @Test
    public void
            onActivityWindowAndroidDestroy_activityWindowAndroidHasSameTaskId_taskHasNoOtherActivity_destroysTask() {
        // Arrange.
        int taskId = 1;
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);

        // Act.
        mChromeAndroidTaskTracker.onActivityWindowAndroidDestroy(
                activityScopedObjects.mActivityWindowAndroid);

        // Assert.
        assertTrue(chromeAndroidTask.isDestroyed());
        assertNull(mChromeAndroidTaskTracker.get(taskId));
    }

    @Test
    public void remove_taskExists_destroysAndRemovesTask() {
        // Arrange.
        int taskId = 1;
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);

        // Act.
        mChromeAndroidTaskTracker.remove(taskId);

        // Assert.
        assertTrue(chromeAndroidTask.isDestroyed());
        assertNull(mChromeAndroidTaskTracker.get(taskId));
    }

    @Test
    public void remove_taskDoesNotExist_noOp() {
        // Arrange.
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 1);
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);

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

        // Act (add new task).
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 1);
        var chromeAndroidTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);

        // Assert (add new task).
        verify(observer).onTaskAdded(chromeAndroidTask);

        // Act (add pending task).
        var pendingTask =
                mChromeAndroidTaskTracker.createPendingTask(
                        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                                BrowserWindowType.NORMAL,
                                new Rect(10, 20, 800, 600),
                                WindowShowState.DEFAULT),
                        /* callback= */ null);
        assertNotNull(pendingTask);
        var chromeAndroidPendingTask =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects, pendingTask.getId());

        // Assert (add pending task).
        verify(observer).onTaskAdded(chromeAndroidPendingTask);

        // Act (remove pending task).
        mChromeAndroidTaskTracker.remove(assertNonNull(chromeAndroidPendingTask.getId()));

        // Assert (remove pending task).
        verify(observer).onTaskRemoved(chromeAndroidPendingTask);

        // Act (remove new task).
        mChromeAndroidTaskTracker.remove(assertNonNull(chromeAndroidTask.getId()));

        // Assert (remove new task).
        verify(observer).onTaskRemoved(chromeAndroidTask);

        // Cleanup.
        mChromeAndroidTaskTracker.removeObserver(observer);
    }

    @Test
    public void activatePenultimatelyActivatedTask_oneTask_doesNothing() {
        // Arrange.
        var activityScopedObjects1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 1);
        var task1 =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects1, /* pendingId= */ null);
        ((TopResumedActivityChangedWithNativeObserver) task1)
                .onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        ((TopResumedActivityChangedWithNativeObserver) task1)
                .onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ false);
        assertNotNull(activityScopedObjects1.mActivityWindowAndroid.getActivity().get());
        var activityManager =
                (ActivityManager)
                        activityScopedObjects1
                                .mActivityWindowAndroid
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
        var activityScopedObjects1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 1);
        var task1 =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects1, /* pendingId= */ null);
        ((TopResumedActivityChangedWithNativeObserver) task1)
                .onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        assertNotNull(activityScopedObjects1.mActivityWindowAndroid.getActivity().get());
        var activityManager1 =
                (ActivityManager)
                        activityScopedObjects1
                                .mActivityWindowAndroid
                                .getActivity()
                                .get()
                                .getSystemService(Context.ACTIVITY_SERVICE);
        mFakeTime.advanceMillis(1);
        var activityScopedObjects2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 2);
        var task2 =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects2, /* pendingId= */ null);
        // Switch the order of activation.
        ((TopResumedActivityChangedWithNativeObserver) task1)
                .onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ false);
        ((TopResumedActivityChangedWithNativeObserver) task2)
                .onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        assertNotNull(activityScopedObjects2.mActivityWindowAndroid.getActivity().get());
        var activityManager2 =
                (ActivityManager)
                        activityScopedObjects2
                                .mActivityWindowAndroid
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
    public void getNativeBrowserWindowPtrsOrderedByActivation_withPendingTask_doesNotCrash() {
        // Arrange.
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(
                        IdSequencer.next());
        mChromeAndroidTaskTracker.obtainTask(
                BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);

        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL);
        mChromeAndroidTaskTracker.createPendingTask(mockParams, null);

        // Act.
        long[] ptrs = mChromeAndroidTaskTracker.getNativeBrowserWindowPtrsOrderedByActivation();

        // Assert.
        assertEquals(
                "The pointer array should contain both the alive and the pending task.",
                2,
                ptrs.length);
    }

    @Test
    public void obtainTask_fromPendingState_dispatchesPendingShowInactive() {
        doTestDispatchPendingShowInactiveOrDeactivate(PendingAction.SHOW_INACTIVE);
    }

    @Test
    public void obtainTask_fromPendingState_dispatchesPendingDeactivate() {
        doTestDispatchPendingShowInactiveOrDeactivate(PendingAction.DEACTIVATE);
    }

    /**
     * @see #createPendingTaskWithExistingTask(AndroidBrowserWindowCreateParams, JniOnceCallback)
     */
    private @Nullable ChromeAndroidTask createPendingTaskWithExistingTask(
            AndroidBrowserWindowCreateParams createParams) {
        return createPendingTaskWithExistingTask(
                createParams, /* taskCreationCallbackForNative= */ null);
    }

    /**
     * Creates an existing {@link ChromeAndroidTask} first, then creates a pending task.
     *
     * <p>This method is useful for cases where a pending {@link ChromeAndroidTask} requires an
     * existing Task. For example, a pending Task of "NORMAL" type requires the {@code
     * MultiInstanceManager} associated with an existing Task.
     */
    private @Nullable ChromeAndroidTask createPendingTaskWithExistingTask(
            AndroidBrowserWindowCreateParams createParams,
            @Nullable JniOnceCallback<Long> taskCreationCallbackForNative) {
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(
                        IdSequencer.next());
        mChromeAndroidTaskTracker.obtainTask(
                BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);

        return mChromeAndroidTaskTracker.createPendingTask(
                createParams, taskCreationCallbackForNative);
    }

    private void doTestDispatchPendingShowInactiveOrDeactivate(@PendingAction int action) {
        assert action == PendingAction.SHOW_INACTIVE || action == PendingAction.DEACTIVATE;
        // Arrange: Create live task and make it the top resumed task.
        int initialTopResumedTaskId = 0;
        var initialTopResumedActivityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(
                        initialTopResumedTaskId);
        var initialTopResumedTask =
                (ChromeAndroidTaskImpl)
                        mChromeAndroidTaskTracker.obtainTask(
                                BrowserWindowType.NORMAL,
                                initialTopResumedActivityScopedObjects,
                                /* pendingId= */ null);
        initialTopResumedTask.onTopResumedActivityChangedWithNative(true);
        initialTopResumedTask.onActivityTopResumedChanged(true);
        var mockWindowAndroid = assumeNonNull(initialTopResumedTask.getTopActivityWindowAndroid());
        var mockActivity = assumeNonNull(mockWindowAndroid.getActivity().get());
        var mockActivityManager =
                (ActivityManager) mockActivity.getSystemService(Context.ACTIVITY_SERVICE);
        // Arrange: Create pending task.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var pendingTask =
                assertNonNull(mChromeAndroidTaskTracker.createPendingTask(mockParams, null));
        // Arrange: Request SHOW_INACTIVE or DEACTIVATE on the pending task.
        if (action == PendingAction.SHOW_INACTIVE) {
            pendingTask.showInactive();
        } else {
            pendingTask.deactivate();
        }

        // Act: Simulate newly created activity gains focus.
        mFakeTime.advanceMillis(100);

        var newActivityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 2);
        var newTask =
                (ChromeAndroidTaskImpl)
                        mChromeAndroidTaskTracker.obtainTask(
                                BrowserWindowType.NORMAL,
                                newActivityScopedObjects,
                                assumeNonNull(pendingTask.getPendingTaskInfo()).mPendingTaskId);
        newTask.onTopResumedActivityChangedWithNative(true);
        newTask.onActivityTopResumedChanged(true);

        // Assert: Penultimately activated task gets activated.
        verify(mockActivityManager).moveTaskToFront(initialTopResumedTaskId, 0);
    }
}
