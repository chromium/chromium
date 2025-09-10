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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.content.Context;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;

/** Unit tests for {@link ChromeAndroidTaskTrackerImpl}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeAndroidTaskTrackerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    private final ChromeAndroidTaskTrackerImpl mChromeAndroidTaskTracker =
            ChromeAndroidTaskTrackerImpl.getInstance();

    @After
    public void tearDown() {
        mChromeAndroidTaskTracker.removeAllForTesting();
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
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel);

        // Assert.
        assertEquals(1, chromeAndroidTask.getId());
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
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel);

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
                        BrowserWindowType.NORMAL, activityWindowAndroid2, tabModel);

        // Assert.
        assertEquals(chromeAndroidTask1, chromeAndroidTask2);
        assertEquals(taskId, chromeAndroidTask2.getId());
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
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel);

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
                            BrowserWindowType.POPUP, activityWindowAndroid2, tabModel);
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
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel);

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
                BrowserWindowType.NORMAL, activityWindowAndroid, tabModel);

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
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel);

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
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel);

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
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel);

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
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel);

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
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel);

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
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel);

        // Assert (add task).
        verify(observer).onTaskAdded(chromeAndroidTask);

        // Act (remove task).
        mChromeAndroidTaskTracker.remove(chromeAndroidTask.getId());

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
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel);
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
                        BrowserWindowType.NORMAL, activityWindowAndroid1, tabModel);
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
                        BrowserWindowType.NORMAL, activityWindowAndroid2, tabModel);
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
}
