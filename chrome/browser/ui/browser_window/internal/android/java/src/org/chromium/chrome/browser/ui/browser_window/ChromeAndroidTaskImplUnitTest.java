// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assertNonNull;

import android.annotation.SuppressLint;
import android.app.ActivityOptions;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.WindowInsets;
import android.view.WindowMetrics;

import androidx.core.view.WindowInsetsCompat;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.insets.InsetObserver;

import java.util.Arrays;

@RunWith(BaseRobolectricTestRunner.class)
public class ChromeAndroidTaskImplUnitTest {

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static ChromeAndroidTaskImpl createChromeAndroidTask() {
        return createChromeAndroidTask(/* taskId= */ 1);
    }

    private static ChromeAndroidTaskImpl createChromeAndroidTask(int taskId) {
        var chromeAndroidTask =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(taskId)
                        .mChromeAndroidTask;
        assert chromeAndroidTask instanceof ChromeAndroidTaskImpl;
        return (ChromeAndroidTaskImpl) chromeAndroidTask;
    }

    @Test
    public void constructor_setsActivityWindowAndroid() {
        // Arrange.
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);
        var profile = mock(Profile.class);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(profile);

        // Act.
        var chromeAndroidTask =
                new ChromeAndroidTaskImpl(
                        BrowserWindowType.NORMAL, activityWindowAndroid, tabModel);

        // Assert.
        assertEquals(activityWindowAndroid, chromeAndroidTask.getActivityWindowAndroid());
    }

    @Test
    public void constructor_registersActivityLifecycleObservers() {
        // Arrange & Act.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
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
    public void constructor_setsTabModelRefAndRegistersTabModelObserver() {
        // Arrange & Act.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockTabModel = chromeAndroidTaskWithMockDeps.mMockTabModel;

        // Assert.
        assertEquals(mockTabModel, chromeAndroidTask.getObservedTabModelForTesting());
        verify(mockTabModel, times(1)).addObserver(chromeAndroidTask);
    }

    @Test
    public void getProfile_returnsInitialProfile() {
        // Arrange.
        var profile = mock(Profile.class);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(profile);
        var chromeAndroidTask =
                new ChromeAndroidTaskImpl(
                        BrowserWindowType.NORMAL,
                        ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(1),
                        tabModel);

        // Act & Assert.
        assertEquals(
                "The returned Profile should be the same as the one from the constructor.",
                profile,
                chromeAndroidTask.getProfile());
    }

    @Test
    public void didAddTab_withDifferentProfile_throwsAssertionError() {
        // Arrange.
        var initialProfile = mock(Profile.class, "InitialProfile");
        var differentProfile = mock(Profile.class, "DifferentProfile");
        var tabWithDifferentProfile = mock(Tab.class);
        when(tabWithDifferentProfile.getProfile()).thenReturn(differentProfile);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(initialProfile);

        var chromeAndroidTask =
                new ChromeAndroidTaskImpl(
                        BrowserWindowType.NORMAL,
                        ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(1),
                        tabModel);

        // Act & Assert.
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
        var chromeAndroidTask = createChromeAndroidTask(taskId);

        // Act & Assert.
        assertEquals(taskId, chromeAndroidTask.getId());
    }

    @Test
    public void setActivityWindowAndroid_refAlreadyExists_throwsException() {
        // Arrange.
        var chromeAndroidTask = createChromeAndroidTask();
        var newActivityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 1);

        // Act & Assert.
        assertThrows(
                AssertionError.class,
                () ->
                        chromeAndroidTask.setActivityWindowAndroid(
                                newActivityWindowAndroid, mock(TabModel.class)));
    }

    @Test
    public void setActivityWindowAndroid_previousRefCleared_setsNewRef() {
        // Arrange.
        int taskId = 1;
        var chromeAndroidTask = createChromeAndroidTask(taskId);
        var newActivityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
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
        var chromeAndroidTask = createChromeAndroidTask(taskId);
        var newActivityWindowAndroidMocks =
                ChromeAndroidTaskUnitTestSupport.createActivityWindowAndroidMocks(taskId);
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
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(taskId);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var oldMockTabModel = chromeAndroidTaskWithMockDeps.mMockTabModel;

        var newActivityWindowAndroidMocks =
                ChromeAndroidTaskUnitTestSupport.createActivityWindowAndroidMocks(taskId);
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
        var chromeAndroidTask = createChromeAndroidTask();
        var newActivityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(/* taskId= */ 2);
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
        var chromeAndroidTask = createChromeAndroidTask(taskId);
        chromeAndroidTask.destroy();

        // Act & Assert.
        var newActivityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        assertThrows(
                AssertionError.class,
                () ->
                        chromeAndroidTask.setActivityWindowAndroid(
                                newActivityWindowAndroid, mock(TabModel.class)));
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
    public void clearActivityWindowAndroid_unregistersActivityLifecycleObservers() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
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
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
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
        var chromeAndroidTask = createChromeAndroidTask();
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
        var chromeAndroidTask = createChromeAndroidTask();
        var mockFeature = mock(ChromeAndroidTaskFeature.class);

        // Act.
        chromeAndroidTask.addFeature(mockFeature);

        // Assert.
        verify(mockFeature, times(1)).onAddedToTask();
    }

    @Test
    public void addFeature_calledAfterTaskDestroyed_throwsException() {
        // Arrange.
        var chromeAndroidTask = createChromeAndroidTask();
        chromeAndroidTask.destroy();

        // Act & Assert.
        var mockFeature = mock(ChromeAndroidTaskFeature.class);
        assertThrows(AssertionError.class, () -> chromeAndroidTask.addFeature(mockFeature));
    }

    @Test
    public void getOrCreateNativeBrowserWindowPtr_returnsPtrValue() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
        var chromeAndroidTask = chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

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
        var chromeAndroidTask = createChromeAndroidTask();
        chromeAndroidTask.destroy();

        // Act & Assert.
        assertThrows(
                AssertionError.class, () -> chromeAndroidTask.getOrCreateNativeBrowserWindowPtr());
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
    public void destroy_destroysAllFeatures() {
        // Arrange.
        var chromeAndroidTask = createChromeAndroidTask();
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
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
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
        var chromeAndroidTask = createChromeAndroidTask();
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
        var chromeAndroidTask = createChromeAndroidTask();
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
        assertThrows(AssertionError.class, () -> chromeAndroidTask.destroy());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void onConfigurationChanged_windowBoundsChanged_invokesOnTaskBoundsChangedForFeature() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
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
        var inOrder = Mockito.inOrder(mockFeature);
        inOrder.verify(mockFeature).onTaskBoundsChanged(taskBounds1);
        inOrder.verify(mockFeature).onTaskBoundsChanged(taskBounds2);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void
            onConfigurationChanged_windowBoundsDoesNotChange_doesNotInvokeOnTaskBoundsChangedForFeature() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;

        var mockFeature = mock(ChromeAndroidTaskFeature.class);
        chromeAndroidTask.addFeature(mockFeature);

        var mockWindowManager =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockWindowManager;
        var mockWindowMetrics = mock(WindowMetrics.class);
        var taskBounds = new Rect(0, 0, 800, 600);
        when(mockWindowMetrics.getBounds()).thenReturn(taskBounds);
        when(mockWindowManager.getCurrentWindowMetrics()).thenReturn(mockWindowMetrics);

        // Act.
        chromeAndroidTask.onConfigurationChanged(new Configuration());
        chromeAndroidTask.onConfigurationChanged(new Configuration());

        // Assert:
        // Only the first onConfigurationChanged() should trigger onTaskBoundsChanged() as the
        // second onConfigurationChanged() doesn't include a change in window bounds.
        verify(mockFeature, times(1)).onTaskBoundsChanged(taskBounds);
    }

    @Test
    public void onTopResumedActivityChanged_activityIsTopResumed_updatesLastActivatedTime() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
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
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
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
        var chromeAndroidTask = createChromeAndroidTask();
        var mockFeature = mock(ChromeAndroidTaskFeature.class);
        chromeAndroidTask.addFeature(mockFeature);

        // Act.
        chromeAndroidTask.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
        chromeAndroidTask.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ false);

        // Assert.
        InOrder inOrder = Mockito.inOrder(mockFeature);
        inOrder.verify(mockFeature).onTaskFocusChanged(true);
        inOrder.verify(mockFeature).onTaskFocusChanged(false);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void maximize_maximizeToMaximizedBounds() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockActivityWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        var mockActivity = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockActivity;
        var mockWindowManager =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockWindowManager;

        // Mock isInDesktopWindowingMode() to return true.
        var mockWindowInsetsCompat = mock(WindowInsetsCompat.class);
        var mockInsetObserver = mock(InsetObserver.class);
        when(mockActivityWindowAndroid.getInsetObserver()).thenReturn(mockInsetObserver);
        when(mockInsetObserver.getLastRawWindowInsets()).thenReturn(mockWindowInsetsCompat);
        when(mockWindowInsetsCompat.isVisible(WindowInsets.Type.captionBar())).thenReturn(true);

        // Mock getMaximizedBounds().
        var mockMaxWindowMetrics = mock(WindowMetrics.class);
        var mockMaxWindowInsets = mock(WindowInsets.class);
        when(mockWindowManager.getMaximumWindowMetrics()).thenReturn(mockMaxWindowMetrics);
        when(mockMaxWindowMetrics.getWindowInsets()).thenReturn(mockMaxWindowInsets);
        var tappableInsets = Insets.of(0, 10, 0, 20);
        when(mockMaxWindowInsets.getInsets(WindowInsets.Type.tappableElement()))
                .thenReturn(tappableInsets);
        var fullscreenBounds = new Rect(0, 0, 1920, 1080);
        when(mockMaxWindowMetrics.getBounds()).thenReturn(fullscreenBounds);
        var maximizedBounds = new Rect(0, 10, 1920, 1060);

        // Act.
        chromeAndroidTask.maximize();

        // Assert.
        var intentCaptor = ArgumentCaptor.forClass(Intent.class);
        var bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mockActivity).startActivity(intentCaptor.capture(), bundleCaptor.capture());

        var capturedIntent = intentCaptor.getValue();
        assertEquals(
                Intent.FLAG_ACTIVITY_CLEAR_TOP,
                capturedIntent.getFlags() & Intent.FLAG_ACTIVITY_CLEAR_TOP);

        var capturedBundle = bundleCaptor.getValue();
        Rect capturedBounds = capturedBundle.getParcelable(ActivityOptions.KEY_LAUNCH_BOUNDS);
        assertEquals(maximizedBounds, capturedBounds);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void isMaximized_falseWhenNotMaximized() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockActivityWindowAndroid =
                chromeAndroidTaskWithMockDeps
                        .mActivityWindowAndroidMocks
                        .mMockActivityWindowAndroid;
        var mockWindowManager =
                chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockWindowManager;

        // Mock isInDesktopWindowingMode() to return true.
        var mockWindowMetrics = mock(WindowMetrics.class);
        var mockWindowInsets = mock(WindowInsetsCompat.class);
        var mockInsetObserver = mock(InsetObserver.class);
        when(mockActivityWindowAndroid.getInsetObserver()).thenReturn(mockInsetObserver);
        when(mockInsetObserver.getLastRawWindowInsets()).thenReturn(mockWindowInsets);
        when(mockWindowInsets.isVisible(WindowInsets.Type.captionBar())).thenReturn(true);


        // Mock getBounds() to return non-maximized bounds.
        var currentBounds = new Rect(0, 0, 800, 600);
        when(mockWindowManager.getCurrentWindowMetrics()).thenReturn(mockWindowMetrics);
        when(mockWindowMetrics.getBounds()).thenReturn(currentBounds);

        // Mock getMaximizedBounds().
        var mockMaxWindowMetrics = mock(WindowMetrics.class);
        var mockMaxWindowInsets = mock(WindowInsets.class);
        when(mockWindowManager.getMaximumWindowMetrics()).thenReturn(mockMaxWindowMetrics);
        when(mockMaxWindowMetrics.getWindowInsets()).thenReturn(mockMaxWindowInsets);
        var tappableInsets = Insets.of(0, 10, 0, 20);
        when(mockMaxWindowInsets.getInsets(WindowInsets.Type.tappableElement()))
                .thenReturn(tappableInsets);
        var fullscreenBounds = new Rect(0, 0, 1920, 1080);
        when(mockMaxWindowMetrics.getBounds()).thenReturn(fullscreenBounds);

        // Act & Assert.
        assertFalse(chromeAndroidTask.isMaximized());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    @SuppressLint("NewApi" /* @Config already specifies the required SDK */)
    public void setBounds_setsNewBounds() {
        // Arrange.
        var chromeAndroidTaskWithMockDeps =
                ChromeAndroidTaskUnitTestSupport.createChromeAndroidTaskWithMockDeps(
                        /* taskId= */ 1);
        var chromeAndroidTask =
                (ChromeAndroidTaskImpl) chromeAndroidTaskWithMockDeps.mChromeAndroidTask;
        var mockActivity = chromeAndroidTaskWithMockDeps.mActivityWindowAndroidMocks.mMockActivity;
        var newBounds = new Rect(10, 20, 800, 600);

        // Act.
        chromeAndroidTask.setBounds(newBounds);

        // Assert.
        var intentCaptor = ArgumentCaptor.forClass(Intent.class);
        var bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mockActivity).startActivity(intentCaptor.capture(), bundleCaptor.capture());

        var capturedIntent = intentCaptor.getValue();
        assertEquals(
                Intent.FLAG_ACTIVITY_CLEAR_TOP,
                capturedIntent.getFlags() & Intent.FLAG_ACTIVITY_CLEAR_TOP);

        var capturedBundle = bundleCaptor.getValue();
        Rect capturedBounds = capturedBundle.getParcelable(ActivityOptions.KEY_LAUNCH_BOUNDS);
        assertEquals(newBounds, capturedBounds);
    }
}
