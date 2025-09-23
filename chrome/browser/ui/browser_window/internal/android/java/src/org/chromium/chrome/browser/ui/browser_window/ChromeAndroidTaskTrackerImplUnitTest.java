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

import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTracker.EXTRA_PENDING_BROWSER_WINDOW_TASK_ID;

import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
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

import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.mojom.WindowShowState;

import java.util.OptionalInt;

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
        var task = mChromeAndroidTaskTracker.createPendingTask(mockParams);

        // Assert.
        assertNotNull(task);
        assertFalse(task.getPendingId().isEmpty());
        assertNotNull(
                mChromeAndroidTaskTracker.getPendingTaskForTesting(task.getPendingId().getAsInt()));
        assertEquals(1, mChromeAndroidTaskTracker.getAllNativeBrowserWindowPtrs().length);
        assertTrue(task.getId().isEmpty());
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
                () -> mChromeAndroidTaskTracker.createPendingTask(mockParams));
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
                () -> mChromeAndroidTaskTracker.createPendingTask(mockParams));
    }

    @Test
    public void createPendingTask_startsActivityWithIntent() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();

        // Act.
        mChromeAndroidTaskTracker.createPendingTask(mockParams);

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
        mChromeAndroidTaskTracker.createPendingTask(mockParams);

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
        var pendingTask = mChromeAndroidTaskTracker.createPendingTask(mockParams);
        int pendingId = pendingTask.getPendingId().getAsInt();

        int taskId = 123;
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));

        // Act.
        var task =
                mChromeAndroidTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid,
                        tabModel,
                        OptionalInt.of(pendingId));

        // Assert.
        assertNull(mChromeAndroidTaskTracker.getPendingTaskForTesting(pendingId));
        assertEquals(1, mChromeAndroidTaskTracker.getAllNativeBrowserWindowPtrs().length);
        assertEquals(activityWindowAndroid, task.getActivityWindowAndroid());
        assertEquals("The pending task should be adopted.", pendingTask, task);
        assertEquals("Task ID should be updated.", taskId, task.getId().getAsInt());
        assertTrue("Pending ID should be cleared.", task.getPendingId().isEmpty());
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
                                OptionalInt.of(invalidPendingId)));
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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid,
                        tabModel,
                        OptionalInt.empty());

        // Assert.
        assertEquals(1, chromeAndroidTask.getId().getAsInt());
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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid1,
                        tabModel,
                        OptionalInt.empty());

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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid2,
                        tabModel,
                        OptionalInt.empty());

        // Assert.
        assertEquals(chromeAndroidTask1, chromeAndroidTask2);
        assertEquals(taskId, chromeAndroidTask2.getId().getAsInt());
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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid1,
                        tabModel,
                        OptionalInt.empty());

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
                            BrowserWindowType.POPUP,
                            activityWindowAndroid2,
                            tabModel,
                            OptionalInt.empty());
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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid,
                        tabModel,
                        OptionalInt.empty());

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
                BrowserWindowType.NORMAL, activityWindowAndroid, tabModel, OptionalInt.empty());

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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid1,
                        tabModel,
                        OptionalInt.empty());

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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid1,
                        tabModel,
                        OptionalInt.empty());

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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid,
                        tabModel,
                        OptionalInt.empty());

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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid,
                        tabModel,
                        OptionalInt.empty());

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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid,
                        tabModel,
                        OptionalInt.empty());

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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid,
                        tabModel,
                        OptionalInt.empty());

        // Assert (add task).
        verify(observer).onTaskAdded(chromeAndroidTask);

        // Act (remove task).
        mChromeAndroidTaskTracker.remove(chromeAndroidTask.getId().getAsInt());

        // Assert (remove task).
        verify(observer).onTaskRemoved(chromeAndroidTask);

        // Cleanup.
        mChromeAndroidTaskTracker.removeObserver(observer);
    }

    @Test
    public void activatePenultimatelyActivatedTask_noTasks_doesNothing() {
        // Act.
        mChromeAndroidTaskTracker.activatePenultimatelyActivatedTask();

        // Assert.
        // No crash.
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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid1,
                        tabModel,
                        OptionalInt.empty());
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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid1,
                        tabModel,
                        OptionalInt.empty());
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
                        BrowserWindowType.NORMAL,
                        activityWindowAndroid2,
                        tabModel,
                        OptionalInt.empty());
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
        verify(activityManager2, never()).moveTaskToFront(eq(task2.getId().getAsInt()), anyInt());
        verify(activityManager1).moveTaskToFront(eq(task1.getId().getAsInt()), anyInt());
    }
}
