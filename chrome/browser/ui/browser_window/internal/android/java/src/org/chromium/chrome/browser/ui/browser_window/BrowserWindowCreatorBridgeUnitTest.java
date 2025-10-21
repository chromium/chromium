// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.JniOnceCallback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;

/** Unit tests for {@link BrowserWindowCreatorBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BrowserWindowCreatorBridgeUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();
    }

    @After
    public void tearDown() {
        ((ChromeAndroidTaskTrackerImpl) ChromeAndroidTaskTrackerFactory.getInstance())
                .removeAllForTesting();
    }

    @Test
    public void createBrowserWindow_returnsNativeBrowserWindowPtr() {
        // Arrange.
        var createParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var tracker = (ChromeAndroidTaskTrackerImpl) ChromeAndroidTaskTrackerFactory.getInstance();

        // Act.
        long result = BrowserWindowCreatorBridge.createBrowserWindow(createParams);

        // Assert.
        var nativeBrowserWindowPtrs = tracker.getAllNativeBrowserWindowPtrs();
        assertEquals(1, nativeBrowserWindowPtrs.length);
        assertEquals(nativeBrowserWindowPtrs[0], result);
    }

    @Test
    public void createBrowserWindowAsync_invokesCallbackAndTransitionsTaskToAlive() {
        // Arrange.
        var createParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var tracker = (ChromeAndroidTaskTrackerImpl) ChromeAndroidTaskTrackerFactory.getInstance();
        JniOnceCallback<Long> mockCallback = mock();

        // Act.
        BrowserWindowCreatorBridge.createBrowserWindowAsync(createParams, mockCallback);

        // Assert initial state.
        var pendingTasks = tracker.getPendingTasksForTesting();
        assertEquals(1, pendingTasks.size());
        var pendingTask = (ChromeAndroidTaskImpl) pendingTasks.values().iterator().next();
        assertEquals(ChromeAndroidTaskImpl.State.PENDING, pendingTask.getState());
        assertNull(pendingTask.getId());
        assertNotNull(pendingTask.getPendingId());

        // Act: simulate activity attachment.
        int taskId = 123;
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mock(Profile.class));
        pendingTask.setActivityWindowAndroid(activityWindowAndroid, tabModel);

        // Assert final state.
        assertEquals(ChromeAndroidTaskImpl.State.ALIVE, pendingTask.getState());
        assertEquals(taskId, pendingTask.getId().intValue());
        assertNull(pendingTask.getPendingId());
        verify(tabModel).addObserver(pendingTask);
        verify(mockCallback)
                .onResult(ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }
}
