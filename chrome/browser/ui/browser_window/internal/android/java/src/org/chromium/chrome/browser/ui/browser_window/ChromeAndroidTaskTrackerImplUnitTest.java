// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;

@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeAndroidTaskTrackerImplUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

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

        // Act.
        var chromeAndroidTask = mChromeAndroidTaskTracker.obtainTask(activityWindowAndroid);

        // Assert.
        assertEquals(1, chromeAndroidTask.getId());
        assertEquals(activityWindowAndroid, chromeAndroidTask.getActivityWindowAndroid());
    }

    @Test
    public void obtainTask_activityWindowAndroidBelongsToExistingTask_reusesExistingTask() {
        // Arrange.
        // (1) Create a new task.
        int taskId = 1;
        var activityWindowAndroid1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var chromeAndroidTask1 = mChromeAndroidTaskTracker.obtainTask(activityWindowAndroid1);

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
        var chromeAndroidTask2 = mChromeAndroidTaskTracker.obtainTask(activityWindowAndroid2);

        // Assert.
        assertEquals(chromeAndroidTask1, chromeAndroidTask2);
        assertEquals(taskId, chromeAndroidTask2.getId());
        assertEquals(activityWindowAndroid2, chromeAndroidTask2.getActivityWindowAndroid());
    }

    @Test
    public void get_taskExists_returnsTask() {
        // Arrange.
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var chromeAndroidTask = mChromeAndroidTaskTracker.obtainTask(activityWindowAndroid);

        // Act & Assert.
        assertEquals(chromeAndroidTask, mChromeAndroidTaskTracker.get(/* taskId= */ 1));
    }

    @Test
    public void get_taskDoesNotExist_returnsNull() {
        // Arrange.
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        mChromeAndroidTaskTracker.obtainTask(activityWindowAndroid);

        // Act & Assert.
        assertEquals(null, mChromeAndroidTaskTracker.get(/* taskId= */ 2));
    }

    @Test
    public void remove_taskExists_destroysAndRemovesTask() {
        // Arrange.
        int taskId = 1;
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var chromeAndroidTask = mChromeAndroidTaskTracker.obtainTask(activityWindowAndroid);

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
        var chromeAndroidTask = mChromeAndroidTaskTracker.obtainTask(activityWindowAndroid);

        // Act.
        mChromeAndroidTaskTracker.remove(/* taskId= */ 2);

        // Assert.
        assertFalse(chromeAndroidTask.isDestroyed());
        assertEquals(chromeAndroidTask, mChromeAndroidTaskTracker.get(/* taskId= */ 1));
    }
}
