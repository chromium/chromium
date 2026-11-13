// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR;

import org.jni_zero.JniUniquePtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.AndroidBrowserWindowObserver.AndroidBrowserWindowInfo;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.NativeBrowserWindowInterface;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.HashMap;

/** Unit tests for {@link GlobalBrowserCollectionPlatformDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlobalBrowserCollectionPlatformDelegateUnitTest {
    private static final long DELEGATE_PTR = 78787878L;

    @Rule public final MockitoRule mockitoRule = MockitoJUnit.rule();
    @Rule public final FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private GlobalBrowserCollectionPlatformDelegate.Natives mNativeMock;

    private final ChromeAndroidTaskTrackerImpl mTaskTracker =
            ChromeAndroidTaskTrackerImpl.getInstance();
    private @Nullable GlobalBrowserCollectionPlatformDelegate mDelegate;

    @Before
    public void setUp() {
        GlobalBrowserCollectionPlatformDelegateJni.setInstanceForTesting(mNativeMock);

        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();
    }

    @After
    public void tearDown() {
        if (mDelegate != null) {
            mDelegate.destroy();
        }
        mTaskTracker.removeAllForTesting();
    }

    @Test
    public void testCreateAndDestroy() {
        mDelegate = new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);

        assertTrue(mTaskTracker.hasObserverForTesting(mDelegate));

        mDelegate.destroy();
        assertFalse(mTaskTracker.hasObserverForTesting(mDelegate));
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
        mDelegate = new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);

        verify(mNativeMock).onBrowserCreated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        verify(mNativeMock, never()).onBrowserActivated(anyLong(), anyLong());
        assertTrue(chromeAndroidTask.hasAndroidBrowserWindowObserver(mDelegate));

        mDelegate.destroy();
        assertFalse(chromeAndroidTask.hasAndroidBrowserWindowObserver(mDelegate));
    }

    @Test
    public void testExistingTaskWithActiveWindow() {
        var mockWindowNatives = mock(AndroidBrowserWindow.Natives.class);
        var windowPtrMap = new HashMap<JniUniquePtr<NativeBrowserWindowInterface>, Long>();
        when(mockWindowNatives.create(any(), anyInt(), any()))
                .thenAnswer(
                        invocation -> {
                            AndroidBrowserWindow window = invocation.getArgument(0);
                            long fakePtr = window.getTask().getId() * 1000L;
                            @SuppressWarnings("unchecked")
                            JniUniquePtr<NativeBrowserWindowInterface> ptr =
                                    mock(JniUniquePtr.class);
                            windowPtrMap.put(ptr, fakePtr);
                            return ptr;
                        });
        when(mockWindowNatives.getLegacyNativePtr(any()))
                .thenAnswer(invocation -> windowPtrMap.getOrDefault(invocation.getArgument(0), 0L));
        AndroidBrowserWindowJni.setInstanceForTesting(mockWindowNatives);

        // Task 1: Never activated.
        var activityScopedObjects1 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 1);
        mTaskTracker.obtainTask(
                BrowserWindowType.NORMAL, activityScopedObjects1, /* pendingId= */ null);

        // Task 2: More-recently-activated inactive task (resumed at t1 > t0, then paused).
        var activityScopedObjects2 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 2);
        var task2 =
                (TopResumedActivityChangedWithNativeObserver)
                        mTaskTracker.obtainTask(
                                BrowserWindowType.NORMAL,
                                activityScopedObjects2,
                                /* pendingId= */ null);

        // Task 3: Older-activated inactive task (resumed at t0, then paused).
        var activityScopedObjects3 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 3);
        var task3 =
                (TopResumedActivityChangedWithNativeObserver)
                        mTaskTracker.obtainTask(
                                BrowserWindowType.NORMAL,
                                activityScopedObjects3,
                                /* pendingId= */ null);

        // Task 4: Currently active task (isTopResumedActivity() == true).
        var activityScopedObjects4 =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(/* taskId= */ 4);
        when(activityScopedObjects4.mActivityWindowAndroid.isTopResumedActivity()).thenReturn(true);
        mTaskTracker.obtainTask(
                BrowserWindowType.NORMAL, activityScopedObjects4, /* pendingId= */ null);

        // Advance time so task3 is activated at t0, and task2 at t1 > t0.
        mFakeTimeTestRule.advanceMillis(100L);
        task3.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        task3.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ false);

        mFakeTimeTestRule.advanceMillis(100L);
        task2.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        task2.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ false);

        mDelegate = new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);

        var inOrder = inOrder(mNativeMock);
        inOrder.verify(mNativeMock).onBrowserCreated(DELEGATE_PTR, 1000L);
        inOrder.verify(mNativeMock).onBrowserCreated(DELEGATE_PTR, 2000L);
        inOrder.verify(mNativeMock).onBrowserCreated(DELEGATE_PTR, 3000L);
        inOrder.verify(mNativeMock).onBrowserCreated(DELEGATE_PTR, 4000L);

        inOrder.verify(mNativeMock).onBrowserActivated(DELEGATE_PTR, 3000L);
        inOrder.verify(mNativeMock).onBrowserActivated(DELEGATE_PTR, 2000L);
        inOrder.verify(mNativeMock).onBrowserActivated(DELEGATE_PTR, 4000L);
        verify(mNativeMock, never()).onBrowserActivated(DELEGATE_PTR, 1000L);

        mDelegate.destroy();
    }

    @Test
    public void testTaskAddedAndRemoved() {
        mDelegate = new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);
        verify(mNativeMock, never()).onBrowserCreated(anyLong(), anyLong());

        int taskId = 1;
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        var chromeAndroidTask =
                mTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);

        verify(mNativeMock).onBrowserCreated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        assertTrue(chromeAndroidTask.hasAndroidBrowserWindowObserver(mDelegate));

        mTaskTracker.remove(taskId);
        verify(mNativeMock).onBrowserClosed(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        assertFalse(chromeAndroidTask.hasAndroidBrowserWindowObserver(mDelegate));

        mDelegate.destroy();
    }

    @Test
    public void testTaskAddedWithActiveWindow() {
        mDelegate = new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);

        // Create an activity scoped objects whose task has an already active window.
        int taskId = 10;
        var activityScopedObjects =
                ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(taskId);
        when(activityScopedObjects.mActivityWindowAndroid.isTopResumedActivity()).thenReturn(true);
        var task =
                mTaskTracker.obtainTask(
                        BrowserWindowType.NORMAL, activityScopedObjects, /* pendingId= */ null);
        assertTrue(task.hasAndroidBrowserWindowObserver(mDelegate));

        var inOrder = inOrder(mNativeMock);
        inOrder.verify(mNativeMock)
                .onBrowserCreated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        inOrder.verify(mNativeMock)
                .onBrowserActivated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        mDelegate.destroy();
    }

    @Test
    public void testBrowserWindowAddedAndRemoved() {
        mDelegate = new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);
        var info =
                new AndroidBrowserWindowInfo(
                        FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR,
                        mock(Profile.class),
                        mock(ActivityWindowAndroid.class));

        mDelegate.onBrowserWindowAdded(info);
        verify(mNativeMock).onBrowserCreated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        mDelegate.onBrowserWindowActivated(info);
        verify(mNativeMock)
                .onBrowserActivated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        mDelegate.onBrowserWindowDeactivated(info);
        verify(mNativeMock)
                .onBrowserDeactivated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        mDelegate.onBrowserWindowRemoved(info);
        verify(mNativeMock).onBrowserClosed(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        mDelegate.destroy();
    }

    @Test
    public void testNativePointerZero() {
        mDelegate = new GlobalBrowserCollectionPlatformDelegate(DELEGATE_PTR);
        mDelegate.destroy();

        var info =
                new AndroidBrowserWindowInfo(
                        FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR,
                        mock(Profile.class),
                        mock(ActivityWindowAndroid.class));
        mDelegate.onBrowserWindowAdded(info);
        mDelegate.onBrowserWindowRemoved(info);
        mDelegate.onBrowserWindowActivated(info);
        mDelegate.onBrowserWindowDeactivated(info);
        verify(mNativeMock, never())
                .onBrowserCreated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        verify(mNativeMock, never())
                .onBrowserClosed(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        verify(mNativeMock, never())
                .onBrowserActivated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        verify(mNativeMock, never())
                .onBrowserDeactivated(DELEGATE_PTR, FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }
}
