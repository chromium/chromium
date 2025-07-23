// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class ChromeAndroidTaskImplUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static ChromeAndroidTaskImpl createChromeAndroidTask() {
        return createChromeAndroidTask(/* taskId= */ 1);
    }

    private static ChromeAndroidTaskImpl createChromeAndroidTask(int taskId) {
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        return new ChromeAndroidTaskImpl(activityWindowAndroid);
    }

    @Test
    public void constructor_setsActivityWindowAndroid() {
        // Arrange.
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);

        // Act.
        var chromeAndroidTask = new ChromeAndroidTaskImpl(activityWindowAndroid);

        // Assert.
        assertEquals(activityWindowAndroid, chromeAndroidTask.getActivityWindowAndroid());
    }

    @Test
    public void getId_returnsTaskId() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask = createChromeAndroidTask(taskId);

        // Act & Assert.
        assertEquals(taskId, chromeAndroidTask.getId());
    }

    @Test
    public void setActivityWindowAndroid_refAlreadyExists_throwsException() {
        // Arrange.
        var activityWindowAndroid1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var activityWindowAndroid2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var chromeAndroidTask = new ChromeAndroidTaskImpl(activityWindowAndroid1);

        // Act & Assert.
        assertThrows(
                AssertionError.class,
                () -> chromeAndroidTask.setActivityWindowAndroid(activityWindowAndroid2));
    }

    @Test
    public void setActivityWindowAndroid_previousRefCleared_setsNewRef() {
        // Arrange.
        var activityWindowAndroid1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var activityWindowAndroid2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var chromeAndroidTask = new ChromeAndroidTaskImpl(activityWindowAndroid1);
        chromeAndroidTask.clearActivityWindowAndroid();

        // Act.
        chromeAndroidTask.setActivityWindowAndroid(activityWindowAndroid2);

        // Assert.
        assertEquals(activityWindowAndroid2, chromeAndroidTask.getActivityWindowAndroid());
    }

    @Test
    public void
            setActivityWindowAndroid_previousRefCleared_newRefHasDifferentTaskId_throwsException() {
        // Arrange.
        var activityWindowAndroid1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var activityWindowAndroid2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 2);
        var chromeAndroidTask = new ChromeAndroidTaskImpl(activityWindowAndroid1);
        chromeAndroidTask.clearActivityWindowAndroid();

        // Act & Assert.
        assertThrows(
                AssertionError.class,
                () -> chromeAndroidTask.setActivityWindowAndroid(activityWindowAndroid2));
    }

    @Test
    public void setActivityWindowAndroid_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask = createChromeAndroidTask(taskId);
        chromeAndroidTask.destroy();

        // Act & Assert.
        var newActivityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        assertThrows(
                AssertionError.class,
                () -> chromeAndroidTask.setActivityWindowAndroid(newActivityWindowAndroid));
    }

    @Test
    public void getActivityWindowAndroid_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        var chromeAndroidTask = createChromeAndroidTask();
        chromeAndroidTask.destroy();

        // Act & Assert.
        assertThrows(AssertionError.class, () -> chromeAndroidTask.getActivityWindowAndroid());
    }

    @Test
    public void destroy_clearsActivityWindowAndroid() {
        // Arrange.
        var chromeAndroidTask = createChromeAndroidTask();

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        assertNull(chromeAndroidTask.getActivityWindowAndroidForTesting());
    }

    @Test
    public void destroy_setsDestroyedToTrue() {
        // Arrange.
        var chromeAndroidTask = createChromeAndroidTask();
        assertFalse(chromeAndroidTask.isDestroyed());

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        assertTrue(chromeAndroidTask.isDestroyed());
    }
}
