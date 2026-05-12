// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static android.os.Looper.getMainLooper;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeFalse;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockingDetails;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX;
import static org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.role.RoleManager;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Process;
import android.util.Pair;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.view.WindowMetrics;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowRoleManager;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.Promise;
import org.chromium.base.TimeUtils;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.ActivityScopedObjects;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskImpl.State;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskUnitTestSupport.ChromeAndroidTaskWithMockDeps;
import org.chromium.chrome.browser.ui.browser_window.PendingActionManager.PendingAction;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.ui.base.WindowResizePrecheckResult;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.mojom.WindowShowState;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

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
                        taskId, isPendingTask, /* isDesktopMode= */ true);
        var activityWindowAndroidMocks = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks;

        // Move mock Activity to the "resumed" state.
        var mockActivity = activityWindowAndroidMocks.mMockActivity;
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.RESUMED);

        return chromeAndroidTaskWithMockDeps;
    }

    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    private static ActivityScopedObjects createActivityScopedObjects(int taskId) {
        return createActivityScopedObjects(taskId, mock(Profile.class));
    }

    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    private static ActivityScopedObjects createActivityScopedObjects(int taskId, Profile profile) {
        var activityWindowAndroidMocks =
                ChromeAndroidTaskUnitTestSupport.createActivityWindowAndroidMocks(taskId);
        ChromeAndroidTaskUnitTestSupport.mockDesktopWindowingMode(activityWindowAndroidMocks);

        // Move mock Activity to the "resumed" state.
        var mockActivity = activityWindowAndroidMocks.mMockActivity;
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.RESUMED);
        return ChromeAndroidTaskUnitTestSupport.createMockActivityScopedObjects(
                activityWindowAndroidMocks.mMockActivityWindowAndroid, profile);
    }

    private static void assertListenersRegisteredForActivity(
            ChromeAndroidTaskImpl chromeAndroidTask, ActivityScopedObjects activityScopedObjects) {
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects,
                /* expectedNumberOfInvocations= */ 1);
    }

    private static void assertListenersRegisteredForActivity(
            ChromeAndroidTaskImpl chromeAndroidTask,
            ActivityScopedObjects activityScopedObjects,
            int expectedNumberOfInvocations) {
        assertListenersForActivity(
                chromeAndroidTask,
                activityScopedObjects,
                /* assertListenerRegistration= */ true,
                expectedNumberOfInvocations);
    }

    private static void assertListenersUnregisteredForActivity(
            ChromeAndroidTaskImpl chromeAndroidTask, ActivityScopedObjects activityScopedObjects) {
        assertListenersUnregisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects,
                /* expectedNumberOfInvocations= */ 1);
    }

    private static void assertListenersUnregisteredForActivity(
            ChromeAndroidTaskImpl chromeAndroidTask,
            ActivityScopedObjects activityScopedObjects,
            int expectedNumberOfInvocations) {
        assertListenersForActivity(
                chromeAndroidTask,
                activityScopedObjects,
                /* assertListenerRegistration= */ false,
                expectedNumberOfInvocations);
    }

    private static void assertListenersForActivity(
            ChromeAndroidTaskImpl chromeAndroidTask,
            ActivityScopedObjects activityScopedObjects,
            boolean assertListenerRegistration,
            int expectedNumberOfInvocations) {
        var activity = activityScopedObjects.mActivityWindowAndroid.getActivity().get();
        assertNotNull(activity);
        assertTrue(activity instanceof ActivityLifecycleDispatcherProvider);

        var activityLifecycleDispatcher =
                ((ActivityLifecycleDispatcherProvider) activity).getLifecycleDispatcher();
        var tabModel = activityScopedObjects.mTabModelSelector.getCurrentModel();
        assertTrue(mockingDetails(activityLifecycleDispatcher).isMock());
        assertTrue(mockingDetails(tabModel).isMock());

        if (assertListenerRegistration) {
            verify(activityLifecycleDispatcher, times(expectedNumberOfInvocations))
                    .register(isA(TopResumedActivityChangedWithNativeObserver.class));
            verify(activityLifecycleDispatcher, times(expectedNumberOfInvocations))
                    .register(isA(ConfigurationChangedObserver.class));
            verify(
                            activity.findViewById(android.R.id.content).getViewTreeObserver(),
                            times(expectedNumberOfInvocations))
                    .addOnGlobalLayoutListener(isA(OnGlobalLayoutListener.class));
        } else {
            verify(activityLifecycleDispatcher, times(expectedNumberOfInvocations))
                    .unregister(isA(TopResumedActivityChangedWithNativeObserver.class));
            verify(activityLifecycleDispatcher, times(expectedNumberOfInvocations))
                    .unregister(isA(ConfigurationChangedObserver.class));
            verify(
                            activity.findViewById(android.R.id.content).getViewTreeObserver(),
                            times(expectedNumberOfInvocations))
                    .removeOnGlobalLayoutListener(isA(OnGlobalLayoutListener.class));
        }
    }

    private static void assertNoPendingActions(ChromeAndroidTaskImpl chromeAndroidTask) {
        int[] pendingActions =
                chromeAndroidTask.getPendingActionManagerForTesting().getPendingActionsForTesting();
        assertEquals(2, pendingActions.length);
        assertEquals(PendingAction.NONE, pendingActions[0]);
        assertEquals(PendingAction.NONE, pendingActions[1]);
    }

    @Before
    public void setUp() {
        ShadowRoleManager.addRoleHolder(
                RoleManager.ROLE_BROWSER,
                ContextUtils.getApplicationContext().getPackageName(),
                Process.myUserHandle());
        var multiInstanceOrchestrator = mock(MultiInstanceOrchestrator.class);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(multiInstanceOrchestrator);
    }

    @Test
    public void constructor_withActivityScopedObjects_addsActivityScopedObjects() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Assert.
        List<ActivityScopedObjects> activityScopedObjectsList =
                chromeAndroidTask.getActivityScopedObjectsListForTesting();
        assertEquals(1, activityScopedObjectsList.size());
        assertEquals(
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects,
                activityScopedObjectsList.get(0));
    }

    @Test
    public void constructor_withActivityScopedObjects_registersListeners() {
        // Arrange & Act.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;

        assertListenersRegisteredForActivity(chromeAndroidTask, activityScopedObjects);
        assertTrue(
                ApplicationStatus.getTaskVisibilityListenersForTesting()
                        .hasObserver(chromeAndroidTask));
    }

    @Test
    public void constructor_withActivityScopedObjects_associateTabModelWithNativeBrowserWindow() {
        // Arrange & Act.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var mockTabModel =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mTabModelSelector
                        .getCurrentModel();

        // Assert.
        verify(mockTabModel, times(1))
                .associateWithBrowserWindow(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
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
        assertEquals(
                pendingTaskInfo.mCreateParams.getWindowType(),
                task.getPendingTaskInfo().mCreateParams.getWindowType());
        assertEquals(State.PENDING_CREATE, task.getState());
        assertNull(task.getId());
        assertTrue(task.getActivityScopedObjectsListForTesting().isEmpty());
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
    public void addActivityScopedObjects_anotherActivityScopedObjectsExists_addsInstanceToTop() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects1 = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var activityScopedObjects2 = createActivityScopedObjects(taskId);

        // Act.
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);

        // Assert.
        List<ActivityScopedObjects> activityScopedObjectsList =
                chromeAndroidTask.getActivityScopedObjectsListForTesting();
        assertEquals(2, activityScopedObjectsList.size());
        assertEquals(activityScopedObjects2, activityScopedObjectsList.get(0));
        assertEquals(activityScopedObjects1, activityScopedObjectsList.get(1));
    }

    @Test
    public void addActivityScopedObjects_anotherActivityScopedObjectsExists_movesListenersToTop() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects1 = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var activityScopedObjects2 = createActivityScopedObjects(taskId);
        var tabModel1 = activityScopedObjects1.mTabModelSelector.getCurrentModel();

        // Act.
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);

        // Assert.
        assertListenersUnregisteredForActivity(chromeAndroidTask, activityScopedObjects1);
        assertListenersRegisteredForActivity(chromeAndroidTask, activityScopedObjects2);
    }

    @Test
    public void addActivityScopedObjects_sameActivityScopedObjectsExists_movesInstanceToTop() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects1 = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var activityScopedObjects2 = createActivityScopedObjects(taskId);
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);

        // Act.
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects1);

        // Assert.
        List<ActivityScopedObjects> activityScopedObjectsList =
                chromeAndroidTask.getActivityScopedObjectsListForTesting();
        assertEquals(2, activityScopedObjectsList.size());
        assertEquals(activityScopedObjects1, activityScopedObjectsList.get(0));
        assertEquals(activityScopedObjects2, activityScopedObjectsList.get(1));
    }

    @Test
    public void
            addActivityScopedObjects_sameActivityScopedObjectsExists_doesNotRecreateBrowserWindow() {
        // Arrange: Create a task and its initial ActivityScopedObjects.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;

        // Arrange: Add an observer to track window lifecycle events.
        var observer = mock(AndroidBrowserWindowObserver.class);
        chromeAndroidTask.addAndroidBrowserWindowObserver(observer);

        // Act: Add the SAME ActivityScopedObjects instance again.
        // This is a common occurrence when an Activity is brought to the foreground
        // and its references are refreshed in the Task tracker.
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects);

        // Assert:
        verify(
                        observer,
                        never().description(
                                        "Window should not be removed when re-adding the same"
                                                + " activity"))
                .onBrowserWindowRemoved(any(Long.class));
        verify(
                        observer,
                        never().description(
                                        "A new window should not be added when re-adding the same"
                                                + " activity"))
                .onBrowserWindowAdded(any(Long.class));

        // Final check: Ensure the task still has exactly 1 window and 1 activity.
        assertEquals(1, chromeAndroidTask.getActivityScopedObjectsListForTesting().size());
        assertEquals(1, chromeAndroidTask.getAllNativeBrowserWindowPtrs().size());
    }

    @Test
    public void
            addActivityScopedObjects_sameActivityScopedObjectsExists_movesListenersToNewTopActivity() {
        // Arrange: Add the 1st instance of ActivityScopedObjects.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects1 = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects1,
                /* expectedNumberOfInvocations= */ 1);

        // Arrange: Add the 2nd instance of ActivityScopedObjects.
        var activityScopedObjects2 = createActivityScopedObjects(taskId);
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);
        assertListenersUnregisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects1,
                /* expectedNumberOfInvocations= */ 1);
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects2,
                /* expectedNumberOfInvocations= */ 1);

        // Act: Add the 1st instance of ActivityScopedObjects again.
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects1);

        // Assert:
        // (1) Unregister listeners for the previous top Activity;
        // (2) Re-register listeners for the Activity that's moved to top.
        assertListenersUnregisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects2,
                /* expectedNumberOfInvocations= */ 1);
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects1,
                /* expectedNumberOfInvocations= */ 2);
    }

    @Test
    public void addActivityScopedObjects_fromPendingState_setsIdAndState() {
        int taskId = 2;
        int unusedTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(unusedTaskId);

        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(taskId, /* isPendingTask= */ true);
        var pendingTask = (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;

        // Act.
        pendingTask.addActivityScopedObjects(activityScopedObjects);
        pendingTask.onActivityTopResumedChanged(true);
        pendingTask.onTopResumedActivityChangedWithNative(true);

        // Assert.
        assertEquals(taskId, (int) pendingTask.getId());
        assertNull(pendingTask.getPendingTaskInfo());

        List<ActivityScopedObjects> activityScopedObjectsList =
                pendingTask.getActivityScopedObjectsListForTesting();
        assertEquals(1, activityScopedObjectsList.size());
        assertEquals(activityScopedObjects, activityScopedObjectsList.get(0));
    }

    @Test
    public void addActivityScopedObjects_associatesTabModelWithNativeBrowserWindow() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        var newActivityScopedObjects = createActivityScopedObjects(taskId);
        var newMockTabModel = newActivityScopedObjects.mTabModelSelector.getCurrentModel();

        // Act.
        chromeAndroidTask.addActivityScopedObjects(newActivityScopedObjects);

        // Assert.
        verify(newMockTabModel, times(1))
                .associateWithBrowserWindow(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }

    @Test
    public void addActivityScopedObjects_newObjectsHaveDifferentTaskId_throwsException() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        var newActivityScopedObjects = createActivityScopedObjects(/* taskId= */ 2);

        // Act & Assert.
        assertThrows(
                AssertionError.class,
                () -> chromeAndroidTask.addActivityScopedObjects(newActivityScopedObjects));
    }

    @Test
    public void addActivityScopedObjects_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        chromeAndroidTask.destroy();

        // Act & Assert.
        var newActivityScopedObjects = createActivityScopedObjects(taskId);
        assertThrows(
                AssertionError.class,
                () -> chromeAndroidTask.addActivityScopedObjects(newActivityScopedObjects));
    }

    @Test
    public void getTopActivityWindowAndroid_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        chromeAndroidTask.destroy();

        // Act & Assert.
        assertThrows(AssertionError.class, chromeAndroidTask::getTopActivityWindowAndroid);
    }

    @Test
    public void removeActivityScopedObjects_activityToRemoveIsAtTop_removesActivityScopedObjects() {
        // Arrange: Add 2 instances of ActivityScopedObjects.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects1 = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var activityScopedObjects2 = createActivityScopedObjects(taskId);
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);

        // Act: Remove activityScopedObjects2, which represents the top Activity.
        chromeAndroidTask.removeActivityScopedObjects(
                activityScopedObjects2.mActivityWindowAndroid);

        // Assert:
        List<ActivityScopedObjects> activityScopedObjectsList =
                chromeAndroidTask.getActivityScopedObjectsListForTesting();
        assertEquals(1, activityScopedObjectsList.size());
        assertEquals(activityScopedObjects1, activityScopedObjectsList.get(0));
    }

    @Test
    public void
            removeActivityScopedObjects_activityToRemoveIsAtTop_movesListenersToNewTopActivity() {
        // Arrange: Add the 1st instance of ActivityScopedObjects.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects1 = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects1,
                /* expectedNumberOfInvocations= */ 1);

        // Arrange: Add the 2nd instance of ActivityScopedObjects.
        var activityScopedObjects2 = createActivityScopedObjects(taskId);
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);
        assertListenersUnregisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects1,
                /* expectedNumberOfInvocations= */ 1);
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects2,
                /* expectedNumberOfInvocations= */ 1);

        var tabModel2 = activityScopedObjects2.mTabModelSelector.getCurrentModel();

        // Act: Remove activityScopedObjects2, which represents the top Activity.
        chromeAndroidTask.removeActivityScopedObjects(
                activityScopedObjects2.mActivityWindowAndroid);

        // Assert:
        // (1) Unregister listeners for the previous top Activity (activityScopedObjects2);
        // (2) Re-register listeners for the Activity that's moved to top (activityScopedObjects1).
        verify(tabModel2).dissociateWithBrowserWindow();
        assertListenersUnregisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects2,
                /* expectedNumberOfInvocations= */ 1);
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects1,
                /* expectedNumberOfInvocations= */ 2);

        // Assert: TaskVisibilityListener still exists for the Task.
        assertTrue(
                ApplicationStatus.getTaskVisibilityListenersForTesting()
                        .hasObserver(chromeAndroidTask));
    }

    @Test
    public void removeActivityScopedObjects_destroysActivityScopedFeatures() throws Exception {
        // Arrange: Add 2 instances of ActivityScopedObjects.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects1 = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var activityScopedObjects2 = createActivityScopedObjects(taskId);
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);

        // Add activity-scoped features.
        var testFeature1 = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey1 =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class,
                        /* profile= */ null,
                        activityScopedObjects1.mActivityWindowAndroid);
        chromeAndroidTask.addFeature(featureKey1, () -> testFeature1);

        var testFeature2 = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey2 =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class,
                        /* profile= */ null,
                        activityScopedObjects2.mActivityWindowAndroid);
        chromeAndroidTask.addFeature(featureKey2, () -> testFeature2);

        // Act: Remove activityScopedObjects2.
        chromeAndroidTask.removeActivityScopedObjects(
                activityScopedObjects2.mActivityWindowAndroid);

        // Assert:
        // (1) Feature associated with activity 2 is destroyed.
        testFeature2.mOnFeatureRemovedHelper.waitForCallback(0, 1);
        assertNull(chromeAndroidTask.getFeatureForTesting(featureKey2));

        // (2) Feature associated with activity 1 is NOT destroyed.
        assertEquals(0, testFeature1.mOnFeatureRemovedHelper.getCallCount());
        assertNotNull(chromeAndroidTask.getFeatureForTesting(featureKey1));
    }

    @Test
    public void
            removeActivityScopedObjects_activityToRemoveIsNotAtTop_removesActivityScopedObjects() {
        // Arrange: Add the 1st instance of ActivityScopedObjects.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects1 = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects1,
                /* expectedNumberOfInvocations= */ 1);

        // Arrange: Add the 2nd instance of ActivityScopedObjects.
        var activityScopedObjects2 = createActivityScopedObjects(taskId);
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);
        assertListenersUnregisteredForActivity(
                chromeAndroidTask, activityScopedObjects1, /* expectedNumberOfInvocations= */ 1);
        assertListenersRegisteredForActivity(
                chromeAndroidTask, activityScopedObjects2, /* expectedNumberOfInvocations= */ 1);
        List<ActivityScopedObjects> activityScopedObjectsList =
                chromeAndroidTask.getActivityScopedObjectsListForTesting();
        assertEquals(2, activityScopedObjectsList.size());

        // Act: Remove activityScopedObjects1, which doesn't represent the top Activity.
        chromeAndroidTask.removeActivityScopedObjects(
                activityScopedObjects1.mActivityWindowAndroid);

        // Assert: activityScopedObjects1 is removed.
        activityScopedObjectsList = chromeAndroidTask.getActivityScopedObjectsListForTesting();
        assertEquals(1, activityScopedObjectsList.size());
        assertEquals(activityScopedObjects2, activityScopedObjectsList.get(0));

        // Assert: no listener should be registered or unregistered as a result of removing
        // activityScopedObjects1.
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects1,
                /* expectedNumberOfInvocations= */ 1);
        assertListenersUnregisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects1,
                /* expectedNumberOfInvocations= */ 1);
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects2,
                /* expectedNumberOfInvocations= */ 1);
        assertListenersUnregisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects2,
                /* expectedNumberOfInvocations= */ 0);
        assertTrue(
                ApplicationStatus.getTaskVisibilityListenersForTesting()
                        .hasObserver(chromeAndroidTask));
    }

    @Test
    public void removeActivityScopedObjects_mixedProfile_destroysBothRegularAndIncognitoWindows() {
        assumeFalse(BuildConfig.IS_DESKTOP_ANDROID);
        // Arrange: Create Task with Mixed Profile support.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1,
                        /* isPendingTask= */ false,
                        /* isDesktopMode= */ true,
                        SupportedProfileType.MIXED);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var tabModelSelector = activityScopedObjects.mTabModelSelector;
        var activityWindowAndroid = activityScopedObjects.mActivityWindowAndroid;

        // Simulate Incognito creation to force a second window/deque entry.
        var incognitoModel = (IncognitoTabModel) tabModelSelector.getModel(true);
        var incognitoProfile = mock(Profile.class);
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);
        when(incognitoModel.getProfile()).thenReturn(incognitoProfile);

        // Trigger the observer to create the incognito window
        ArgumentCaptor<IncognitoTabModelObserver> captor =
                ArgumentCaptor.forClass(IncognitoTabModelObserver.class);
        verify(incognitoModel).addIncognitoObserver(captor.capture());
        captor.getValue().onIncognitoModelCreated();

        // Pre-assertion: We should have 2 native pointers (Regular + Incognito).
        assertEquals(2, chromeAndroidTask.getAllNativeBrowserWindowPtrs().size());

        // Act: Remove the SINGLE ActivityScopedObjects.
        chromeAndroidTask.removeActivityScopedObjects(activityWindowAndroid);

        // Assert: Both windows should be destroyed.
        assertEquals(0, chromeAndroidTask.getAllNativeBrowserWindowPtrs().size());

        // Verify native destroy was called for both.
        verify(chromeAndroidTaskWithMockDeps.mMockAndroidBrowserWindowNatives, times(1))
                .destroy(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
        verify(chromeAndroidTaskWithMockDeps.mMockAndroidBrowserWindowNatives, times(1))
                .destroy(
                        ChromeAndroidTaskUnitTestSupport
                                .FAKE_INCOGNITO_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }

    @Test
    public void removeActivityScopedObjects_multipleActivities_destroysOnlyRemovedActivityWindow() {
        // Arrange: Add Activity 1
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        var mockNatives = chromeAndroidTaskWithMockDeps.mMockAndroidBrowserWindowNatives;

        // Arrange: Add Activity 2 to the same task using the same profile
        var activityScopedObjects2 = createActivityScopedObjects(taskId, profile);
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);

        // We now have 2 ActivityScopedObjects wrappers, each with its own AndroidBrowserWindow
        assertEquals(2, chromeAndroidTask.getAllNativeBrowserWindowPtrs().size());

        // Grab the specific Java window objects so we can verify which one survives
        var window1 =
                chromeAndroidTask.getBrowserWindowsForTesting(profile).get(1); // older (Activity 1)

        // Act: Remove Activity 2
        chromeAndroidTask.removeActivityScopedObjects(
                activityScopedObjects2.mActivityWindowAndroid);

        // Assert:
        // 1. Exactly one window was destroyed natively. (Both windows share the same mock native
        //    pointer, so we verify the destroy method was invoked exactly 1 time in total).
        verify(mockNatives, times(1)).destroy(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        // 2. Only Activity 1's window remains tracked in the Java layer.
        var remainingWindows = chromeAndroidTask.getBrowserWindowsForTesting(profile);
        assertEquals(1, remainingWindows.size());
        assertEquals(
                "Activity 1's window should be the surviving window",
                window1,
                remainingWindows.get(0));
    }

    @Test
    public void removeActivityScopedObjects_destroysBrowserWindow() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        long nativePtr = chromeAndroidTask.getOrCreateNativeBrowserWindowPtr(profile);
        var observer = mock(AndroidBrowserWindowObserver.class);
        chromeAndroidTask.addAndroidBrowserWindowObserver(observer);
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;

        // Act.
        chromeAndroidTask.removeActivityScopedObjects(activityScopedObjects.mActivityWindowAndroid);

        // Assert.
        verify(observer, times(1)).onBrowserWindowRemoved(nativePtr);
        verify(chromeAndroidTaskWithMockDeps.mMockAndroidBrowserWindowNatives, times(1))
                .destroy(nativePtr);
    }

    @Test
    public void removeActivityScopedObjects_mixedProfile_removesIncognitoObserver() {
        assumeFalse(BuildConfig.IS_DESKTOP_ANDROID);
        // Arrange: Create Task with Mixed Profile support.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1,
                        /* isPendingTask= */ false,
                        /* isDesktopMode= */ true,
                        SupportedProfileType.MIXED);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var tabModelSelector = activityScopedObjects.mTabModelSelector;
        var incognitoModel = (IncognitoTabModel) tabModelSelector.getModel(true);

        // Capture the observer that was registered during initialization.
        ArgumentCaptor<IncognitoTabModelObserver> captor =
                ArgumentCaptor.forClass(IncognitoTabModelObserver.class);
        verify(incognitoModel).addIncognitoObserver(captor.capture());
        IncognitoTabModelObserver registeredObserver = captor.getValue();
        assertNotNull(registeredObserver);

        // Act: Remove the ActivityScopedObjects.
        chromeAndroidTask.removeActivityScopedObjects(activityScopedObjects.mActivityWindowAndroid);

        // Assert: The exact observer that was added should be removed.
        verify(incognitoModel, times(1)).removeIncognitoObserver(registeredObserver);
    }

    @Test
    public void removeActivityScopedObjects_dissociatesTabModelBeforeDestroyingNativeWindow() {
        // Arrange
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var mockTabModel = activityScopedObjects.mTabModelSelector.getCurrentModel();
        var mockNatives = chromeAndroidTaskWithMockDeps.mMockAndroidBrowserWindowNatives;

        // Act
        chromeAndroidTask.removeActivityScopedObjects(activityScopedObjects.mActivityWindowAndroid);

        // Assert: Strict ordering
        InOrder inOrder = inOrder(mockTabModel, mockNatives);
        inOrder.verify(mockTabModel).dissociateWithBrowserWindow();
        inOrder.verify(mockNatives).destroy(any(Long.class));
    }

    @Test
    public void addFeature_featureDoesNotExist_returnNewlyCreatedFeature() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey =
                new ChromeAndroidTaskFeatureKey(TestChromeAndroidTaskFeature.class, profile);

        // Act.
        var returnedFeature = chromeAndroidTask.addFeature(featureKey, () -> testFeature);

        // Assert.
        assertEquals(testFeature, returnedFeature);
    }

    @Test
    public void addFeature_featureAlreadyAdded_returnExistingFeature() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        var activityWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, profile, activityWindowAndroid);
        Supplier<TestChromeAndroidTaskFeature> testFeatureSupplier =
                new Supplier<>() {
                    private boolean mIsFeatureCreated;

                    @Override
                    public TestChromeAndroidTaskFeature get() {
                        assertFalse(
                                "addFeature() should only create the feature once",
                                mIsFeatureCreated);

                        var feature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
                        mIsFeatureCreated = true;

                        return feature;
                    }
                };

        // Act: add the feature twice.
        var returnedFeature1 = chromeAndroidTask.addFeature(featureKey, testFeatureSupplier);
        var returnedFeature2 = chromeAndroidTask.addFeature(featureKey, testFeatureSupplier);

        // Assert.
        assertEquals(returnedFeature1, returnedFeature2);
    }

    @Test
    public void addFeature_addsFeatureToInternalFeatureMap() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey =
                new ChromeAndroidTaskFeatureKey(TestChromeAndroidTaskFeature.class, profile);
        var altFeatureKeyRef =
                new ChromeAndroidTaskFeatureKey(TestChromeAndroidTaskFeature.class, profile);
        var profileLessFeatureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null);

        // Act.
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);

        // Assert.
        assertEquals(testFeature, chromeAndroidTask.getFeatureForTesting(featureKey));
        assertEquals(testFeature, chromeAndroidTask.getFeatureForTesting(altFeatureKeyRef));
        assertNull(chromeAndroidTask.getFeatureForTesting(profileLessFeatureKey));
    }

    @Test
    public void
            addFeature_nativeBrowserWindowMatchingFeatureKeyExists_invokesOnAddedToTaskWithBrowserWindowPtr() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        var activityWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);

        // Act.
        chromeAndroidTask.addFeature(
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, profile, activityWindowAndroid),
                () -> testFeature);

        // Assert.
        assertEquals(1, testFeature.mOnAddedToTaskHistory.size());
        assertEquals(
                FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR,
                (long) testFeature.mOnAddedToTaskHistory.get(0));
    }

    @Test
    public void
            addFeature_nativeBrowserWindowMatchingFeatureKeyDoesNotExist_invokesOnAddedToTaskWithNullPtr() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);

        // Act.
        chromeAndroidTask.addFeature(
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class,
                        // A native BrowserWindowInterface is always associated with a valid
                        // Profile, so a null Profile will not match any BrowserWindowInterface.
                        /* profile= */ null,
                        activityWindowAndroid),
                () -> testFeature);

        // Assert.
        assertEquals(1, testFeature.mOnAddedToTaskHistory.size());
        assertEquals(0, (long) testFeature.mOnAddedToTaskHistory.get(0));
    }

    @Test
    public void addFeature_featureAlreadyAdded_doesNotInvokeOnAddedToTaskForFeature() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        var activityWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, profile, activityWindowAndroid);

        // Act: add the feature twice.
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);

        // Assert: only the first addFeature() should invoke onAddedToTask().
        assertEquals(1, testFeature.mOnAddedToTaskHistory.size());
        assertEquals(
                FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR,
                (long) testFeature.mOnAddedToTaskHistory.get(0));
    }

    @Test
    public void addFeature_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        chromeAndroidTask.destroy();

        // Act & Assert.
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        ChromeAndroidTaskFeatureKey featureKey =
                new ChromeAndroidTaskFeatureKey(TestChromeAndroidTaskFeature.class, profile);
        assertThrows(
                AssertionError.class,
                () -> chromeAndroidTask.addFeature(featureKey, () -> testFeature));
    }

    @Test
    public void removeAllFeaturesForActivity_removesFeatureAndInvokesOnFeatureRemoved()
            throws Exception {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var windowAndroid =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mActivityWindowAndroid;
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null, windowAndroid);
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);

        // Act.
        chromeAndroidTask.removeAllFeaturesForActivity(windowAndroid);

        // Assert.
        testFeature.mOnFeatureRemovedHelper.waitForCallback(
                /* currentCallCount= */ 0, /* numberOfCallsToWaitFor= */ 1);
        assertNull(
                "Feature should be removed from the Task.",
                chromeAndroidTask.getFeatureForTesting(featureKey));
    }

    @Test
    public void removeAllFeaturesForActivity_keepFeaturesNotAssociatedWithGivenActivity()
            throws Exception {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var windowAndroid =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mActivityWindowAndroid;
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null, windowAndroid);
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);

        var activityScopedObjects2 = createActivityScopedObjects(/* taskId= */ 1);
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);
        var testFeature2 = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey2 =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class,
                        /* profile= */ null,
                        activityScopedObjects2.mActivityWindowAndroid);
        chromeAndroidTask.addFeature(featureKey2, () -> testFeature2);

        // Act.
        chromeAndroidTask.removeAllFeaturesForActivity(windowAndroid);

        // Assert.
        testFeature.mOnFeatureRemovedHelper.waitForCallback(
                /* currentCallCount= */ 0, /* numberOfCallsToWaitFor= */ 1);
        assertNull(
                "Feature should be removed from the Task.",
                chromeAndroidTask.getFeatureForTesting(featureKey));
        assertEquals(
                "Feature should not be removed from the Task.",
                testFeature2,
                chromeAndroidTask.getFeatureForTesting(featureKey2));
    }

    @Test
    public void getOrCreateNativeBrowserWindowPtr_returnsPtrValueForAliveTask() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;

        // Act.
        long nativeBrowserWindowPtr = chromeAndroidTask.getOrCreateNativeBrowserWindowPtr(profile);

        // Assert.
        assertEquals(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR, nativeBrowserWindowPtr);
    }

    @Test
    public void getOrCreateNativeBrowserWindowPtr_returnsPtrValueForPendingTask() {
        // Arrange.
        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();
        var pendingTaskInfo = ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo();
        var profile = pendingTaskInfo.mCreateParams.getProfile();
        var chromeAndroidTask = new ChromeAndroidTaskImpl(pendingTaskInfo);

        // Act.
        long nativeBrowserWindowPtr = chromeAndroidTask.getOrCreateNativeBrowserWindowPtr(profile);

        // Assert.
        assertEquals(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR, nativeBrowserWindowPtr);
    }

    @Test
    public void getOrCreateNativeBrowserWindowPtr_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        chromeAndroidTask.destroy();

        // Act & Assert.
        assertThrows(
                AssertionError.class,
                () -> chromeAndroidTask.getOrCreateNativeBrowserWindowPtr(profile));
    }

    @Test
    public void getNativeBrowserWindowPtr_returnsPtrValueForRegisteredActivity() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        var activity =
                chromeAndroidTaskWithMockDeps
                        .mActivityScopedObjects
                        .mActivityWindowAndroid
                        .getActivity()
                        .get();

        // Act.
        long nativeBrowserWindowPtr =
                chromeAndroidTask.getNativeBrowserWindowPtr(profile, activity);

        // Assert.
        assertEquals(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR, nativeBrowserWindowPtr);
    }

    @Test
    public void getNativeBrowserWindowPtr_returnsZeroForUnregisteredActivity() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        var unregisteredActivity = mock(Activity.class);

        // Act.
        long nativeBrowserWindowPtr =
                chromeAndroidTask.getNativeBrowserWindowPtr(profile, unregisteredActivity);

        // Assert.
        assertEquals(0, nativeBrowserWindowPtr);
    }

    @Test
    public void getNativeBrowserWindowPtr_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        var activity =
                chromeAndroidTaskWithMockDeps
                        .mActivityScopedObjects
                        .mActivityWindowAndroid
                        .getActivity()
                        .get();
        chromeAndroidTask.destroy();

        // Act & Assert.
        assertThrows(
                AssertionError.class,
                () -> chromeAndroidTask.getNativeBrowserWindowPtr(profile, activity));
    }

    @Test
    public void destroy_unregisterListenersForTopActivity() {
        // Arrange: Add the 1st instance of ActivityScopedObjects.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects1 = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects1,
                /* expectedNumberOfInvocations= */ 1);

        // Arrange: Add the 2nd instance of ActivityScopedObjects.
        var activityScopedObjects2 = createActivityScopedObjects(taskId);
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);
        assertListenersUnregisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects1,
                /* expectedNumberOfInvocations= */ 1);
        assertListenersRegisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects2,
                /* expectedNumberOfInvocations= */ 1);

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        assertListenersUnregisteredForActivity(
                chromeAndroidTask,
                activityScopedObjects2,
                /* expectedNumberOfInvocations= */ 1);
    }

    @Test
    public void destroy_clearsActivityScopedObjects() {
        // Arrange.
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl)
                        createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        assertTrue(chromeAndroidTask.getActivityScopedObjectsListForTesting().isEmpty());
    }

    @Test
    public void destroy_destroysFeature() throws Exception {
        // Arrange.
        var chromeAndroidTask =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null);
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        testFeature.mOnFeatureRemovedHelper.waitForCallback(
                /* currentCallCount= */ 0, /* numberOfCallsToWaitFor= */ 1);
        assertTrue(chromeAndroidTask.getAllFeaturesForTesting().isEmpty());
    }

    @Test
    public void destroy_mixedProfile_removesIncognitoObserver() {
        assumeFalse(BuildConfig.IS_DESKTOP_ANDROID);
        // Arrange: Create Task with Mixed Profile support.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1,
                        /* isPendingTask= */ false,
                        /* isDesktopMode= */ true,
                        SupportedProfileType.MIXED);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var tabModelSelector = activityScopedObjects.mTabModelSelector;
        var incognitoModel = (IncognitoTabModel) tabModelSelector.getModel(true);

        // Capture the observer that was registered during initialization.
        ArgumentCaptor<IncognitoTabModelObserver> captor =
                ArgumentCaptor.forClass(IncognitoTabModelObserver.class);
        verify(incognitoModel).addIncognitoObserver(captor.capture());
        IncognitoTabModelObserver registeredObserver = captor.getValue();
        assertNotNull(registeredObserver);

        // Act: Destroy the entire task.
        chromeAndroidTask.destroy();

        // Assert: The observer should be removed during the teardown of ActivityScopedObjects.
        verify(incognitoModel, times(1)).removeIncognitoObserver(registeredObserver);
    }

    @Test
    public void destroy_destroysAndroidBrowserWindow() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        var mockAndroidBrowserWindowNatives =
                assertNonNull(chromeAndroidTaskWithMockDeps.mMockAndroidBrowserWindowNatives);
        long nativeAndroidBrowserWindowPtr =
                chromeAndroidTask.getOrCreateNativeBrowserWindowPtr(profile);
        var mockTabModel =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mTabModelSelector
                        .getCurrentModel();

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        verify(mockAndroidBrowserWindowNatives, times(1)).destroy(nativeAndroidBrowserWindowPtr);
        verify(mockTabModel).dissociateWithBrowserWindow();
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
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null);
        testFeature.mShouldRefuseToBeRemoved = true;
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);

        // Act & Assert.
        assertThrows(AssertionError.class, chromeAndroidTask::destroy);
    }

    @Test
    public void onProfileDestroyed_removesProfileScopedFeature() throws Exception {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;

        var profileScopedFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var profileScopedFeatureKey =
                new ChromeAndroidTaskFeatureKey(TestChromeAndroidTaskFeature.class, profile);
        chromeAndroidTask.addFeature(profileScopedFeatureKey, () -> profileScopedFeature);

        var nonProfileScopedFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var nonProfileScopedFeatureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null);
        chromeAndroidTask.addFeature(nonProfileScopedFeatureKey, () -> nonProfileScopedFeature);

        // Act.
        // Simulate a profile destruction.
        ProfileManager.onProfileDestroyed(profile);

        // Assert.
        profileScopedFeature.mOnFeatureRemovedHelper.waitForCallback(0, 1);
        assertNull(chromeAndroidTask.getFeatureForTesting(profileScopedFeatureKey));
        assertEquals(
                nonProfileScopedFeature,
                chromeAndroidTask.getFeatureForTesting(nonProfileScopedFeatureKey));
        assertEquals(0, nonProfileScopedFeature.mOnFeatureRemovedHelper.getCallCount());
    }

    @Test
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void onConfigurationChanged_windowBoundsChanged_invokesOnTaskBoundsChangedForFeature() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null);
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);

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
        assertEquals(2, testFeature.mTaskBoundsChangeHistory.size());
        assertEquals(taskBounds1, testFeature.mTaskBoundsChangeHistory.get(0));
        assertEquals(taskBounds2, testFeature.mTaskBoundsChangeHistory.get(1));
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

        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null);
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);

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
        assertEquals(1, testFeature.mTaskBoundsChangeHistory.size());
        assertEquals(
                DisplayUtil.scaleToEnclosingRect(taskBoundsInPx, 1.0f / dipScale),
                testFeature.mTaskBoundsChangeHistory.get(0));
    }

    @Test
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void
            onConfigurationChanged_windowBoundsChangesInPxButNotInDp_doesNotInvokeOnTaskBoundsChangedForFeature() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null);
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);

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
        assertEquals(1, testFeature.mTaskBoundsChangeHistory.size());
        assertEquals(
                DisplayUtil.scaleToEnclosingRect(taskBoundsInPx1, 1.0f / dipScale1),
                testFeature.mTaskBoundsChangeHistory.get(0));
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
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null);
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);

        // Act.
        chromeAndroidTask.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        chromeAndroidTask.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ false);

        // Assert.
        assertEquals(2, testFeature.mTaskFocusChangeHistory.size());
        assertTrue(testFeature.mTaskFocusChangeHistory.get(0));
        assertFalse(testFeature.mTaskFocusChangeHistory.get(1));
    }

    @Test
    public void onTaskVisibilityChanged_invokesOnTaskVisibilityChangedForFeature() {
        // Arrange.
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl)
                        createChromeAndroidTaskWithMockDeps(/* taskId= */ 1).mChromeAndroidTask;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null);
        chromeAndroidTask.addFeature(featureKey, () -> testFeature);

        // Act.
        chromeAndroidTask.onTaskVisibilityChanged(/* taskId= */ 1, /* isVisible= */ true);
        chromeAndroidTask.onTaskVisibilityChanged(/* taskId= */ 1, /* isVisible= */ false);

        // Assert.
        assertEquals(2, testFeature.mTaskVisibilityChangeHistory.size());
        assertTrue(testFeature.mTaskVisibilityChangeHistory.get(0));
        assertFalse(testFeature.mTaskVisibilityChangeHistory.get(1));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void canResize_noRestrictions() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Act & Assert.
        assertEquals(WindowResizePrecheckResult.OK, chromeAndroidTask.canResize());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void canResize_sdkTooLow_returnsSdkTooLow() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Act & Assert.
        assertEquals(WindowResizePrecheckResult.SDK_TOO_LOW, chromeAndroidTask.canResize());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void canResize_notBrowserRole_returnsBrowserRoleNotHeld() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Remove the ROLE_BROWSER that was added in setUp()
        ShadowRoleManager.removeRoleHolder(
                RoleManager.ROLE_BROWSER,
                ContextUtils.getApplicationContext().getPackageName(),
                Process.myUserHandle());

        // Act & Assert.
        assertEquals(
                WindowResizePrecheckResult.BROWSER_ROLE_NOT_HELD, chromeAndroidTask.canResize());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void canResize_notInDesktopWindow_returnsNotAFreeformWindow() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        AppHeaderUtils.setAppInDesktopWindowForTesting(false);

        // Act & Assert.
        assertEquals(
                WindowResizePrecheckResult.NOT_A_FREEFORM_WINDOW, chromeAndroidTask.canResize());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void canResize_nullAppTask_returnsNullAppTask() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Simulate a Custom Tab (CCT) window by setting AppTask to null
        AndroidTaskUtils.setAppTaskForTesting(null);

        // Act & Assert.
        assertEquals(WindowResizePrecheckResult.NULL_APP_TASK, chromeAndroidTask.canResize());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void canResize_nullAconfigFlaggedApiDelegate_returnsNullAconfigFlaggedApiDelegate() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        AconfigFlaggedApiDelegate.setInstanceForTesting(null);

        // Act & Assert.
        assertEquals(
                WindowResizePrecheckResult.NULL_ACONFIG_FLAGGED_API_DELEGATE,
                chromeAndroidTask.canResize());
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
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void maximize_cannotSetBounds_noOp() {
        // Arrange: Set up ChromeAndroidTask and its mock dependencies.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Arrange: Enter non-desktop-windowing mode, where we can't set window bounds.
        AppHeaderUtils.setAppInDesktopWindowForTesting(false);

        // Act.
        chromeAndroidTask.maximize();

        // Assert.
        assertNoPendingActions(chromeAndroidTask);
        verify(apiDelegate, never()).moveTaskToWithPromise(any(), anyInt(), any());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void maximize_whenWindowMinimized_shouldActivateWindow() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var mockActivity = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockActivity;
        var mockActivityManager =
                (ActivityManager) mockActivity.getSystemService(Context.ACTIVITY_SERVICE);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        chromeAndroidTask.minimize();
        assertEquals(
                "Future state of isVisible() should be false when minimize() is pending",
                false,
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isVisibleFuture(chromeAndroidTask.getState()));
        chromeAndroidTask.onTaskVisibilityChanged(/* taskId= */ 1, /* isVisible= */ false);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);

        // Act.
        chromeAndroidTask.maximize();

        // Assert.
        verify(mockActivityManager, description("Task should be activated"))
                .moveTaskToFront(/* taskId= */ eq(1), anyInt());
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
    public void setBoundsInDp_cannotSetBounds_noOp() {
        // Arrange: Set up ChromeAndroidTask and its dependencies.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var displayAndroid =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockDisplayAndroid;
        float dipScale = 2.0f;
        when(displayAndroid.getDipScale()).thenReturn(dipScale);

        // Arrange: Enter non-desktop-windowing mode, where we can't set window bounds.
        AppHeaderUtils.setAppInDesktopWindowForTesting(false);

        // Act.
        Rect newBoundsInDp =
                DisplayUtil.scaleToEnclosingRect(
                        DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX, 1.0f / dipScale);
        newBoundsInDp.offset(/* dx= */ 10, /* dy= */ 10);
        chromeAndroidTask.setBoundsInDp(newBoundsInDp);

        // Assert.
        assertNoPendingActions(chromeAndroidTask);
        verify(apiDelegate, never()).moveTaskToWithPromise(any(), anyInt(), any());
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
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void restore_cannotSetBounds_noOp() {
        // Arrange: Set up ChromeAndroidTask and its mock dependencies.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Check the default test setup.
        assertFalse("Task shouldn't be minimized", chromeAndroidTask.isMinimized());
        assertFalse("Task shouldn't be maximized", chromeAndroidTask.isMaximized());
        assertFalse("Task shouldn't be fullscreen", chromeAndroidTask.isFullscreen());

        // Arrange: Call maximize(). This should set mRestoredBounds to the current bounds.
        chromeAndroidTask.maximize();
        assertEquals(
                "restored bounds should be set to the current bounds",
                DEFAULT_CURRENT_WINDOW_BOUNDS_IN_PX,
                chromeAndroidTask.getRestoredBoundsInPxForTesting());
        chromeAndroidTask.getPendingActionManagerForTesting().clearPendingActionsForTesting();

        // Arrange: Enter non-desktop-windowing mode, where we can't set window bounds.
        AppHeaderUtils.setAppInDesktopWindowForTesting(false);

        // Act
        chromeAndroidTask.restore();

        // Assert:
        // (1) No pending actions.
        // (2) moveTaskToWithPromise() should only be called once
        // (for maximize() during test setup).
        assertNoPendingActions(chromeAndroidTask);
        verify(apiDelegate, times(1))
                .moveTaskToWithPromise(any(), anyInt(), eq(DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX));
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

        // Assert:
        // The Task is visible by default, so show() should be a no-op.
        assertNoPendingActions(task);
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
        // Ensure there are at least 2 tasks.
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var secondChromeAndroidTask =
                (ChromeAndroidTaskImpl)
                        createChromeAndroidTaskWithMockDeps(/* taskId= */ 2).mChromeAndroidTask;
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(true);
        Assert.assertTrue("Set up task to be active", chromeAndroidTask.isActive());
        // All tasks must have been active before.
        secondChromeAndroidTask.onTopResumedActivityChangedWithNative(true);
        chromeAndroidTask.onTopResumedActivityChangedWithNative(true);
        secondChromeAndroidTask.onTopResumedActivityChangedWithNative(false);

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

        // Assert:
        // The Task is active by default, so activate() should be a no-op.
        assertNoPendingActions(task);
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
        // Ensure there are at least 2 tasks.
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var secondChromeAndroidTask =
                (ChromeAndroidTaskImpl)
                        createChromeAndroidTaskWithMockDeps(/* taskId= */ 2).mChromeAndroidTask;
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(true);
        Assert.assertTrue("Set up task to be active", chromeAndroidTask.isActive());
        // All tasks must have been active before.
        secondChromeAndroidTask.onTopResumedActivityChangedWithNative(true);
        chromeAndroidTask.onTopResumedActivityChangedWithNative(true);
        secondChromeAndroidTask.onTopResumedActivityChangedWithNative(false);

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
    public void deactivate_alreadyUnfocusedAndOnlyOneTask_shouldDoNothing() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(false);
        Assert.assertFalse("Set up task to be inactive", chromeAndroidTask.isActive());

        // Act.
        chromeAndroidTask.deactivate();

        // Assert
        assertNull(
                "Future state of isActive() should be null when nothing is pending",
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isActiveFuture(chromeAndroidTask.getState()));
        assertFalse("isActive should still be false", chromeAndroidTask.isActive());
    }

    @Test
    public void deactivate_focusedButOnlyOneTask_shouldDoNothing() {
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

        // Assert
        assertNull(
                "Future state of isActive() should be null when nothing is pending",
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isActiveFuture(chromeAndroidTask.getState()));
        assertTrue("isActive should still be true", chromeAndroidTask.isActive());
    }

    @Test
    public void deactivate_notInDesktopWindowingMode_shouldDoNothing() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        var mockActivity = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockActivity;
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(true);
        Assert.assertTrue("Set up task to be active", chromeAndroidTask.isActive());
        when(mockActivity.isInMultiWindowMode()).thenReturn(false);

        // Act.
        chromeAndroidTask.deactivate();

        // Assert
        Assert.assertNull(
                "No future state as deactivate is a no-op in this case",
                chromeAndroidTask
                        .getPendingActionManagerForTesting()
                        .isActiveFuture(chromeAndroidTask.getState()));
        assertTrue(chromeAndroidTask.isActive());
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
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
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
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void maximize_whenPendingUpdate_notAffectIsActive() {
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
        when(mockWindowAndroid.isTopResumedActivity()).thenReturn(false);

        // Act.
        chromeAndroidTask.maximize();
        assertTrue("isMaximized is true while pending", chromeAndroidTask.isMaximized());
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), any());
        promise.fulfill(Pair.create(0, DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX));
        shadowOf(getMainLooper()).idle();

        // Assert
        Assert.assertFalse("Maximize should not activate task", chromeAndroidTask.isActive());
    }

    @Test
    public void maximize_whenPendingCreate_returnCachedMaximizeBound() {
        // Arrange.
        // Cache a maximize bound.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var mockWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        // Create a pending task.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());
        // Act.
        task.maximize();

        // Assert.
        assertEquals(
                ChromeAndroidTaskImpl.convertBoundsInPxToDp(
                        DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX, mockWindowAndroid.getDisplay()),
                task.getBoundsInDp());
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
        Assert.assertEquals(
                "Task should have finished updating", State.IDLE, chromeAndroidTask.getState());
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
        assertEquals(taskBounds, pendingActionManager.getPendingBoundsInDpForTesting());
    }

    @Test
    public void setBounds_whenPendingCreate_emptyBounds_ignoresPendingAction() {
        // Arrange.
        var task =
                new ChromeAndroidTaskImpl(ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo());

        // Act.
        task.setBoundsInDp(new Rect());

        // Assert.
        assertNoPendingActions(task);
        assertEquals(
                "Initial bounds default to empty",
                new Rect(),
                task.getPendingActionManagerForTesting().getFutureBoundsInDp());
    }

    @Test
    public void setBounds_notInDesktopWindowingMode_shouldDoNothing() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockActivity = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockActivity;
        when(mockActivity.isInMultiWindowMode()).thenReturn(false);

        // Act.
        chromeAndroidTask.setBoundsInDp(new Rect());

        // Assert.
        Assert.assertNull(
                "no future state of setBounds() as task is not in desktop windowing mode",
                chromeAndroidTask.getPendingActionManagerForTesting().getFutureBoundsInDp());
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
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void isMaximized_whenSetBoundsPending_returnsBasedOnFutureBounds() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockWindowManager =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockWindowManager;
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var promise = new Promise<Pair<Integer, Rect>>();
        when(apiDelegate.moveTaskToWithPromise(any(), anyInt(), any())).thenReturn(promise);

        // Act.
        chromeAndroidTask.maximize();
        assertTrue("isMaximized is true while pending", chromeAndroidTask.isMaximized());
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), any());
        promise.fulfill(Pair.create(0, DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX));
        shadowOf(getMainLooper()).idle();

        chromeAndroidTask.setBoundsInDp(new Rect(100, 100, 600, 800));
        var mockMaximiumMetrics = mock(WindowMetrics.class);
        when(mockMaximiumMetrics.getBounds()).thenReturn(DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX);
        when(mockWindowManager.getCurrentWindowMetrics()).thenReturn(mockMaximiumMetrics);

        // Assert
        Assert.assertFalse(
                "isMaximized should return false if future bounds don't equal to maximum bound",
                chromeAndroidTask.isMaximized());
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
    public void addActivityScopedObjects_fromPendingState_NoPendingShowToDispatch() {
        int taskId = 2;
        int unusedTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(unusedTaskId);
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(taskId, /* isPendingTask= */ true);
        var pendingTask = (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Arrange: Request SHOW on a pending task.
        pendingTask.show();

        // Arrange: Set up ActivityScopedObjects.
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var mockActivity = activityScopedObjects.mActivityWindowAndroid.getActivity().get();
        var mockActivityManager =
                (ActivityManager) mockActivity.getSystemService(Context.ACTIVITY_SERVICE);

        // Act.
        pendingTask.addActivityScopedObjects(activityScopedObjects);
        pendingTask.onActivityTopResumedChanged(true);
        pendingTask.onTopResumedActivityChangedWithNative(true);

        // Assert.
        verify(mockActivityManager, never().description("The task defaults to be visible"))
                .moveTaskToFront(taskId, 0);
    }

    @Test
    public void addActivityScopedObjects_fromPendingState_dispatchesPendingClose() {
        int taskId = 2;
        int unusedTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(unusedTaskId);
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(taskId, /* isPendingTask= */ true);
        var pendingTask = (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Arrange: Request CLOSE on a pending task.
        pendingTask.close();

        // Arrange: Set up ActivityScopedObjects.
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var mockActivity = activityScopedObjects.mActivityWindowAndroid.getActivity().get();

        // Act.
        pendingTask.addActivityScopedObjects(activityScopedObjects);
        pendingTask.onActivityTopResumedChanged(true);
        pendingTask.onTopResumedActivityChangedWithNative(true);

        // Assert.
        verify(mockActivity).finishAndRemoveTask();
    }

    @Test
    public void addActivityScopedObjects_fromPendingState_NoPendingActivateToDispatch() {
        int taskId = 2;
        int unusedTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(unusedTaskId);
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(taskId, /* isPendingTask= */ true);
        var pendingTask = (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        // Arrange: Request ACTIVATE on a pending task.
        pendingTask.activate();

        // Arrange: Set up ActivityScopedObjects.
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var mockActivity = activityScopedObjects.mActivityWindowAndroid.getActivity().get();

        // Act.
        pendingTask.addActivityScopedObjects(activityScopedObjects);
        pendingTask.onActivityTopResumedChanged(true);
        pendingTask.onTopResumedActivityChangedWithNative(true);

        // Assert.
        verify(mockActivity, never().description("The task defaults to be active"))
                .moveTaskToBack(true);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void addActivityScopedObjects_fromPendingState_dispatchesPendingMaximize() {
        int unusedTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(unusedTaskId);
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        // Arrange: Request MAXIMIZE on a pending task.
        chromeAndroidTask.maximize();

        // Act.
        chromeAndroidTask.addActivityScopedObjects(
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects);
        chromeAndroidTask.onActivityTopResumedChanged(true);
        chromeAndroidTask.onTopResumedActivityChangedWithNative(true);

        // Assert.
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());
        var capturedBounds = boundsCaptor.getValue();
        assertEquals(DEFAULT_MAXIMIZED_WINDOW_BOUNDS_IN_PX, capturedBounds);
    }

    @Test
    public void addActivityScopedObjects_fromPendingState_dispatchesPendingMinimize() {
        int unusedTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(unusedTaskId);
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        // Arrange: Request MINIMIZE on a pending task.
        chromeAndroidTask.minimize();
        // Arrange: Setup ActivityScopedObjects.
        int taskId = 2;
        var profile = chromeAndroidTaskWithMockDeps.mMockProfile;
        var activityScopedObjects = createActivityScopedObjects(taskId, profile);
        var mockActivity = activityScopedObjects.mActivityWindowAndroid.getActivity().get();

        // Act.
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects);
        chromeAndroidTask.onActivityTopResumedChanged(true);
        chromeAndroidTask.onTopResumedActivityChangedWithNative(true);

        // Assert.
        verify(mockActivity).moveTaskToBack(true);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void
            addActivityScopedObjects_fromPendingState_withNonEmptyPendingBounds_dispatchesPendingRestore() {
        int unusedTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(unusedTaskId);
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
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
        chromeAndroidTask.addActivityScopedObjects(
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects);
        chromeAndroidTask.onActivityTopResumedChanged(true);
        chromeAndroidTask.onTopResumedActivityChangedWithNative(true);

        // Assert.
        Rect expectedBoundsInPx = DisplayUtil.scaleToEnclosingRect(pendingBoundsInDp, dipScale);
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());
        assertEquals(expectedBoundsInPx, boundsCaptor.getValue());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void
            addActivityScopedObjects_fromPendingState_withEmptyPendingBounds_ignoresPendingRestore() {
        int unusedTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(unusedTaskId);
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var apiDelegate = chromeAndroidTaskWithMockDeps.mMockAconfigFlaggedApiDelegate;
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        // Arrange: Request RESTORE on a pending task.
        chromeAndroidTask.restore();

        // Act.
        chromeAndroidTask.addActivityScopedObjects(
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects);

        // Assert.
        verify(apiDelegate, never()).moveTaskToWithPromise(any(), anyInt(), any());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void addActivityScopedObjects_fromPendingState_dispatchesPendingPushBounds() {
        int unusedTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(unusedTaskId);
        // Arrange: Create pending task.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 1, /* isPendingTask= */ true);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
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
        chromeAndroidTask.addActivityScopedObjects(
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects);
        chromeAndroidTask.onActivityTopResumedChanged(true);
        chromeAndroidTask.onTopResumedActivityChangedWithNative(true);

        // Assert.
        Rect expectedBoundsInPx = DisplayUtil.scaleToEnclosingRect(pendingBoundsInDp, dipScale);
        var boundsCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(apiDelegate).moveTaskToWithPromise(any(), anyInt(), boundsCaptor.capture());
        assertEquals(expectedBoundsInPx, boundsCaptor.getValue());
    }

    @Test
    public void addActivityScopedObjects_fromPendingState_invokesCallback() {
        // Arrange: Create pending task with a callback.
        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();
        var pendingTaskInfo = ChromeAndroidTaskUnitTestSupport.createPendingTaskInfo();
        var task = new ChromeAndroidTaskImpl(pendingTaskInfo);

        // Arrange: Setup ActivityScopedObjects.
        int taskId = 2;
        var profile = pendingTaskInfo.mCreateParams.getProfile();
        var activityScopedObjects = createActivityScopedObjects(taskId, profile);

        // Act.
        task.addActivityScopedObjects(activityScopedObjects);
        task.onActivityTopResumedChanged(true);
        task.onTopResumedActivityChangedWithNative(true);

        // Assert.
        verify(pendingTaskInfo.mTaskCreationCallbackForNative)
                .onResult(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }

    @Test
    public void addActivityScopedObjects_fromPendingState_waitsForBothSignals() {
        int existingTaskId = 2;
        int pendingTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(existingTaskId);

        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(pendingTaskId, /* isPendingTask= */ true);
        var pendingTask = (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;

        // Act & Assert 1: Add activity objects. Still pending.
        pendingTask.addActivityScopedObjects(activityScopedObjects);
        assertEquals(State.PENDING_CREATE, pendingTask.getState());

        // Act & Assert 2: Only onActivityTopResumedChanged. Still pending.
        pendingTask.onActivityTopResumedChanged(true);
        assertEquals(State.PENDING_CREATE, pendingTask.getState());

        // Act & Assert 3: onTopResumedActivityChangedWithNative. Now IDLE.
        pendingTask.onTopResumedActivityChangedWithNative(true);
        assertEquals(State.IDLE, pendingTask.getState());
        assertEquals(pendingTaskId, (int) pendingTask.getId());
    }

    @Test
    public void addActivityScopedObjects_fromPendingState_waitsForBothSignals_reverseOrder() {
        int existingTaskId = 2;
        int pendingTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(existingTaskId);

        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(pendingTaskId, /* isPendingTask= */ true);
        var pendingTask = (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;

        // Act & Assert 1: Add activity objects. Still pending.
        pendingTask.addActivityScopedObjects(activityScopedObjects);
        assertEquals(State.PENDING_CREATE, pendingTask.getState());

        // Act & Assert 2: Only onTopResumedActivityChangedWithNative. Still pending.
        pendingTask.onTopResumedActivityChangedWithNative(true);
        assertEquals(State.PENDING_CREATE, pendingTask.getState());

        // Act & Assert 3: onActivityTopResumedChanged. Now IDLE.
        pendingTask.onActivityTopResumedChanged(true);
        assertEquals(State.IDLE, pendingTask.getState());
        assertEquals(pendingTaskId, (int) pendingTask.getId());
    }

    @Test
    public void
            addActivityScopedObjects_fromPendingState_activityAlreadyResumed_completesPendingCreate() {
        int existingTaskId = 2;
        int pendingTaskId = 3;

        // Arrange: Creating a pending task requires an existing task.
        createChromeAndroidTaskWithMockDeps(existingTaskId);

        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                createChromeAndroidTaskWithMockDeps(pendingTaskId, /* isPendingTask= */ true);
        var pendingTask = (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var activityScopedObjects = chromeAndroidTaskWithMockDeps.mActivityScopedObjects;
        var activityWindowAndroidMocks = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks;

        // Mock the activity to be already resumed.
        when(activityWindowAndroidMocks.mMockActivityWindowAndroid.isTopResumedActivity())
                .thenReturn(true);
        when(activityWindowAndroidMocks.mMockActivityLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(ActivityLifecycleDispatcher.ActivityState.RESUMED_WITH_NATIVE);

        // Act.
        pendingTask.addActivityScopedObjects(activityScopedObjects);

        // Assert.
        assertEquals(State.IDLE, pendingTask.getState());
        assertEquals(pendingTaskId, (int) pendingTask.getId());
    }

    @Test
    public void addFeature_invokesOnTabModelSelected() throws Exception {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var expectedTabModel =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mTabModelSelector
                        .getCurrentModel();

        // Act.
        chromeAndroidTask.addFeature(
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null),
                () -> testFeature);

        // Assert.
        assertEquals(1, testFeature.mTabModelSelectedHistory.size());
        assertEquals(expectedTabModel, testFeature.mTabModelSelectedHistory.get(0));
    }

    @Test
    public void onTabModelSelected_invokedWhenTabModelChanges() throws Exception {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var tabModelSelector =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mTabModelSelector;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        chromeAndroidTask.addFeature(
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null),
                () -> testFeature);
        assertEquals(1, testFeature.mTabModelSelectedHistory.size());

        var newTabModel = mock(TabModel.class);
        when(tabModelSelector.getCurrentModel()).thenReturn(newTabModel);

        // Act.
        // Simulate a tab model change by invoking the observer callback.
        ((SettableMonotonicObservableSupplier<TabModel>)
                        tabModelSelector.getCurrentTabModelSupplier())
                .set(newTabModel);

        // Assert.
        assertEquals(2, testFeature.mTabModelSelectedHistory.size());
        assertEquals(newTabModel, testFeature.mTabModelSelectedHistory.get(1));
    }

    @Test
    public void onIncognitoTabModelCreated_associatesIncognitoTabModelWithNativeBrowserWindow() {
        // Arrange
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1,
                        /* isPendingTask= */ false,
                        /* isDesktopMode= */ true,
                        SupportedProfileType.MIXED);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var tabModelSelector =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mTabModelSelector;
        var incognitoTabModel = (IncognitoTabModel) tabModelSelector.getModel(true);
        var incognitoProfile = mock(Profile.class, "IncognitoProfile");
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);

        ArgumentCaptor<IncognitoTabModelObserver> incognitoObserverCaptor =
                ArgumentCaptor.forClass(IncognitoTabModelObserver.class);
        verify(incognitoTabModel).addIncognitoObserver(incognitoObserverCaptor.capture());

        // Act
        when(incognitoTabModel.getProfile()).thenReturn(incognitoProfile);
        incognitoObserverCaptor.getValue().onIncognitoModelCreated();

        // Assert
        verify(incognitoTabModel)
                .associateWithBrowserWindow(
                        ChromeAndroidTaskUnitTestSupport
                                .FAKE_INCOGNITO_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        var allPtrs = chromeAndroidTask.getAllNativeBrowserWindowPtrs();
        assertEquals(2, allPtrs.size());
        assertTrue(allPtrs.contains(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR));
        assertTrue(
                allPtrs.contains(
                        ChromeAndroidTaskUnitTestSupport
                                .FAKE_INCOGNITO_NATIVE_ANDROID_BROWSER_WINDOW_PTR));
    }

    @Test
    public void onIncognitoTabModelDidBecomeEmpty_destroysIncognitoBrowserWindow() {
        // Arrange
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1,
                        /* isPendingTask= */ false,
                        /* isDesktopMode= */ true,
                        SupportedProfileType.MIXED);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var tabModelSelector =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mTabModelSelector;
        var incognitoTabModel = (IncognitoTabModel) tabModelSelector.getModel(true);
        var incognitoProfile = mock(Profile.class, "IncognitoProfile");
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);

        ArgumentCaptor<IncognitoTabModelObserver> incognitoObserverCaptor =
                ArgumentCaptor.forClass(IncognitoTabModelObserver.class);
        verify(incognitoTabModel).addIncognitoObserver(incognitoObserverCaptor.capture());

        when(incognitoTabModel.getProfile()).thenReturn(incognitoProfile);
        incognitoObserverCaptor.getValue().onIncognitoModelCreated();

        var allPtrs = chromeAndroidTask.getAllNativeBrowserWindowPtrs();
        assertEquals(2, allPtrs.size());

        // Act
        incognitoObserverCaptor.getValue().didBecomeEmpty();

        // Assert
        verify(incognitoTabModel).dissociateWithBrowserWindow();
        var allPtrsAfter = chromeAndroidTask.getAllNativeBrowserWindowPtrs();
        assertEquals(1, allPtrsAfter.size());
        assertTrue(allPtrsAfter.contains(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR));
        verify(chromeAndroidTaskWithMockDeps.mMockAndroidBrowserWindowNatives, times(1))
                .destroy(
                        ChromeAndroidTaskUnitTestSupport
                                .FAKE_INCOGNITO_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }

    @Test
    public void onIncognitoTabModelDidBecomeEmpty_removesTabModelScopedFeature() throws Exception {
        assumeFalse(BuildConfig.IS_DESKTOP_ANDROID);

        // Arrange
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1,
                        /* isPendingTask= */ false,
                        /* isDesktopMode= */ true,
                        SupportedProfileType.MIXED);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var tabModelSelector =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mTabModelSelector;
        var incognitoTabModel = (IncognitoTabModel) tabModelSelector.getModel(true);
        var incognitoProfile = mock(Profile.class, "IncognitoProfile");
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);

        ArgumentCaptor<IncognitoTabModelObserver> incognitoObserverCaptor =
                ArgumentCaptor.forClass(IncognitoTabModelObserver.class);
        verify(incognitoTabModel).addIncognitoObserver(incognitoObserverCaptor.capture());

        when(incognitoTabModel.getProfile()).thenReturn(incognitoProfile);
        incognitoObserverCaptor.getValue().onIncognitoModelCreated();

        var tabModelScopedFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var tabModelScopedFeatureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class,
                        /* profile= */ null,
                        /* activityWindowAndroid= */ null,
                        incognitoTabModel);
        chromeAndroidTask.addFeature(tabModelScopedFeatureKey, () -> tabModelScopedFeature);

        var nonTabModelScopedFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        var nonTabModelScopedFeatureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null);
        chromeAndroidTask.addFeature(nonTabModelScopedFeatureKey, () -> nonTabModelScopedFeature);

        // Act
        incognitoObserverCaptor.getValue().didBecomeEmpty();

        // Assert
        tabModelScopedFeature.mOnFeatureRemovedHelper.waitForCallback(0, 1);
        assertNull(chromeAndroidTask.getFeatureForTesting(tabModelScopedFeatureKey));
        assertEquals(
                nonTabModelScopedFeature,
                chromeAndroidTask.getFeatureForTesting(nonTabModelScopedFeatureKey));
        assertEquals(0, nonTabModelScopedFeature.mOnFeatureRemovedHelper.getCallCount());
    }

    @Test
    public void onProfileDestroyed_removesBrowserWindow() {
        // TODO(crbug.com/479566813): Until clients are ported to properly destroy themselves on
        // profile
        // destruction, this test needs to be disabled.
        assumeFalse(BuildConfig.IS_DESKTOP_ANDROID);

        // Arrange
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1,
                        /* isPendingTask= */ false,
                        /* isDesktopMode= */ true,
                        SupportedProfileType.MIXED);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var tabModelSelector =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mTabModelSelector;
        var incognitoTabModel = (IncognitoTabModel) tabModelSelector.getModel(true);
        var incognitoProfile = mock(Profile.class, "IncognitoProfile");
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);

        ArgumentCaptor<IncognitoTabModelObserver> incognitoObserverCaptor =
                ArgumentCaptor.forClass(IncognitoTabModelObserver.class);
        verify(incognitoTabModel).addIncognitoObserver(incognitoObserverCaptor.capture());
        when(incognitoTabModel.getProfile()).thenReturn(incognitoProfile);
        incognitoObserverCaptor.getValue().onIncognitoModelCreated();

        assertNotNull(chromeAndroidTask.getSessionIdForTesting(incognitoProfile));
        assertEquals(2, chromeAndroidTask.getAllNativeBrowserWindowPtrs().size());

        // Act
        ProfileManager.onProfileDestroyed(incognitoProfile);

        // Assert
        verify(incognitoTabModel).dissociateWithBrowserWindow();
        assertNull(
                "Browser window for destroyed profile should be removed.",
                chromeAndroidTask.getSessionIdForTesting(incognitoProfile));
        var allPtrs = chromeAndroidTask.getAllNativeBrowserWindowPtrs();
        assertEquals(1, allPtrs.size());
        assertEquals((Long) FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR, allPtrs.get(0));
    }

    @Test
    public void onProfileDestroyed_mixedProfile_removesOnlyDestroyedProfileWindow() {
        assumeFalse(BuildConfig.IS_DESKTOP_ANDROID);

        // Arrange: Create Task with Mixed Profile support.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1,
                        /* isPendingTask= */ false,
                        /* isDesktopMode= */ true,
                        SupportedProfileType.MIXED);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var tabModelSelector =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mTabModelSelector;
        var mockNatives = chromeAndroidTaskWithMockDeps.mMockAndroidBrowserWindowNatives;

        // Simulate Incognito creation.
        var incognitoModel = (IncognitoTabModel) tabModelSelector.getModel(true);
        var incognitoProfile = mock(Profile.class, "IncognitoProfile");
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);
        when(incognitoModel.getProfile()).thenReturn(incognitoProfile);

        ArgumentCaptor<IncognitoTabModelObserver> captor =
                ArgumentCaptor.forClass(IncognitoTabModelObserver.class);
        verify(incognitoModel).addIncognitoObserver(captor.capture());
        captor.getValue().onIncognitoModelCreated();

        assertEquals(
                "Both regular and incognito windows should exist",
                2,
                chromeAndroidTask.getAllNativeBrowserWindowPtrs().size());
        assertEquals(
                "There should be 1 ActivityScopedObjects wrapping both windows",
                1,
                chromeAndroidTask.getActivityScopedObjectsListForTesting().size());

        // Act: Destroy ONLY the Incognito profile.
        ProfileManager.onProfileDestroyed(incognitoProfile);

        // Assert:
        // 1. Incognito window was destroyed.
        verify(mockNatives, times(1))
                .destroy(
                        ChromeAndroidTaskUnitTestSupport
                                .FAKE_INCOGNITO_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        // 2. Regular window was NOT destroyed.
        verify(mockNatives, never()).destroy(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

        // 3. The ActivityScopedObjects wrapper should STILL exist because the regular window is
        // still alive.
        assertEquals(
                "The Activity wrapper should not have been removed",
                1,
                chromeAndroidTask.getActivityScopedObjectsListForTesting().size());

        // 4. Only 1 native pointer (the regular one) should remain tracked.
        assertEquals(1, chromeAndroidTask.getAllNativeBrowserWindowPtrs().size());
    }

    @Test
    public void onProfileDestroyed_whenTaskIsPendingCreate_destroysPendingBrowserWindow() {
        // TODO(crbug.com/479566813): Re-enable for Desktop Android when fixed.
        assumeFalse(BuildConfig.IS_DESKTOP_ANDROID);

        // Arrange: Creating a pending task requires an existing task to generate the Intent.
        createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);

        // Arrange: Create the pending task.
        var pendingTaskWithDeps =
                createChromeAndroidTaskWithMockDeps(/* taskId= */ 2, /* isPendingTask= */ true);
        var pendingTask = (ChromeAndroidTaskImpl) pendingTaskWithDeps.mChromeAndroidTask;
        var profile = pendingTaskWithDeps.mMockProfile;
        var mockNatives = pendingTaskWithDeps.mMockAndroidBrowserWindowNatives;

        assertEquals(
                "Pending task should track exactly 1 native window pointer",
                1,
                pendingTask.getAllNativeBrowserWindowPtrs().size());

        // Extract the created native pointer to verify it gets destroyed.
        long pendingWindowPtr = pendingTask.getAllNativeBrowserWindowPtrs().get(0);

        // Act: Destroy the profile before the task attaches to an Activity.
        ProfileManager.onProfileDestroyed(profile);

        // Assert: The pending window should be destroyed and cleared.
        assertEquals(
                "Pending window should be cleared",
                0,
                pendingTask.getAllNativeBrowserWindowPtrs().size());

        // Verify native destroy was actually called.
        verify(mockNatives, times(1)).destroy(pendingWindowPtr);
    }

    @Test
    public void addActivityScopedObjects_invokesOnTabModelSelectedOnFeatures() throws Exception {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var testFeature = new TestChromeAndroidTaskFeature(chromeAndroidTask);
        chromeAndroidTask.addFeature(
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, /* profile= */ null),
                () -> testFeature);
        assertEquals(1, testFeature.mTabModelSelectedHistory.size());

        var activityScopedObjects2 = createActivityScopedObjects(taskId);
        var expectedTabModel = activityScopedObjects2.mTabModelSelector.getCurrentModel();

        // Act.
        chromeAndroidTask.addActivityScopedObjects(activityScopedObjects2);

        // Assert.
        assertEquals(2, testFeature.mTabModelSelectedHistory.size());
        assertEquals(expectedTabModel, testFeature.mTabModelSelectedHistory.get(1));
    }

    @Test
    public void addAndroidBrowserWindowObserver_doesNotNotifyForExistingWindows() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var observer = mock(AndroidBrowserWindowObserver.class);

        // Act.
        chromeAndroidTask.addAndroidBrowserWindowObserver(observer);

        // Assert.
        verify(observer, never()).onBrowserWindowAdded(any(Long.class));
    }

    @Test
    public void androidBrowserWindowObserver_notifiedOnDestruction() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var observer = mock(AndroidBrowserWindowObserver.class);
        chromeAndroidTask.addAndroidBrowserWindowObserver(observer);

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        verify(observer, times(1)).onBrowserWindowRemoved(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }

    @Test
    public void androidBrowserWindowObserver_notifiedOnAddition() {
        // Arrange
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1,
                        /* isPendingTask= */ false,
                        /* isDesktopMode= */ true,
                        SupportedProfileType.MIXED);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var observer = mock(AndroidBrowserWindowObserver.class);
        chromeAndroidTask.addAndroidBrowserWindowObserver(observer);

        var tabModelSelector =
                chromeAndroidTaskWithMockDeps.mActivityScopedObjects.mTabModelSelector;
        var incognitoTabModel = (IncognitoTabModel) tabModelSelector.getModel(true);
        var incognitoProfile = mock(Profile.class, "IncognitoProfile");
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);

        ArgumentCaptor<IncognitoTabModelObserver> incognitoObserverCaptor =
                ArgumentCaptor.forClass(IncognitoTabModelObserver.class);
        verify(incognitoTabModel).addIncognitoObserver(incognitoObserverCaptor.capture());

        // Act
        when(incognitoTabModel.getProfile()).thenReturn(incognitoProfile);
        incognitoObserverCaptor.getValue().onIncognitoModelCreated();

        // Assert
        verify(observer, times(1))
                .onBrowserWindowAdded(
                        ChromeAndroidTaskUnitTestSupport
                                .FAKE_INCOGNITO_NATIVE_ANDROID_BROWSER_WINDOW_PTR);
    }

    @Test
    public void removeAndroidBrowserWindowObserver_stopsNotifications() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps = createChromeAndroidTaskWithMockDeps(/* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var observer = mock(AndroidBrowserWindowObserver.class);
        chromeAndroidTask.addAndroidBrowserWindowObserver(observer);
        chromeAndroidTask.removeAndroidBrowserWindowObserver(observer);

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        verify(observer, never()).onBrowserWindowRemoved(any(Long.class));
    }

    private static final class TestChromeAndroidTaskFeature implements ChromeAndroidTaskFeature {

        final CallbackHelper mOnFeatureRemovedHelper = new CallbackHelper();

        /** Records the {@code nativeBrowserWindowPtr} passed to {@link #onAddedToTask(long)}. */
        final List<Long> mOnAddedToTaskHistory = new ArrayList<>();

        /** Records the bounds passed to {@link #onTaskBoundsChanged}. */
        final List<Rect> mTaskBoundsChangeHistory = new ArrayList<>();

        /** Records the {@code hasFocus} values passed to {@link #onTaskFocusChanged}. */
        final List<Boolean> mTaskFocusChangeHistory = new ArrayList<>();

        /** Records the {@code isVisible} values passed to {@link #onTaskVisibilityChanged}. */
        final List<Boolean> mTaskVisibilityChangeHistory = new ArrayList<>();

        /** Records the {@link TabModel} passed to {@link #onTabModelSelected}. */
        final List<TabModel> mTabModelSelectedHistory = new ArrayList<>();

        /**
         * If true, enable the malicious behavior: add the feature itself to {@link
         * ChromeAndroidTask} in {@link #onFeatureRemoved()}.
         */
        boolean mShouldRefuseToBeRemoved;

        private final ChromeAndroidTask mChromeAndroidTask;

        TestChromeAndroidTaskFeature(ChromeAndroidTask chromeAndroidTask) {
            mChromeAndroidTask = chromeAndroidTask;
        }

        @Override
        public void onAddedToTask(long nativeBrowserWindowPtr) {
            mOnAddedToTaskHistory.add(nativeBrowserWindowPtr);
        }

        @Override
        public void onFeatureRemoved() {
            mOnFeatureRemovedHelper.notifyCalled();

            if (mShouldRefuseToBeRemoved) {
                var featureKey =
                        new ChromeAndroidTaskFeatureKey(
                                TestChromeAndroidTaskFeature.class, /* profile= */ null);
                mChromeAndroidTask.addFeature(featureKey, () -> this);
            }
        }

        @Override
        public void onTaskBoundsChanged(Rect newBoundsInDp) {
            mTaskBoundsChangeHistory.add(newBoundsInDp);
        }

        @Override
        public void onTaskFocusChanged(boolean hasFocus) {
            mTaskFocusChangeHistory.add(hasFocus);
        }

        @Override
        public void onTaskVisibilityChanged(boolean isVisible) {
            mTaskVisibilityChangeHistory.add(isVisible);
        }

        @Override
        public void onTabModelSelected(TabModel tabModel) {
            mTabModelSelectedHistory.add(tabModel);
        }
    }
}
