// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static android.os.Looper.getMainLooper;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.util.Pair;
import android.view.WindowMetrics;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.Promise;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.ActivityScopedObjects;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskImpl.State;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.ActivityWindowAndroidMocks;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.ChromeAndroidTaskWithMockDeps;
import org.chromium.chrome.browser.ui.browser_window.PendingActionManager.PendingAction;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.mojom.WindowShowState;

import java.util.Arrays;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
public class ChromeAndroidTaskImplUnitTest {

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    private static ChromeAndroidTaskWithMockDeps createChromeAndroidTaskWithMockDeps(int taskId) {
        return createChromeAndroidTaskWithMockDeps(taskId, /* isPendingTask= */ false);
    }

    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    private static ChromeAndroidTaskWithMockDeps createChromeAndroidTaskWithMockDeps(
            int taskId, boolean isPendingTask) {
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        taskId, /* mockNatives= */ true, isPendingTask);
        var activityWindowAndroidMocks = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks;
        mockDesktopWindowingMode(activityWindowAndroidMocks);

        return chromeAndroidTaskWithMockDeps;
    }

    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    private static ActivityScopedObjects createActivityScopedObjects(int taskId) {
        var activityWindowAndroidMocks =
                ChromeAndroidTaskUnitTestSupport.createActivityWindowAndroidMocks(taskId);
        mockDesktopWindowingMode(activityWindowAndroidMocks);
        return ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(
                activityWindowAndroidMocks.mMockActivityWindowAndroid, mock(Profile.class));
    }

    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    private static void mockDesktopWindowingMode(
            ActivityWindowAndroidMocks activityWindowAndroidMocks) {
        ChromeAndroidTaskUnitTestSupport.mockDesktopWindowingMode(activityWindowAndroidMocks);

        // Move mock Activity to the "resumed" state.
        var mockActivity = activityWindowAndroidMocks.mMockActivity;
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.RESUMED);
    }

    @Test
    public void constructor_withActivityScopedObjects_setsRef() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Assert.
        var activityScopedObjects = chromeAndroidTask.getActivityScopedObjectsForTesting();
        assertNotNull(activityScopedObjects);
    }

    @Test
    public void constructor_withActivityScopedObjects_registersActivityLifecycleObservers() {
        // Arrange & Act.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var mockActivityLifecycleDispatcher =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityLifecycleDispatcher;

        // Assert.
        verify(mockActivityLifecycleDispatcher, times(1))
                .register(isA(TopResumedActivityChangedWithNativeObserver.class));
        verify(mockActivityLifecycleDispatcher, times(1))
                .register(isA(ConfigurationChangedObserver.class));
    }

    @Test
    public void constructor_withActivityScopedObjects_registersTaskVisibilityListener() {
        // Arrange & Act.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);

        // Assert.
        assertTrue(
                ApplicationStatus.getTaskVisibilityListenersForTesting()
                        .hasObserver(
                                (ChromeAndroidTaskImpl)
                                        chromeAndroidTaskWithMockDeps.mChromeAndroidTask));
    }

    @Test
    public void constructor_withActivityScopedObjects_registersTabModelObserver() {
        // Arrange & Act.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockTabModel = chromeAndroidTask.getActivityScopedObjectsForTesting().mTabModel;

        // Assert.
        verify(mockTabModel, times(1)).addObserver(chromeAndroidTask);
    }

    @Test
    public void constructor_withActivityScopedObjects_associateTabModelWithNativeBrowserWindow() {
        // Arrange & Act.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockTabModel = chromeAndroidTask.getActivityScopedObjectsForTesting().mTabModel;

        // Assert.
        verify(mockTabModel, times(1))
                .associateWithBrowserWindow(
                        ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }

    @Test
    public void constructor_withPendingTaskInfo_pendingState() {
        // Arrange.
        var pendingTaskInfo = ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo();

        // Act.
        var task = new ChromeAndroidTaskImpl(pendingTaskInfo);

        // Assert.
        var actualPendingTaskInfo = task.getPendingTaskInfo();
        assertNotNull(actualPendingTaskInfo);
        assertEquals(pendingTaskInfo.mPendingTaskId, actualPendingTaskInfo.mPendingTaskId);
        assertEquals(pendingTaskInfo.mCreateParams.getWindowType(), task.getBrowserWindowType());
        assertEquals(pendingTaskInfo.mCreateParams.getProfile(), task.getProfile());
        assertEquals(State.PENDING_CREATE, task.getState());
        assertNull(task.getId());
        assertNull(task.getActivityScopedObjectsForTesting());
    }

    @Test
    public void getProfile_returnsInitialProfile() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var initialProfile = chromeAndroidTaskWithMockDeps.mMockProfile;

        // Act & Assert.
        assertEquals(
                "The returned Profile should be the same as the one from the constructor.",
                initialProfile,
                chromeAndroidTask.getProfile());
    }

    @Test
    public void didAddTab_withDifferentProfile_throwsAssertionError() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var initialProfile = chromeAndroidTaskWithMockDeps.mMockProfile;

        var differentProfile = mock(Profile.class, "DifferentProfile");
        var tabWithDifferentProfile = mock(Tab.class);
        when(tabWithDifferentProfile.getProfile()).thenReturn(differentProfile);

        // Act & Assert.
        assertNotEquals(initialProfile, differentProfile);
        assertThrows(
                AssertionError.class,
                () ->
                        chromeAndroidTask.didAddTab(
                                tabWithDifferentProfile,
                                TabLaunchType.FROM_CHROME_UI,
                                TabCreationState.LIVE_IN_FOREGROUND,
                                /* markedForSelection= */ true));
    }

    @Test
    public void getId_returnsTaskId() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask = createChromeAndroidTaskWithMockDeps(taskId).mChromeAndroidTask;

        // Act & Assert.
        assertEquals(taskId, (int) chromeAndroidTask.getId());
    }

    @Test
    public void setActivityScopedObjects_refAlreadyExists_throwsException() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask = createChromeAndroidTaskWithMockDeps(taskId).mChromeAndroidTask;
        var newActivityScopedObjects = createActivityScopedObjects(taskId);

        // Act & Assert.
        assertThrows(
                AssertionError.class,
                () -> chromeAndroidTask.setActivityScopedObjects(newActivityScopedObjects));
    }

    @Test
    public void setActivityScopedObjects_fromPendingState_setsIdAndState() {
        int taskId = 2;

        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(taskId, /* isPendingTask= */ true);
        var pendingTask = (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;

        // Act.
        pendingTask.setActivityScopedObjects(activityScopedObjects);
        pendingTask.onNativeInitializationFinished();

        // Assert.
        assertEquals(taskId, (int) pendingTask.getId());
        assertNull(pendingTask.getPendingTaskInfo());
        assertEquals(activityScopedObjects, pendingTask.getActivityScopedObjectsForTesting());
    }

    @Test
    public void setActivityScopedObjects_previousRefCleared_setsNewRef() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl)
                        createChromeAndroidTaskWithMockDeps(taskId).mChromeAndroidTask;
        var newActivityScopedObjects = createActivityScopedObjects(taskId);
        chromeAndroidTask.clearActivityScopedObjects();

        // Act.
        chromeAndroidTask.setActivityScopedObjects(newActivityScopedObjects);

        // Assert.
        assertEquals(
                newActivityScopedObjects, chromeAndroidTask.getActivityScopedObjectsForTesting());
    }

    @Test
    public void
            setActivityScopedObjects_previousRefCleared_registersNewActivityLifecycleObservers() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask = createChromeAndroidTaskWithMockDeps(taskId).mChromeAndroidTask;
        var newActivityScopedObjects = createActivityScopedObjects(taskId);
        chromeAndroidTask.clearActivityScopedObjects();

        // Act.
        chromeAndroidTask.setActivityScopedObjects(newActivityScopedObjects);

        // Assert.
        var activity = newActivityScopedObjects.mActivityWindowAndroid.getActivity().get();
        assertNotNull(activity);
        assertTrue(activity instanceof ActivityLifecycleDispatcherProvider);
        var activityLifecycleDispatcher =
                ((ActivityLifecycleDispatcherProvider) activity).getLifecycleDispatcher();
        verify(activityLifecycleDispatcher, times(1))
                .register(isA(TopResumedActivityChangedWithNativeObserver.class));
        verify(activityLifecycleDispatcher, times(1))
                .register(isA(ConfigurationChangedObserver.class));
    }

    @Test
    public void setActivityScopedObjects_previousRefCleared_registersNewTaskVisibilityListener() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl)
                        createChromeAndroidTaskWithMockDeps(taskId).mChromeAndroidTask;
        var newActivityScopedObjects = createActivityScopedObjects(taskId);
        chromeAndroidTask.clearActivityScopedObjects();
        assertFalse(
                "Listener should be removed",
                ApplicationStatus.getTaskVisibilityListenersForTesting()
                        .hasObserver(chromeAndroidTask));

        // Act.
        chromeAndroidTask.setActivityScopedObjects(newActivityScopedObjects);

        // Assert.
        assertTrue(
                "Listener should be added",
                ApplicationStatus.getTaskVisibilityListenersForTesting()
                        .hasObserver(chromeAndroidTask));
    }

    @Test
    public void setActivityScopedObjects_previousRefCleared_registersTabModelObserver() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var oldMockTabModel = chromeAndroidTask.getActivityScopedObjectsForTesting().mTabModel;

        var newActivityScopedObjects = createActivityScopedObjects(taskId);
        var newMockTabModel = newActivityScopedObjects.mTabModel;

        // Act.
        chromeAndroidTask.clearActivityScopedObjects();
        chromeAndroidTask.setActivityScopedObjects(newActivityScopedObjects);

        // Assert.
        // Verify observer was removed from the old TabModel.
        verify(oldMockTabModel, times(1)).removeObserver(chromeAndroidTask);

        // Verify the new TabModel is being observed.
        assertEquals(
                newMockTabModel,
                assumeNonNull(chromeAndroidTask.getActivityScopedObjectsForTesting()).mTabModel);
        verify(newMockTabModel, times(1)).addObserver(chromeAndroidTask);
    }

    @Test
    public void
            setActivityScopedObjects_previousRefCleared_associatesTabModelWithNativeBrowserWindow() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var oldMockTabModel = chromeAndroidTask.getActivityScopedObjectsForTesting().mTabModel;

        var newActivityScopedObjects = createActivityScopedObjects(taskId);
        var newMockTabModel = newActivityScopedObjects.mTabModel;

        // Act.
        chromeAndroidTask.clearActivityScopedObjects();
        chromeAndroidTask.setActivityScopedObjects(newActivityScopedObjects);

        // Assert.
        verify(newMockTabModel, times(1))
                .associateWithBrowserWindow(
                        ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }

    @Test
    public void
            setActivityScopedObjects_previousRefCleared_newRefHasDifferentTaskId_throwsException() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        var newActivityScopedObjects = createActivityScopedObjects(/* taskId= */ 2);
        chromeAndroidTask.clearActivityScopedObjects();

        // Act & Assert.
        assertThrows(
                AssertionError.class,
                () -> chromeAndroidTask.setActivityScopedObjects(newActivityScopedObjects));
    }

    @Test
    public void setActivityScopedObjects_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        chromeAndroidTask.destroy();

        // Act & Assert.
        var newActivityScopedObjects = createActivityScopedObjects(taskId);
        assertThrows(
                AssertionError.class,
                () -> chromeAndroidTask.setActivityScopedObjects(newActivityScopedObjects));
    }

    @Test
    public void getActivityWindowAndroid_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        chromeAndroidTask.destroy();

        // Act & Assert.
        assertThrows(AssertionError.class, chromeAndroidTask::getActivityWindowAndroid);
    }

    @Test
    public void clearActivityScopedObjects_unregistersActivityLifecycleObservers() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockActivityLifecycleDispatcher =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityLifecycleDispatcher;

        // Act.
        chromeAndroidTask.clearActivityScopedObjects();

        // Assert.
        verify(mockActivityLifecycleDispatcher, times(1))
                .unregister(isA(TopResumedActivityChangedWithNativeObserver.class));
        verify(mockActivityLifecycleDispatcher, times(1))
                .unregister(isA(ConfigurationChangedObserver.class));
    }

    @Test
    public void clearActivityScopedObjects_unregistersTaskVisibilityListener() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Act.
        chromeAndroidTask.clearActivityScopedObjects();

        // Assert.
        Assert.assertFalse(
                "Listener should be removed",
                ApplicationStatus.getTaskVisibilityListenersForTesting()
                        .hasObserver(chromeAndroidTask));
    }

    @Test
    public void clearActivityScopedObjects_unregistersTabModelObserver() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockTabModel = chromeAndroidTask.getActivityScopedObjectsForTesting().mTabModel;

        // Act.
        chromeAndroidTask.clearActivityScopedObjects();

        // Assert.
        verify(mockTabModel, times(1)).removeObserver(chromeAndroidTask);
    }

    @Test
    public void clearActivityScopedObjects_clearsRef() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Act.
        chromeAndroidTask.clearActivityScopedObjects();

        // Assert.
        assertNull(chromeAndroidTask.getActivityScopedObjectsForTesting());
    }

    @Test
    public void addFeature_addsFeatureToInternalFeatureList() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        var mockFeature1 = mock(ChromeAndroidTaskFeature.class);
        var mockFeature2 = mock(ChromeAndroidTaskFeature.class);

        // Act.
        chromeAndroidTask.addFeature(mockFeature1);
        chromeAndroidTask.addFeature(mockFeature2);

        // Assert.
        assertEquals(
                chromeAndroidTask.getAllFeaturesForTesting(),
                Arrays.asList(mockFeature1, mockFeature2));
    }

    @Test
    public void addFeature_invokesOnAddedToTaskForFeature() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        var mockFeature = mock(ChromeAndroidTaskFeature.class);

        // Act.
        chromeAndroidTask.addFeature(mockFeature);

        // Assert.
        verify(mockFeature, times(1)).onAddedToTask();
    }

    @Test
    public void addFeature_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        chromeAndroidTask.destroy();

        // Act & Assert.
        var mockFeature = mock(ChromeAndroidTaskFeature.class);
        assertThrows(AssertionError.class, () -> chromeAndroidTask.addFeature(mockFeature));
    }

    @Test
    public void createIntentForNormalBrowserWindow_notIncognito_callsMultiInstanceManager() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var multiInstanceManager =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mMultiInstanceManager;

        // Act.
        var intent = chromeAndroidTask.createIntentForNormalBrowserWindow(/* isIncognito= */ false);

        // Assert.
        assertNotNull(intent);
        verify(multiInstanceManager, times(1)).createNewWindowIntent(/* isIncognito= */ false);
    }

    @Test
    public void createIntentForNormalBrowserWindow_incognito_callsMultiInstanceManager() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var multiInstanceManager =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mMultiInstanceManager;

        // Act.
        var intent = chromeAndroidTask.createIntentForNormalBrowserWindow(/* isIncognito= */ true);

        // Assert.
        assertNotNull(intent);
        verify(multiInstanceManager, times(1)).createNewWindowIntent(/* isIncognito= */ true);
    }

    @Test
    public void getOrCreateNativeBrowserWindowPtr_returnsPtrValueForAliveTask() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Act.
        long nativeBrowserWindowPtr = chromeAndroidTask.getOrCreateNativeBrowserWindowPtr();

        // Assert.
        assertEquals(
                ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR,
                nativeBrowserWindowPtr);
    }

    @Test
    public void getOrCreateNativeBrowserWindowPtr_returnsPtrValueForPendingTask() {
        // Arrange.
        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();
        var pendingTaskInfo = ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo();
        var chromeAndroidTask = new ChromeAndroidTaskImpl(pendingTaskInfo);

        // Act.
        long nativeBrowserWindowPtr = chromeAndroidTask.getOrCreateNativeBrowserWindowPtr();

        // Assert.
        assertEquals(
                ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR,
                nativeBrowserWindowPtr);
    }

    @Test
    public void getOrCreateNativeBrowserWindowPtr_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        chromeAndroidTask.destroy();

        // Act & Assert.
        assertThrows(AssertionError.class, chromeAndroidTask::getOrCreateNativeBrowserWindowPtr);
    }

    @Test
    public void destroy_clearsActivityWindowAndroid() {
        // Arrange.
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl)
                        createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        assertNull(chromeAndroidTask.getActivityScopedObjectsForTesting());
    }

    @Test
    public void destroy_destroysAllFeatures() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        var mockFeature1 = mock(ChromeAndroidTaskFeature.class);
        var mockFeature2 = mock(ChromeAndroidTaskFeature.class);
        chromeAndroidTask.addFeature(mockFeature1);
        chromeAndroidTask.addFeature(mockFeature2);

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        assertTrue(chromeAndroidTask.getAllFeaturesForTesting().isEmpty());
        verify(mockFeature1, times(1)).onTaskRemoved();
        verify(mockFeature2, times(1)).onTaskRemoved();
    }

    @Test
    public void destroy_destroysAndroidBrowserWindow() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockAndroidBrowserWindowNatives =
                assertNonNull(chromeAndroidTaskWithMockDeps.mMockAndroidBrowserWindowNatives);
        long nativeAndroidBrowserWindowPtr = chromeAndroidTask.getOrCreateNativeBrowserWindowPtr();

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        verify(mockAndroidBrowserWindowNatives, times(1)).destroy(nativeAndroidBrowserWindowPtr);
    }

    @Test
    public void destroy_setsStateToDestroyed() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        assertFalse(chromeAndroidTask.isDestroyed());

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        assertTrue(chromeAndroidTask.isDestroyed());
    }

    /**
     * Verifies that {@link ChromeAndroidTask#destroy} uses the {@code DESTROYING} state to block
     * access to APIs that should only be called when the Task is alive.
     */
    @Test
    public void destroy_blocksAccessToApisThatShouldOnlyBeCalledWhenAlive() {
        // Arrange:
        //
        // Set up a mock ChromeAndroidTaskFeature that calls ChromeAndroidTask#addFeature() in
        // its onTaskRemoved() method. No feature should do this in production, but there is nothing
        // preventing this at compile time. Besides ChromeAndroidTask#addFeature(), the feature
        // could also call other ChromeAndroidTask APIs that require the Task state to be "ALIVE".
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        var mockFeature = mock(ChromeAndroidTaskFeature.class);
        doAnswer(
                        invocation -> {
                            chromeAndroidTask.addFeature(mockFeature);
                            return null;
                        })
                .when(mockFeature)
                .onTaskRemoved();
        chromeAndroidTask.addFeature(mockFeature);

        // Act & Assert.
        assertThrows(AssertionError.class, chromeAndroidTask::destroy);
    }

    @Test
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void onConfigurationChanged_windowBoundsChanged_invokesOnTaskBoundsChangedForFeature() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        var mockFeature = mock(ChromeAndroidTaskFeature.class);
        chromeAndroidTask.addFeature(mockFeature);

        var mockWindowManager =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockWindowManager;
        var mockWindowMetrics = mock(WindowMetrics.class);
        var taskBounds1 = new Rect(0, 0, 800, 600);
        var taskBounds2 = new Rect(0, 0, 1920, 1080);
        when(mockWindowMetrics.getBounds()).thenReturn(taskBounds1, taskBounds2);
        when(mockWindowManager.getCurrentWindowMetrics()).thenReturn(mockWindowMetrics);

        // Act.
        chromeAndroidTask.onConfigurationChanged(new Configuration());
        chromeAndroidTask.onConfigurationChanged(new Configuration());

        // Assert.
        var inOrder = inOrder(mockFeature);
        inOrder.verify(mockFeature).onTaskBoundsChanged(taskBounds1);
        inOrder.verify(mockFeature).onTaskBoundsChanged(taskBounds2);
    }

    @Test
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void
            onConfigurationChanged_windowBoundsDoesNotChangeInPxOrDp_doesNotInvokeOnTaskBoundsChangedForFeature() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        float dipScale = 2.0f;
        var mockDisplayAndroid =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockDisplayAndroid;
        when(mockDisplayAndroid.getDipScale()).thenReturn(dipScale);

        var mockFeature = mock(ChromeAndroidTaskFeature.class);
        chromeAndroidTask.addFeature(mockFeature);

        var mockWindowManager =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockWindowManager;
        var mockWindowMetrics = mock(WindowMetrics.class);
        var taskBoundsInPx = new Rect(0, 0, 800, 600);
        when(mockWindowMetrics.getBounds()).thenReturn(taskBoundsInPx);
        when(mockWindowManager.getCurrentWindowMetrics()).thenReturn(mockWindowMetrics);

        // Act.
        chromeAndroidTask.onConfigurationChanged(new Configuration());
        chromeAndroidTask.onConfigurationChanged(new Configuration());

        // Assert:
        // Only the first onConfigurationChanged() should trigger onTaskBoundsChanged() as the
        // second onConfigurationChanged() doesn't include a change in window bounds.
        verify(mockFeature, times(1))
                .onTaskBoundsChanged(
                        DisplayUtil.scaleToEnclosingRect(taskBoundsInPx, 1.0f / dipScale));
    }

    @Test
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void
            onConfigurationChanged_windowBoundsChangesInPxButNotInDp_doesNotInvokeOnTaskBoundsChangedForFeature() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        var mockFeature = mock(ChromeAndroidTaskFeature.class);
        chromeAndroidTask.addFeature(mockFeature);

        var mockWindowManager =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockWindowManager;
        var mockWindowMetrics = mock(WindowMetrics.class);

        float dipScale1 = 1.0f;
        float dipScale2 = 2.0f;
        var mockDisplayAndroid =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockDisplayAndroid;
        when(mockDisplayAndroid.getDipScale()).thenReturn(dipScale1, dipScale2);

        var taskBoundsInPx1 = new Rect(0, 0, 800, 600);
        var taskBoundsInPx2 = DisplayUtil.scaleToEnclosingRect(taskBoundsInPx1, dipScale2);

        when(mockWindowMetrics.getBounds()).thenReturn(taskBoundsInPx1, taskBoundsInPx2);
        when(mockWindowManager.getCurrentWindowMetrics()).thenReturn(mockWindowMetrics);

        // Act.
        chromeAndroidTask.onConfigurationChanged(new Configuration());
        chromeAndroidTask.onConfigurationChanged(new Configuration());

        // Assert:
        // Only the first onConfigurationChanged() should trigger onTaskBoundsChanged() as the
        // second onConfigurationChanged() doesn't include a DP change in window bounds.
        verify(mockFeature, times(1))
                .onTaskBoundsChanged(
                        DisplayUtil.scaleToEnclosingRect(taskBoundsInPx1, 1.0f / dipScale1));
    }

    @Test
    public void onTopResumedActivityChanged_activityIsTopResumed_updatesLastActivatedTime() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        long elapsedRealTime = TimeUtils.elapsedRealtimeMillis();

        // Act.
        chromeAndroidTask.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);

        // Assert.
        assertEquals(elapsedRealTime, chromeAndroidTask.getLastActivatedTimeMillis());
    }

    @Test
    public void
            onTopResumedActivityChanged_activityIsNotTopResumed_doesNotUpdateLastActivatedTime() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        long elapsedRealTime1 = TimeUtils.elapsedRealtimeMillis();

        chromeAndroidTask.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        mFakeTimeTestRule.advanceMillis(100L);

        // Act.
        chromeAndroidTask.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ false);

        // Assert.
        assertEquals(elapsedRealTime1, chromeAndroidTask.getLastActivatedTimeMillis());
    }

    @Test
    public void onTopResumedActivityChanged_invokesOnTaskFocusChangedForFeature() {
        // Arrange.
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl)
                        createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        var mockFeature = mock(ChromeAndroidTaskFeature.class);
        chromeAndroidTask.addFeature(mockFeature);

        // Act.
        chromeAndroidTask.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        chromeAndroidTask.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ false);

        // Assert.
        InOrder inOrder = inOrder(mockFeature);
        inOrder.verify(mockFeature).onTaskFocusChanged(true);
        inOrder.verify(mockFeature).onTaskFocusChanged(false);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void maximize_maximizeToMaximizedBounds() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Act.
        chromeAndroidTask.maximize();

        // Assert.
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());

        var capturedBounds = boundsCaptor.getValue();
        assertEquals(
                "Not moving to target bound",
                DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX,
                capturedBounds);
    }

    @Test
    public void isMaximized_falseWhenNotMaximized() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;

        // Act & Assert.
        assertFalse(chromeAndroidTask.isMaximized());
    }

    @Test
    public void getBoundsInDp_convertsBoundsInPxToDp() {
        // Arrange: ChromeAndroidTask
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Arrange: scaling factor
        float dipScale = 2.0f;
        var mockDisplayAndroid =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockDisplayAndroid;
        when(mockDisplayAndroid.getDipScale()).thenReturn(dipScale);

        // Act
        Rect boundsInDp = chromeAndroidTask.getBoundsInDp();

        // Assert
        Rect expectedBoundsInDp =
                DisplayUtil.scaleToEnclosingRect(
                        DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX, 1.0f / dipScale);
        assertEquals(expectedBoundsInDp, boundsInDp);
    }

    @Test
    public void getRestoredBoundsInDp_convertsBoundsInPxToDp() {
        // 1. Arrange: Create a ChromeAndroidTask with mock dependencies.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockDisplayAndroid =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockDisplayAndroid;

        // 2. Arrange: scaling factor
        float dipScale = 2.0f;
        when(mockDisplayAndroid.getDipScale()).thenReturn(dipScale);

        // 3. Arrange: check the default test setup.
        assertFalse("Task shouldn't be minimized", chromeAndroidTask.isMinimized());
        assertFalse("Task shouldn't be maximized", chromeAndroidTask.isMaximized());
        assertFalse("Task shouldn't be full-screen", chromeAndroidTask.isFullscreen());

        // 4. Arrange: Call maximize(). This should set mRestoredBounds to the current bounds.
        chromeAndroidTask.maximize();

        // 5. Act
        Rect restoredBoundsInDp = chromeAndroidTask.getRestoredBoundsInDp();

        // 6. Assert
        Rect expectedBoundsInDp =
                DisplayUtil.scaleToEnclosingRect(
                        DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX, 1.0f / dipScale);
        assertEquals(
                "restored bounds should be the current bounds in dp",
                expectedBoundsInDp,
                restoredBoundsInDp);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void setBoundsInDp_setsNewBoundsInPx() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var displayAndroid =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockDisplayAndroid;
        float dipScale = 2.0f;
        when(displayAndroid.getDipScale()).thenReturn(dipScale);

        // Act.
        Rect newBoundsInDp =
                DisplayUtil.scaleToEnclosingRect(
                        DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX, 1.0f / dipScale);
        newBoundsInDp.offset(/* dx= */ 10, /* dy= */ 10);
        chromeAndroidTask.setBoundsInDp(newBoundsInDp);

        // Assert.
        Rect expectedNewBoundsInPx = DisplayUtil.scaleToEnclosingRect(newBoundsInDp, dipScale);
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());
        assertEquals(
                "Bounds passed to moveTaskToWithPromise() should be in pixels",
                expectedNewBoundsInPx,
                boundsCaptor.getValue());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void setBoundsInDp_clampsBoundsThatAreTooLarge() {
        // Arrange
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var displayAndroid =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockDisplayAndroid;
        float dipScale = 2.0f;
        when(displayAndroid.getDipScale()).thenReturn(dipScale);

        // Act: set new bounds that are larger than the maximized bounds.
        Rect newBoundsInDp =
                DisplayUtil.scaleToEnclosingRect(
                        DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX, 1.0f / dipScale);
        newBoundsInDp.offset(/* dx= */ 0, /* dy= */ 500);
        chromeAndroidTask.setBoundsInDp(newBoundsInDp);

        // Assert
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());
        assertEquals(
                "Bounds that are too large should be clamped to the maximized bounds",
                DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX,
                boundsCaptor.getValue());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void setBoundsInDp_clampsBoundsThatAreTooSmall() {
        // Arrange
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var displayAndroid =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockDisplayAndroid;
        float dipScale = 2.0f;
        when(displayAndroid.getDipScale()).thenReturn(dipScale);

        // Act: set new bounds that are smaller than the minimum size.
        Rect maxBoundsInDp =
                DisplayUtil.scaleToEnclosingRect(
                        DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX, 1.0f / dipScale);
        Rect newBoundsInDp =
                new Rect(
                        maxBoundsInDp.centerX(),
                        maxBoundsInDp.centerY(),
                        /* right= */ maxBoundsInDp.centerX()
                                + ChromeAndroidTaskBoundsConstraints.MINIMAL_TASK_SIZE_DP
                                - 10,
                        /* bottom= */ maxBoundsInDp.centerY()
                                + ChromeAndroidTaskBoundsConstraints.MINIMAL_TASK_SIZE_DP
                                - 10);
        chromeAndroidTask.setBoundsInDp(newBoundsInDp);

        // Assert
        Rect expectedBoundsInDp =
                new Rect(
                        maxBoundsInDp.centerX(),
                        maxBoundsInDp.centerY(),
                        /* right= */ maxBoundsInDp.centerX()
                                + ChromeAndroidTaskBoundsConstraints.MINIMAL_TASK_SIZE_DP,
                        /* bottom= */ maxBoundsInDp.centerY()
                                + ChromeAndroidTaskBoundsConstraints.MINIMAL_TASK_SIZE_DP);
        Rect expectedBoundsInPx = DisplayUtil.scaleToEnclosingRect(expectedBoundsInDp, dipScale);
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());
        assertEquals(
                "Bounds that are too small should be adjusted to the minimum size",
                expectedBoundsInPx,
                boundsCaptor.getValue());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void restore_restoresToPreviousBounds() {
        // Arrange
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Check the default test setup.
        assertFalse("Task shouldn't be minimized", chromeAndroidTask.isMinimized());
        assertFalse("Task shouldn't be maximized", chromeAndroidTask.isMaximized());
        assertFalse("Task shouldn't be fullscreen", chromeAndroidTask.isFullscreen());

        // Call maximize(). This should set mRestoredBounds to the current bounds.
        chromeAndroidTask.maximize();

        assertEquals(
                "restored bounds should be set to the current bounds",
                DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX,
                chromeAndroidTask.getRestoredBoundsInPxForTesting());

        // Act
        chromeAndroidTask.restore();

        // Assert
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate, times(2))
                .moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());

        var capturedBounds = boundsCaptor.getValue();
        assertEquals(
                "Not moving to target bound", DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX, capturedBounds);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void restore_whenMinimized_activateFirstBeforeSettingBounds() {
        // Arrange
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockActivity = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockActivity;
        var mockActivityManager =
                (ActivityManager) mockActivity.getSystemService(Context.ACTIVITY_SERVICE);
        int taskId = mockActivity.getTaskId();

        // Check the default test setup.
        assertFalse("Task shouldn't be minimized", chromeAndroidTask.isMinimized());
        assertFalse("Task shouldn't be maximized", chromeAndroidTask.isMaximized());
        assertFalse("Task shouldn't be fullscreen", chromeAndroidTask.isFullscreen());

        // Minimize the task.

        chromeAndroidTask.minimize();
        assertEquals(
                DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX,
                chromeAndroidTask.getRestoredBoundsInPxForTesting());

        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        assertTrue("Task should be minimized", chromeAndroidTask.isMinimized());

        // Act.
        chromeAndroidTask.restore();

        // Assert.
        // We need to verify the order of calls: moveTaskToFront then moveTaskTo.
        InOrder inOrder = inOrder(mockActivityManager, apiDelegate);

        // Verify moveTaskToFront is called.
        inOrder.verify(mockActivityManager).moveTaskToFront(taskId, 0);

        // Verify moveTaskTo is called with the restored bounds.
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        inOrder.verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());
        assertEquals(
                "moveTaskTo should be called with the restored bounds",
                DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX,
                boundsCaptor.getValue());
    }

    @Test
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void minimize_alreadyMinimized_doesNotMinimizeAgain() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activity = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockActivity;

        // Mock isMinimized() to return true.
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.STOPPED);
        assertTrue("Task is minimized", chromeAndroidTask.isMinimized());

        // Act.
        chromeAndroidTask.minimize();

        // Assert.
        verify(activity, never().description("Not minimize a task which has been minimized"))
                .moveTaskToBack(anyBoolean());
    }

    @Test
    public void show_whenPendingCreate_enqueuesShowDoesNothing() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act.
        task.show();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(
                "The task defaults to be visible and so show becomes a no-op",
                PendingAction.NONE,
                pendingActions[0]);
    }

    @Test
    public void show_whenPendingUpdate_ignoresRedundantCall() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(false);
        var mockActivity = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockActivity;
        var mockActivityManager =
                (ActivityManager) mockActivity.getSystemService(Context.ACTIVITY_SERVICE);
        assertTrue("Set up task to be visible", chromeAndroidTask.isVisible());
        assertFalse("Set up task to be inactive", chromeAndroidTask.isActive());

        // Act.
        chromeAndroidTask.show();
        assertEquals(
                "Show should be pending after #show is triggered",
                true,
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isActiveFuture(chromeAndroidTask.getState()));
        assertTrue("isActive is true while pending", chromeAndroidTask.isActive());

        chromeAndroidTask.show();

        // Assert
        verify(
                        mockActivityManager,
                        times(1).description("Redundant calls to #show should be ignored"))
                .moveTaskToFront(anyInt(), anyInt());
    }

    @Test
    public void showInactive_whenPendingCreate_enqueuesPendingAction() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act.
        task.showInactive();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.SHOW_INACTIVE, pendingActions[1]);
    }

    @Test
    public void showInactive_whenPendingUpdate_isActiveReturnsFalse() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(true);
        Assert.assertTrue("Set up task to be active", chromeAndroidTask.isActive());

        // Act.
        chromeAndroidTask.showInactive();
        assertEquals(
                "Future state of isActive() should be false when showInactive() is pending",
                false,
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isActiveFuture(chromeAndroidTask.getState()));
        assertFalse("isActive is false while pending", chromeAndroidTask.isActive());
        chromeAndroidTask.onTopResumedActivityChangedWithNative(false);
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(false);

        // Assert
        Assert.assertNull(
                "Future state of showInactive() should be cleared when showInactive() is completed",
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isActiveFuture(chromeAndroidTask.getState()));
        assertFalse(chromeAndroidTask.isActive());
    }

    @Test
    public void close_whenPendingCreate_enqueuesPendingAction() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act.
        task.close();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.CLOSE, pendingActions[0]);
    }

    @Test
    public void activate_whenPendingCreate_enqueuesActivateDoesNothing() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act.
        task.activate();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(
                "The task defaults to be active and so activate becomes a no-op",
                PendingAction.NONE,
                pendingActions[0]);
    }

    @Test
    public void activate_whenPendingUpdate_ignoresRedundantCall() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(false);
        var mockActivity = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockActivity;
        var mockActivityManager =
                (ActivityManager) mockActivity.getSystemService(Context.ACTIVITY_SERVICE);

        // Act.
        chromeAndroidTask.activate();
        assertEquals(
                "Activate should be pending after #activate is triggered",
                true,
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isActiveFuture(chromeAndroidTask.getState()));
        assertTrue("isActive is true while pending", chromeAndroidTask.isActive());

        chromeAndroidTask.activate();

        // Assert
        verify(
                        mockActivityManager,
                        times(1).description("Redundant calls to #activate should be ignored"))
                .moveTaskToFront(anyInt(), anyInt());
    }

    @Test
    public void activate_alreadyActive_ignoresRedundantCall() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockActivity = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockActivity;
        var mockActivityManager =
                (ActivityManager) mockActivity.getSystemService(Context.ACTIVITY_SERVICE);
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(false);

        // Act.
        chromeAndroidTask.activate();

        chromeAndroidTask.onTopResumedActivityChangedWithNative(true);
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(true);
        chromeAndroidTask.activate();

        // Assert.
        verify(mockWindowAndroid, times(2).description("Called twice to verify if task is active"))
                .isTopResumedActivity();
        verify(
                        mockActivityManager,
                        times(1).description(
                                        "Redundant #activate should be ignored if task is active"))
                .moveTaskToFront(anyInt(), anyInt());
    }

    @Test
    public void deactivate_whenPendingCreate_enqueuesPendingAction() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act.
        task.deactivate();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.DEACTIVATE, pendingActions[1]);
    }

    @Test
    public void deactivate_whenPendingUpdate_isActiveReturnsFalse() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(true);
        Assert.assertTrue("Set up task to be active", chromeAndroidTask.isActive());

        // Act.
        chromeAndroidTask.deactivate();
        assertEquals(
                "Future state of isActive() should be false when deactivate() is pending",
                false,
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isActiveFuture(chromeAndroidTask.getState()));
        assertFalse("isActive is false while pending", chromeAndroidTask.isActive());
        chromeAndroidTask.onTopResumedActivityChangedWithNative(false);
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(false);

        // Assert
        Assert.assertNull(
                "Future state of isActive() should be cleared when deactivate() is completed",
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isActiveFuture(chromeAndroidTask.getState()));
        assertFalse(chromeAndroidTask.isActive());
    }

    @Test
    public void maximize_whenPendingCreate_enqueuesPendingAction() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act.
        task.maximize();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.MAXIMIZE, pendingActions[0]);
    }

    @Test
    public void maximize_whenPendingUpdate_isMaximizeReturnsTrue() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var promise = new Promise<Pair<Integer, Rect>>();
        when(apiDelegate.moveTaskToWithPromise(any(), anyInt(), any())).thenReturn(promise);

        // Act.
        chromeAndroidTask.maximize();
        assertEquals(
                "Maximize should be pending after #maximize is triggered",
                true,
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isMaximizedFuture(chromeAndroidTask.getState()));
        assertTrue("isMaximized is true while pending", chromeAndroidTask.isMaximized());
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), any());
        assertEquals(
                "isMaximized returns maximized bounds while pending",
                ChromeAndroidTaskImpl.convertBoundsInPxToDp(
                        DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX, mockWindowAndroid.getDisplay()),
                chromeAndroidTask.getBoundsInDp());
        promise.fulfill(Pair.create(0, DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX));
        shadowOf(getMainLooper()).idle();

        // Assert
        Assert.assertNull(
                "Maximize should be not pending after #maximize is finished",
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isMaximizedFuture(chromeAndroidTask.getState()));
    }

    @Test
    public void minimize_whenPendingCreate_enqueuesPendingAction() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act.
        task.minimize();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.MINIMIZE, pendingActions[0]);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void restore_whenPendingCreate_enqueuesPendingAction() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act.
        task.restore();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.RESTORE, pendingActions[0]);
    }

    @Test
    public void restore_whenPendingUpdate_restoreReturnsCorrectBounds() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var promise = new Promise<Pair<Integer, Rect>>();
        when(apiDelegate.moveTaskToWithPromise(any(), anyInt(), any())).thenReturn(promise);

        // Act.
        chromeAndroidTask.maximize();
        promise.fulfill(Pair.create(0, DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX));
        shadowOf(getMainLooper()).idle();

        promise = new Promise<>();
        when(apiDelegate.moveTaskToWithPromise(any(), anyInt(), any())).thenReturn(promise);
        chromeAndroidTask.restore();

        Assert.assertEquals(
                "Should return pending bounds when restore is in progress",
                ChromeAndroidTaskImpl.convertBoundsInPxToDp(
                        DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX, mockWindowAndroid.getDisplay()),
                chromeAndroidTask.getBoundsInDp());

        promise.fulfill(Pair.create(0, DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX));
        shadowOf(getMainLooper()).idle();

        // Assert
        Assert.assertNull(
                "Restore should be not pending after #restore is finished",
                chromeAndroidTask.getPendingActionManagerForTesting().getFutureBoundsInDp());
    }

    @Test
    public void setBounds_whenPendingCreate_nonEmptyBounds_enqueuesPendingAction() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());
        var taskBounds = new Rect(0, 0, 800, 600);

        // Act.
        task.setBoundsInDp(taskBounds);

        // Assert.
        var pendingActionManager = task.getPendingActionManagerForTesting();
        assertEquals(
                PendingAction.SET_BOUNDS, pendingActionManager.getPendingActionsForTesting()[0]);
        assertEquals(taskBounds, pendingActionManager.getPendingBoundsInDp());
    }

    @Test
    public void setBounds_whenPendingCreate_emptyBounds_ignoresPendingAction() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act.
        task.setBoundsInDp(new Rect());

        // Assert.
        var pendingActionManager = task.getPendingActionManagerForTesting();
        assertEquals(PendingAction.NONE, pendingActionManager.getPendingActionsForTesting()[0]);
        assertNull(pendingActionManager.getPendingBoundsInDp());
    }

    @Test
    public void isActive_whenPendingCreate_withNoPendingShowOrActivate_returnsTrue() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act and Assert.
        assertTrue("Task defaults to be active when created", task.isActive());
    }

    @Test
    public void isActive_whenPendingCreate_withPendingShow_returnsTrue() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());
        // Request SHOW in pending state.
        task.show();

        // Act and Assert.
        assertTrue(task.isActive());
    }

    @Test
    public void isActive_whenPendingCreate_withPendingActivate_returnsTrue() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());
        // Request ACTIVATE in pending state.
        task.activate();

        // Act and Assert.
        assertTrue(task.isActive());
    }

    @Test
    public void isActive_whenPendingUpdate_withPendingActivate_returnsTrue() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(false);

        // Act.
        chromeAndroidTask.activate();
        Assert.assertTrue(
                "Activate should be pending after #activate is triggered",
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isActiveFuture(chromeAndroidTask.getState()));
        assertTrue("isActive is true while pending", chromeAndroidTask.isActive());

        chromeAndroidTask.onTopResumedActivityChangedWithNative(true);
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(true);

        // Assert
        Assert.assertNull(
                "Activate should be pending after #activate is triggered",
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isActiveFuture(chromeAndroidTask.getState()));
        assertTrue(chromeAndroidTask.isActive());
    }

    @Test
    public void isMaximized_whenPendingCreate_withPendingMaximize_returnsTrue() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());
        // Request MAXIMIZE in pending state.
        task.maximize();

        // Act and Assert.
        assertTrue(task.isMaximized());
    }

    @Test
    public void isMaximized_whenPendingCreate_withMaximizedStateInCreateParams_returnsTrue() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, new Rect(), WindowShowState.MAXIMIZED);
        var task =
                new ChromeAndroidTaskImpl(
                        ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo(mockParams));

        // Act and Assert.
        assertTrue(task.isMaximized());
    }

    @Test
    public void
            isMaximized_whenPendingCreate_withDefaultStateInCreateParams_withoutPendingMaximize_returnsFalse() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act and Assert.
        assertFalse(task.isMaximized());
    }

    @Test
    public void isMinimized_whenPendingCreate_withPendingMinimize_returnsTrue() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Request MINIMIZE in pending state.
        task.minimize();

        // Act and Assert.
        assertTrue(task.isMinimized());
    }

    @Test
    public void isMinimized_whenPendingCreate_withMinimizedStateInCreateParams_returnsTrue() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, new Rect(), WindowShowState.MINIMIZED);
        var task =
                new ChromeAndroidTaskImpl(
                        ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo(mockParams));

        // Act and Assert.
        assertTrue(task.isMinimized());
    }

    @Test
    public void
            isMinimized_whenPendingCreate_withDefaultStateInCreateParams_withoutPendingMinimize_returnsFalse() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act and Assert.
        assertFalse(task.isMinimized());
    }

    @Test
    public void isMinimized_whenPendingUpdate_withPendingMinimize_returnsTrue() {
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        chromeAndroidTask.onTopResumedActivityChangedWithNative(true);
        chromeAndroidTask.onTaskVisibilityChanged(1, true);

        // Act.
        chromeAndroidTask.minimize();
        assertEquals(
                "Future state of isVisible() should be false when minimize() is pending",
                false,
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isVisibleFuture(chromeAndroidTask.getState()));
        assertTrue(
                "isMinimized() should be true when minimize() is pending",
                chromeAndroidTask.isMinimized());
        chromeAndroidTask.onTopResumedActivityChangedWithNative(false);
        chromeAndroidTask.onTaskVisibilityChanged(1, false);

        // Assert.
        assertNull(
                "Future state of isVisible() should be cleared after minimized() is completed",
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isVisibleFuture(chromeAndroidTask.getState()));
        assertFalse(chromeAndroidTask.isMinimized());
    }

    @Test
    public void isFullscreen_whenPendingCreate_returnsFalse() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act and Assert.
        assertFalse(task.isFullscreen());
    }

    @Test
    public void getRestoredBoundsInDp_whenPendingCreate_withNonEmptyBounds_returnsPendingBounds() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Request SET_BOUNDS in pending state.
        var bounds = new Rect(100, 100, 600, 800);
        task.setBoundsInDp(bounds);

        // Act and Assert.
        assertEquals(bounds, task.getRestoredBoundsInDp());
    }

    @Test
    public void
            getRestoredBoundsInDp_whenPendingCreate_withNonEmptyRestoredBounds_returnsPendingRestoredBounds() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Request SET_BOUNDS, MAXIMIZE, and RESTORE in pending state.
        var bounds = new Rect(100, 100, 600, 800);
        task.setBoundsInDp(bounds);
        task.maximize();
        task.restore();

        // Act and Assert.
        assertEquals(bounds, task.getRestoredBoundsInDp());
    }

    @Test
    public void
            getRestoredBoundsInDp_whenPendingCreate_withoutPendingRestoredBounds_returnsInitialBounds() {
        // Arrange.
        var bounds = new Rect(100, 100, 600, 800);
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, bounds, WindowShowState.DEFAULT);
        var task =
                new ChromeAndroidTaskImpl(
                        ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo(mockParams));

        // Act: Request RESTORE in pending state.
        task.restore();

        // Assert.
        assertEquals(bounds, task.getRestoredBoundsInDp());
    }

    @Test
    public void
            getRestoredBoundsInDp_whenPendingCreate_withNonEmptyInitialBoundsInCreateParams_returnsInitialBounds() {
        // Arrange.
        var bounds = new Rect(100, 100, 600, 800);
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, bounds, WindowShowState.DEFAULT);
        var task =
                new ChromeAndroidTaskImpl(
                        ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo(mockParams));

        // Act and Assert.
        assertEquals(bounds, task.getRestoredBoundsInDp());
    }

    @Test
    public void
            getRestoredBoundsInDp_whenPendingCreate_withDefaultStateInCreateParams_withoutPendingSetBounds_returnsEmptyRect() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act and Assert.
        assertTrue(task.getRestoredBoundsInDp().isEmpty());
    }

    @Test
    public void getBoundsInDp_whenPendingCreate_withPendingSetBounds_returnsPendingBounds() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Request SET_BOUNDS in pending state.
        var bounds = new Rect(100, 100, 600, 800);
        task.setBoundsInDp(bounds);

        // Act and Assert.
        assertEquals(bounds, task.getBoundsInDp());
    }

    @Test
    public void
            getBoundsInDp_whenPendingCreate_withNonEmptyInitialBoundsInCreateParams_returnsInitialBounds() {
        // Arrange.
        var bounds = new Rect(100, 100, 600, 800);
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, bounds, WindowShowState.DEFAULT);
        var task =
                new ChromeAndroidTaskImpl(
                        ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo(mockParams));

        // Act and Assert.
        assertEquals(bounds, task.getBoundsInDp());
    }

    @Test
    public void
            getBoundsInDp_whenPendingCreate_withDefaultStateInCreateParams_withoutPendingSetBounds_returnsEmptyRect() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act and Assert.
        assertTrue(task.getBoundsInDp().isEmpty());
    }

    @Test
    public void isVisible_whenPendingCreate_withMinimizedStateInCreateParams_returnsFalse() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, new Rect(), WindowShowState.MINIMIZED);
        var task =
                new ChromeAndroidTaskImpl(
                        ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo(mockParams));

        // Act and Assert.
        assertFalse(task.isVisible());
    }

    @Test
    public void isVisible_whenPendingCreate_withNonMinimizedStateInCreateParams_returnsTrue() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act and Assert.
        assertTrue(task.isVisible());
    }

    @Test
    public void isVisible_whenPendingUpdate_withPendingShow_returnsTrue() {
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Act.
        chromeAndroidTask.show();
        assertEquals(
                "Future state of isVisible() should be true when show() is pending",
                true,
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isVisibleFuture(chromeAndroidTask.getState()));

        chromeAndroidTask.onTopResumedActivityChangedWithNative(true);

        // Assert.
        assertNull(
                "Future state of isVisible() should be cleared after show() is completed",
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isVisibleFuture(chromeAndroidTask.getState()));
        assertTrue(chromeAndroidTask.isVisible());
    }

    @Test
    public void isVisible_whenPendingUpdate_withPendingMinimize_returnsFalse() {
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockActivity = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockActivity;

        // Act.
        chromeAndroidTask.minimize();
        assertEquals(
                "Future state of isVisible() should be false when minimize() is pending",
                false,
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isVisibleFuture(chromeAndroidTask.getState()));

        chromeAndroidTask.onTaskVisibilityChanged(/* taskId= */ 1, /* isVisible= */ false);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);

        // Assert.
        assertNull(
                "Future state of isVisible() should be cleared when minimize() is completed",
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isVisibleFuture(chromeAndroidTask.getState()));
        assertTrue("Should be minimized", chromeAndroidTask.isMinimized());
        assertFalse("Should not be visible", chromeAndroidTask.isVisible());
    }

    @Test
    public void setActivityScopedObjects_fromPendingState_NoPendingShowToDispatch() {
        int taskId = 2;

        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(taskId, /* isPendingTask= */ true);
        var pendingTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Arrange: Request SHOW on a pending task.
        pendingTask.show();

        // Arrange: Set up ActivityScopedObjects.
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var mockActivity = activityScopedObjects.mActivityWindowAndroid.getActivity().get();
        var mockActivityManager =
                (ActivityManager) mockActivity.getSystemService(Context.ACTIVITY_SERVICE);

        // Act.
        pendingTask.setActivityScopedObjects(activityScopedObjects);
        pendingTask.onNativeInitializationFinished();

        // Assert.
        verify(mockActivityManager, never().description("The task defaults to be visible"))
                .moveTaskToFront(taskId, 0);
    }

    @Test
    public void setActivityScopedObjects_fromPendingState_dispatchesPendingClose() {
        int taskId = 2;

        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(taskId, /* isPendingTask= */ true);
        var pendingTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Arrange: Request CLOSE on a pending task.
        pendingTask.close();

        // Arrange: Set up ActivityScopedObjects.
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var mockActivity = activityScopedObjects.mActivityWindowAndroid.getActivity().get();

        // Act.
        pendingTask.setActivityScopedObjects(activityScopedObjects);
        pendingTask.onNativeInitializationFinished();

        // Assert.
        verify(mockActivity).finishAndRemoveTask();
    }

    @Test
    public void setActivityScopedObjects_fromPendingState_NoPendingActivateToDispatch() {
        int taskId = 2;

        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(taskId, /* isPendingTask= */ true);
        var pendingTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Arrange: Request ACTIVATE on a pending task.
        pendingTask.activate();

        // Arrange: Set up ActivityScopedObjects.
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var mockActivity = activityScopedObjects.mActivityWindowAndroid.getActivity().get();

        // Act.
        pendingTask.setActivityScopedObjects(activityScopedObjects);
        pendingTask.onNativeInitializationFinished();

        // Assert.
        verify(mockActivity, never().description("The task defaults to be active"))
                .moveTaskToBack(true);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void setActivityScopedObjects_fromPendingState_dispatchesPendingMaximize() {
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        // Arrange: Request MAXIMIZE on a pending task.
        chromeAndroidTask.maximize();

        // Act.
        chromeAndroidTask.setActivityScopedObjects(
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects);
        chromeAndroidTask.onNativeInitializationFinished();

        // Assert.
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());
        var capturedBounds = boundsCaptor.getValue();
        assertEquals(DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX, capturedBounds);
    }

    @Test
    public void setActivityScopedObjects_fromPendingState_dispatchesPendingMinimize() {
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        // Arrange: Request MINIMIZE on a pending task.
        chromeAndroidTask.minimize();
        // Arrange: Setup ActivityScopedObjects.
        int taskId = 2;
        var activityScopedObjects = createActivityScopedObjects(taskId);
        var mockActivity = activityScopedObjects.mActivityWindowAndroid.getActivity().get();

        // Act.
        chromeAndroidTask.setActivityScopedObjects(activityScopedObjects);
        chromeAndroidTask.onNativeInitializationFinished();

        // Assert.
        verify(mockActivity).moveTaskToBack(true);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void
            setActivityScopedObjects_fromPendingState_withNonEmptyPendingBounds_dispatchesPendingRestore() {
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        // Arrange: Setup display parameters.
        var displayAndroid =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockDisplayAndroid;
        float dipScale = 2.0f;
        when(displayAndroid.getDipScale()).thenReturn(dipScale);
        // Arrange: Sequentially request SET_BOUNDS, MAXIMIZE, and RESTORE on a pending task.
        Rect pendingBoundsInDp = new Rect(10, 20, 800, 600);
        chromeAndroidTask.setBoundsInDp(pendingBoundsInDp);
        chromeAndroidTask.maximize();
        chromeAndroidTask.restore();

        // Act.
        chromeAndroidTask.setActivityScopedObjects(
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects);
        chromeAndroidTask.onNativeInitializationFinished();

        // Assert.
        Rect expectedBoundsInPx = DisplayUtil.scaleToEnclosingRect(pendingBoundsInDp, dipScale);
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());
        assertEquals(expectedBoundsInPx, boundsCaptor.getValue());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void
            setActivityScopedObjects_fromPendingState_withEmptyPendingBounds_ignoresPendingRestore() {
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        // Arrange: Request RESTORE on a pending task.
        chromeAndroidTask.restore();

        // Act.
        chromeAndroidTask.setActivityScopedObjects(
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects);

        // Assert.
        verify(apiDelegate, never()).moveTaskToWithPromise(any(), anyInt(), any());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void setActivityScopedObjects_fromPendingState_dispatchesPendingSetBounds() {
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;

        // Arrange: Setup display parameters.
        var displayAndroid =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockDisplayAndroid;
        float dipScale = 2.0f;
        when(displayAndroid.getDipScale()).thenReturn(dipScale);

        // Arrange: Request SET_BOUNDS on a pending task.
        Rect pendingBoundsInDp =
                DisplayUtil.scaleToEnclosingRect(
                        DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX, 1.0f / dipScale);
        pendingBoundsInDp.offset(/* dx= */ 10, /* dy= */ 10);
        chromeAndroidTask.setBoundsInDp(pendingBoundsInDp);

        // Act.
        chromeAndroidTask.setActivityScopedObjects(
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects);
        chromeAndroidTask.onNativeInitializationFinished();

        // Assert.
        Rect expectedBoundsInPx = DisplayUtil.scaleToEnclosingRect(pendingBoundsInDp, dipScale);
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());
        assertEquals(expectedBoundsInPx, boundsCaptor.getValue());
    }

    @Test
    public void setActivityScopedObjects_fromPendingState_invokesCallback() {
        // Arrange: Create pending task with a callback.
        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();
        var pendingTaskInfo = ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo();
        var task = new ChromeAndroidTaskImpl(pendingTaskInfo);

        // Arrange: Setup ActivityScopedObjects.
        int taskId = 2;
        var activityScopedObjects = createActivityScopedObjects(taskId);

        // Act.
        task.setActivityScopedObjects(activityScopedObjects);
        task.onNativeInitializationFinished();

        // Assert.
        verify(pendingTaskInfo.mTaskCreationCallbackForNative)
                .onResult(ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }
}
