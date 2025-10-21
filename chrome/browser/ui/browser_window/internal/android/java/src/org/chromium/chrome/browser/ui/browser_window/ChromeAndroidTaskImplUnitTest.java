// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
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

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.view.WindowMetrics;

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
import org.chromium.base.JniOnceCallback;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
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
    private static ActivityWindowAndroidMocks createActivityWindowAndroidMocks(int taskId) {
        var activityWindowAndroidMocks =
                ChromeAndroidTaskUnitTestSupport.createActivityWindowAndroidMocks(taskId);
        mockDesktopWindowingMode(activityWindowAndroidMocks);
        return activityWindowAndroidMocks;
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
    public void constructor_withActivity_setsActivityWindowAndroid() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;

        // Assert.
        assertEquals(activityWindowAndroid, chromeAndroidTask.getActivityWindowAndroid());
    }

    @Test
    public void constructor_withActivity_registersActivityLifecycleObservers() {
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
    public void constructor_withActivity_setsTabModelRefAndRegistersTabModelObserver() {
        // Arrange & Act.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockTabModel = chromeAndroidTaskWithMockDeps.mMockTabModel;

        // Assert.
        assertEquals(mockTabModel, chromeAndroidTask.getObservedTabModelForTesting());
        verify(mockTabModel, times(1)).addObserver(chromeAndroidTask);
    }

    @Test
    public void constructor_withCreateParams_pendingState() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();

        // Act.
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Assert.
        assertEquals(mockParams.getWindowType(), task.getBrowserWindowType());
        assertEquals(mockParams.getProfile(), task.getProfile());
        assertEquals(State.PENDING, task.getState());
        assertEquals(1, (int) task.getPendingId());
        assertNull(task.getId());
        assertNull(task.getActivityWindowAndroidForTesting());
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
    public void setActivityWindowAndroid_refAlreadyExists_throwsException() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask = createChromeAndroidTaskWithMockDeps(taskId).mChromeAndroidTask;
        var newActivityWindowAndroid =
                createActivityWindowAndroidMocks(taskId).mMockActivityWindowAndroid;

        // Act & Assert.
        assertThrows(
                AssertionError.class,
                () ->
                        chromeAndroidTask.setActivityWindowAndroid(
                                newActivityWindowAndroid, mock(TabModel.class)));
    }

    @Test
    public void setActivityWindowAndroid_fromPendingState_setsIdAndState() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
        int taskId = 2;
        var activityWindowAndroid =
                createActivityWindowAndroidMocks(taskId).mMockActivityWindowAndroid;

        // Act.
        task.setActivityWindowAndroid(activityWindowAndroid, mock(TabModel.class));

        // Assert.
        assertEquals(taskId, (int) task.getId());
        assertNull(task.getPendingId());
        assertEquals(activityWindowAndroid, task.getActivityWindowAndroid());
    }

    @Test
    public void setActivityWindowAndroid_previousRefCleared_setsNewRef() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask = createChromeAndroidTaskWithMockDeps(taskId).mChromeAndroidTask;
        var newActivityWindowAndroid =
                createActivityWindowAndroidMocks(taskId).mMockActivityWindowAndroid;
        chromeAndroidTask.clearActivityWindowAndroid();

        // Act.
        chromeAndroidTask.setActivityWindowAndroid(newActivityWindowAndroid, mock(TabModel.class));

        // Assert.
        assertEquals(newActivityWindowAndroid, chromeAndroidTask.getActivityWindowAndroid());
    }

    @Test
    public void
            setActivityWindowAndroid_previousRefCleared_registersNewActivityLifecycleObservers() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask = createChromeAndroidTaskWithMockDeps(taskId).mChromeAndroidTask;
        var newActivityWindowAndroidMocks = createActivityWindowAndroidMocks(taskId);
        chromeAndroidTask.clearActivityWindowAndroid();

        // Act.
        chromeAndroidTask.setActivityWindowAndroid(
                newActivityWindowAndroidMocks.mMockActivityWindowAndroid, mock(TabModel.class));

        // Assert.
        verify(newActivityWindowAndroidMocks.mMockActivityLifecycleDispatcher, times(1))
                .register(isA(TopResumedActivityChangedWithNativeObserver.class));
        verify(newActivityWindowAndroidMocks.mMockActivityLifecycleDispatcher, times(1))
                .register(isA(ConfigurationChangedObserver.class));
    }

    @Test
    public void
            setActivityWindowAndroid_previousRefCleared_setsNewTabModelRefAndRegistersTabModelObserver() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var oldMockTabModel = chromeAndroidTaskWithMockDeps.mMockTabModel;

        var newActivityWindowAndroidMocks = createActivityWindowAndroidMocks(taskId);
        var newMockTabModel = mock(TabModel.class);

        // Act.
        chromeAndroidTask.clearActivityWindowAndroid();
        chromeAndroidTask.setActivityWindowAndroid(
                newActivityWindowAndroidMocks.mMockActivityWindowAndroid, newMockTabModel);

        // Assert.
        // Verify observer was removed from the old TabModel.
        verify(oldMockTabModel, times(1)).removeObserver(chromeAndroidTask);

        // Verify the new TabModel is being observed.
        assertEquals(newMockTabModel, chromeAndroidTask.getObservedTabModelForTesting());
        verify(newMockTabModel, times(1)).addObserver(chromeAndroidTask);
    }

    @Test
    public void
            setActivityWindowAndroid_previousRefCleared_newRefHasDifferentTaskId_throwsException() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        var newActivityWindowAndroid =
                createActivityWindowAndroidMocks(/* taskId= */ 2).mMockActivityWindowAndroid;
        chromeAndroidTask.clearActivityWindowAndroid();

        // Act & Assert.
        assertThrows(
                AssertionError.class,
                () ->
                        chromeAndroidTask.setActivityWindowAndroid(
                                newActivityWindowAndroid, mock(TabModel.class)));
    }

    @Test
    public void setActivityWindowAndroid_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        chromeAndroidTask.destroy();

        // Act & Assert.
        var newActivityWindowAndroid =
                createActivityWindowAndroidMocks(taskId).mMockActivityWindowAndroid;
        assertThrows(
                AssertionError.class,
                () ->
                        chromeAndroidTask.setActivityWindowAndroid(
                                newActivityWindowAndroid, mock(TabModel.class)));
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
    public void clearActivityWindowAndroid_unregistersActivityLifecycleObservers() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockActivityLifecycleDispatcher =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityLifecycleDispatcher;

        // Act.
        chromeAndroidTask.clearActivityWindowAndroid();

        // Assert.
        verify(mockActivityLifecycleDispatcher, times(1))
                .unregister(isA(TopResumedActivityChangedWithNativeObserver.class));
        verify(mockActivityLifecycleDispatcher, times(1))
                .unregister(isA(ConfigurationChangedObserver.class));
    }

    @Test
    public void clearActivityWindowAndroid_unregistersTabModelObserverAndClearTabModelRef() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockTabModel = chromeAndroidTaskWithMockDeps.mMockTabModel;

        // Act.
        chromeAndroidTask.clearActivityWindowAndroid();

        // Assert.
        verify(mockTabModel, times(1)).removeObserver(chromeAndroidTask);
        assertNull(chromeAndroidTask.getObservedTabModelForTesting());
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
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var chromeAndroidTask = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

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
        assertNull(chromeAndroidTask.getActivityWindowAndroidForTesting());
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
        verify(apiDelegate).moveTaskTo(any(), anyInt(), boundsCaptor.capture());

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
        verify(apiDelegate).moveTaskTo(any(), anyInt(), boundsCaptor.capture());
        assertEquals(
                "Bounds passed to moveTaskTo() should be in pixels",
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
        verify(apiDelegate).moveTaskTo(any(), anyInt(), boundsCaptor.capture());
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
        verify(apiDelegate).moveTaskTo(any(), anyInt(), boundsCaptor.capture());
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
        verify(apiDelegate, times(2)).moveTaskTo(any(), anyInt(), boundsCaptor.capture());

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
        inOrder.verify(apiDelegate).moveTaskTo(any(), anyInt(), boundsCaptor.capture());
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
    public void show_whenPending_enqueuesPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act.
        task.show();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.SHOW, pendingActions[0]);
    }

    @Test
    public void showInactive_whenPending_enqueuesPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act.
        task.showInactive();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.SHOW_INACTIVE, pendingActions[1]);
    }

    @Test
    public void close_whenPending_enqueuesPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act.
        task.close();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.CLOSE, pendingActions[0]);
    }

    @Test
    public void activate_whenPending_enqueuesPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act.
        task.activate();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.ACTIVATE, pendingActions[0]);
    }

    @Test
    public void deactivate_whenPending_enqueuesPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act.
        task.deactivate();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.DEACTIVATE, pendingActions[1]);
    }

    @Test
    public void maximize_whenPending_enqueuesPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act.
        task.maximize();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.MAXIMIZE, pendingActions[0]);
    }

    @Test
    public void minimize_whenPending_enqueuesPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act.
        task.minimize();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.MINIMIZE, pendingActions[0]);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void restore_whenPending_enqueuesPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act.
        task.restore();

        // Assert.
        int[] pendingActions =
                task.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(PendingAction.RESTORE, pendingActions[0]);
    }

    @Test
    public void setBounds_whenPending_nonEmptyBounds_enqueuesPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
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
    public void setBounds_whenPending_emptyBounds_ignoresPendingAction() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act.
        task.setBoundsInDp(new Rect());

        // Assert.
        var pendingActionManager = task.getPendingActionManagerForTesting();
        assertEquals(PendingAction.NONE, pendingActionManager.getPendingActionsForTesting()[0]);
        assertNull(pendingActionManager.getPendingBoundsInDp());
    }

    @Test
    public void isActive_whenPending_withNoPendingShowOrActivate_returnsFalse() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertFalse(task.isActive());
    }

    @Test
    public void isActive_whenPending_withPendingShow_returnsTrue() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
        // Request SHOW in pending state.
        task.show();

        // Act and Assert.
        assertTrue(task.isActive());
    }

    @Test
    public void isActive_whenPending_withPendingActivate_returnsTrue() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
        // Request ACTIVATE in pending state.
        task.activate();

        // Act and Assert.
        assertTrue(task.isActive());
    }

    @Test
    public void isMaximized_whenPending_withPendingMaximize_returnsTrue() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
        // Request MAXIMIZE in pending state.
        task.maximize();

        // Act and Assert.
        assertTrue(task.isMaximized());
    }

    @Test
    public void isMaximized_whenPending_withMaximizedStateInCreateParams_returnsTrue() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, new Rect(), WindowShowState.MAXIMIZED);
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertTrue(task.isMaximized());
    }

    @Test
    public void
            isMaximized_whenPending_withDefaultStateInCreateParams_withoutPendingMaximize_returnsFalse() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertFalse(task.isMaximized());
    }

    @Test
    public void isMinimized_whenPending_withPendingMinimize_returnsTrue() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
        // Request MINIMIZE in pending state.
        task.minimize();

        // Act and Assert.
        assertTrue(task.isMinimized());
    }

    @Test
    public void isMinimized_whenPending_withMinimizedStateInCreateParams_returnsTrue() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, new Rect(), WindowShowState.MINIMIZED);
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertTrue(task.isMinimized());
    }

    @Test
    public void
            isMinimized_whenPending_withDefaultStateInCreateParams_withoutPendingMinimize_returnsFalse() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertFalse(task.isMinimized());
    }

    @Test
    public void isFullscreen_whenPending_returnsFalse() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertFalse(task.isFullscreen());
    }

    @Test
    public void getRestoredBoundsInDp_whenPending_withNonEmptyBounds_returnsPendingBounds() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
        // Request SET_BOUNDS in pending state.
        var bounds = new Rect(100, 100, 600, 800);
        task.setBoundsInDp(bounds);

        // Act and Assert.
        assertEquals(bounds, task.getRestoredBoundsInDp());
    }

    @Test
    public void
            getRestoredBoundsInDp_whenPending_withNonEmptyRestoredBounds_returnsPendingRestoredBounds() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
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
            getRestoredBoundsInDp_whenPending_withoutPendingRestoredBounds_returnsInitialBounds() {
        // Arrange.
        var bounds = new Rect(100, 100, 600, 800);
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, bounds, WindowShowState.DEFAULT);
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
        // Request RESTORE in pending state.
        task.restore();

        // Act and Assert.
        assertEquals(bounds, task.getRestoredBoundsInDp());
    }

    @Test
    public void
            getRestoredBoundsInDp_whenPending_withNonEmptyInitialBoundsInCreateParams_returnsInitialBounds() {
        // Arrange.
        var bounds = new Rect(100, 100, 600, 800);
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, bounds, WindowShowState.DEFAULT);
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertEquals(bounds, task.getRestoredBoundsInDp());
    }

    @Test
    public void
            getRestoredBoundsInDp_whenPending_withDefaultStateInCreateParams_withoutPendingSetBounds_returnsEmptyRect() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertTrue(task.getRestoredBoundsInDp().isEmpty());
    }

    @Test
    public void getBoundsInDp_whenPending_withPendingSetBounds_returnsPendingBounds() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
        // Request SET_BOUNDS in pending state.
        var bounds = new Rect(100, 100, 600, 800);
        task.setBoundsInDp(bounds);

        // Act and Assert.
        assertEquals(bounds, task.getBoundsInDp());
    }

    @Test
    public void
            getBoundsInDp_whenPending_withNonEmptyInitialBoundsInCreateParams_returnsInitialBounds() {
        // Arrange.
        var bounds = new Rect(100, 100, 600, 800);
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, bounds, WindowShowState.DEFAULT);
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertEquals(bounds, task.getBoundsInDp());
    }

    @Test
    public void
            getBoundsInDp_whenPending_withDefaultStateInCreateParams_withoutPendingSetBounds_returnsEmptyRect() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertTrue(task.getBoundsInDp().isEmpty());
    }

    @Test
    public void isVisible_whenPending_withMinimizedStateInCreateParams_returnsFalse() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams(
                        BrowserWindowType.NORMAL, new Rect(), WindowShowState.MINIMIZED);
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertFalse(task.isVisible());
    }

    @Test
    public void isVisible_whenPending_withNonMinimizedStateInCreateParams_returnsTrue() {
        // Arrange.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);

        // Act and Assert.
        assertTrue(task.isVisible());
    }

    @Test
    public void setActivityWindowAndroid_fromPendingState_dispatchesPendingShow() {
        // Arrange: Create pending task.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
        // Arrange: Request SHOW on a pending task.
        task.show();
        int taskId = 2;
        // Arrange: Setup WindowAndroid.
        var activityWindowAndroid =
                createActivityWindowAndroidMocks(taskId).mMockActivityWindowAndroid;
        var mockActivity = activityWindowAndroid.getActivity().get();
        var mockActivityManager =
                (ActivityManager) mockActivity.getSystemService(Context.ACTIVITY_SERVICE);

        // Act.
        task.setActivityWindowAndroid(activityWindowAndroid, mock(TabModel.class));

        // Assert.
        verify(mockActivityManager).moveTaskToFront(taskId, 0);
    }

    @Test
    public void setActivityWindowAndroid_fromPendingState_dispatchesPendingClose() {
        // Arrange: Create pending task.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
        // Arrange: Request CLOSE on a pending task.
        task.close();
        int taskId = 2;
        // Arrange: Setup WindowAndroid.
        var activityWindowAndroid =
                createActivityWindowAndroidMocks(taskId).mMockActivityWindowAndroid;
        var mockActivity = activityWindowAndroid.getActivity().get();

        // Act.
        task.setActivityWindowAndroid(activityWindowAndroid, mock(TabModel.class));

        // Assert.
        verify(mockActivity).finishAndRemoveTask();
    }

    @Test
    public void setActivityWindowAndroid_fromPendingState_dispatchesPendingActivate() {
        // Arrange: Create pending task.
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams);
        // Arrange: Request ACTIVATE on a pending task.
        task.activate();
        // Arrange: Setup WindowAndroid.
        int taskId = 2;
        var activityWindowAndroid =
                createActivityWindowAndroidMocks(taskId).mMockActivityWindowAndroid;
        var mockActivity = activityWindowAndroid.getActivity().get();
        var mockActivityManager =
                (ActivityManager) mockActivity.getSystemService(Context.ACTIVITY_SERVICE);

        // Act.
        task.setActivityWindowAndroid(activityWindowAndroid, mock(TabModel.class));

        // Assert.
        verify(mockActivityManager).moveTaskToFront(taskId, 0);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void setActivityWindowAndroid_fromPendingState_dispatchesPendingMaximize() {
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        // Arrange: Request MAXIMIZE on a pending task.
        chromeAndroidTask.maximize();
        // Arrange: Setup WindowAndroid and mock maximized bounds.
        var activityWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;

        // Act.
        chromeAndroidTask.setActivityWindowAndroid(
                activityWindowAndroid, chromeAndroidTaskWithMockDeps.mMockTabModel);

        // Assert.
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskTo(any(), anyInt(), boundsCaptor.capture());
        var capturedBounds = boundsCaptor.getValue();
        assertEquals(DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX, capturedBounds);
    }

    @Test
    public void setActivityWindowAndroid_fromPendingState_dispatchesPendingMinimize() {
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        // Arrange: Request MINIMIZE on a pending task.
        chromeAndroidTask.minimize();
        // Arrange: Setup WindowAndroid.
        int taskId = 2;
        var activityWindowAndroid =
                createActivityWindowAndroidMocks(taskId).mMockActivityWindowAndroid;
        var mockActivity = activityWindowAndroid.getActivity().get();

        // Act.
        chromeAndroidTask.setActivityWindowAndroid(
                activityWindowAndroid, chromeAndroidTaskWithMockDeps.mMockTabModel);

        // Assert.
        verify(mockActivity).moveTaskToBack(true);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void
            setActivityWindowAndroid_fromPendingState_withNonEmptyPendingBounds_dispatchesPendingRestore() {
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
        // Arrange: Setup WindowAndroid.
        var activityWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;

        // Act.
        chromeAndroidTask.setActivityWindowAndroid(
                activityWindowAndroid, chromeAndroidTaskWithMockDeps.mMockTabModel);

        // Assert.
        Rect expectedBoundsInPx = DisplayUtil.scaleToEnclosingRect(pendingBoundsInDp, dipScale);
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskTo(any(), anyInt(), boundsCaptor.capture());
        assertEquals(expectedBoundsInPx, boundsCaptor.getValue());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void
            setActivityWindowAndroid_fromPendingState_withEmptyPendingBounds_ignoresPendingRestore() {
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        // Arrange: Request RESTORE on a pending task.
        chromeAndroidTask.restore();
        // Arrange: Setup WindowAndroid.
        var activityWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;

        // Act.
        chromeAndroidTask.setActivityWindowAndroid(
                activityWindowAndroid, chromeAndroidTaskWithMockDeps.mMockTabModel);

        // Assert.
        verify(apiDelegate, never()).moveTaskTo(any(), anyInt(), any());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void setActivityWindowAndroid_fromPendingState_dispatchesPendingSetBounds() {
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
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
        chromeAndroidTask.setActivityWindowAndroid(
                activityWindowAndroid, chromeAndroidTaskWithMockDeps.mMockTabModel);

        // Assert.
        Rect expectedBoundsInPx = DisplayUtil.scaleToEnclosingRect(pendingBoundsInDp, dipScale);
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskTo(any(), anyInt(), boundsCaptor.capture());
        assertEquals(expectedBoundsInPx, boundsCaptor.getValue());
    }

    @Test
    public void setActivityWindowAndroid_fromPendingState_invokesCallback() {
        // Arrange: Create pending task with a callback.
        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();
        JniOnceCallback<Long> mockCallback = mock();
        var mockParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var task = new ChromeAndroidTaskImpl(/* pendingId= */ 1, mockParams, mockCallback);
        // Arrange: Setup WindowAndroid.
        int taskId = 2;
        var activityWindowAndroid =
                createActivityWindowAndroidMocks(taskId).mMockActivityWindowAndroid;

        // Act.
        task.setActivityWindowAndroid(activityWindowAndroid, mock(TabModel.class));

        // Assert.
        var ptrCaptor = ArgumentCaptor.forClass(Long.class);
        verify(mockCallback).onResult(ptrCaptor.capture());
        assertEquals(
                ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR,
                (long) ptrCaptor.getValue());
    }
}
