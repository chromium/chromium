// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link GlobalBrowserCollectionPlatformDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GlobalBrowserCollectionPlatformDelegateUnitTest {
    private static final long DELEGATE_PTR = 78787878L;

    @Rule public final MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private GlobalBrowserCollectionPlatformDelegate.Natives mNativeMock;

    private final ChromeAndroidTaskTrackerImpl mTaskTracker =
            ChromeAndroidTaskTrackerImpl.getInstance();

    @Before
    public void setUp() {
        GlobalBrowserCollectionPlatformDelegateJni.setInstanceForTesting(mNativeMock);

        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();
    }

    @After
    public void tearDown() {
        mTaskTracker.removeAllForTesting();
    }

    @Test
    public void testCreateAndDestroy() {
        GlobalBrowserCollectionPlatformDelegate delegate =
                new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);

        assertTrue(mTaskTracker.hasObserverForTesting(delegate));

        delegate.destroy();
        assertFalse(mTaskTracker.hasObserverForTesting(delegate));
    }

    @Test
    public void testExistingTask() {
        // Arrange: Create a new task.
        int taskId = 1;
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        var chromeAndroidTask =
                mTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);
        GlobalBrowserCollectionPlatformDelegate delegate =
                new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);

        verify(mNativeMock).onBrowserCreated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        assertTrue(chromeAndroidTask.hasAndroidBrowserWindowObserver(delegate));

        delegate.destroy();
    }

    @Test
    public void testTaskAddedAndRemoved() {
        GlobalBrowserCollectionPlatformDelegate delegate =
                new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);
        verify(mNativeMock, never()).onBrowserCreated(anyLong(), anyLong());

        int taskId = 1;
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        var chromeAndroidTask =
                mTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);

        verify(mNativeMock).onBrowserCreated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        assertTrue(chromeAndroidTask.hasAndroidBrowserWindowObserver(delegate));

        delegate.onTaskRemoved(chromeAndroidTask);
        verify(mNativeMock).onBrowserClosed(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        assertFalse(chromeAndroidTask.hasAndroidBrowserWindowObserver(delegate));

        delegate.destroy();
    }

    @Test
    public void testBrowserWindowAddedAndRemoved() {
        GlobalBrowserCollectionPlatformDelegate delegate =
                new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);

        delegate.onBrowserWindowAdded(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        verify(mNativeMock).onBrowserCreated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        delegate.onBrowserWindowRemoved(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        verify(mNativeMock).onBrowserClosed(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        delegate.destroy();
    }

    @Test
    public void testNativePointerZero() {
        GlobalBrowserCollectionPlatformDelegate delegate =
                new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);
        delegate.destroy();

        delegate.onBrowserWindowAdded(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        delegate.onBrowserWindowRemoved(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        verify(mNativeMock, never())
                .onBrowserCreated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        verify(mNativeMock, never())
                .onBrowserClosed(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }
}
