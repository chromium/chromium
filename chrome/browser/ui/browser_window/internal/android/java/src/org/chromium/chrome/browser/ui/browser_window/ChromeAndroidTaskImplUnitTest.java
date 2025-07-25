// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Arrays;

@RunWith(BaseRobolectricTestRunner.class)
public class ChromeAndroidTaskImplUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final long FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR = 42;

    private final AndroidBrowserWindow.Natives mMockAndroidBrowserWindowNatives =
            ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives(
                    FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR);

    private static ChromeAndroidTaskImpl createChromeAndroidTask() {
        return createChromeAndroidTask(/* taskId= */ 1);
    }

    private static ChromeAndroidTaskImpl createChromeAndroidTask(int taskId) {
        var activityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        return new ChromeAndroidTaskImpl(activityWindowAndroid);
    }

    @Before
    public void setUp() {
        AndroidBrowserWindowJni.setInstanceForTesting(mMockAndroidBrowserWindowNatives);
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
        var chromeAndroidTask = createChromeAndroidTask();

        // Act.
        long nativeBrowserWindowPtr = chromeAndroidTask.getOrCreateNativeBrowserWindowPtr();

        // Assert.
        assertEquals(FAKE_NATIVE_ANDROID_BROWSER_WINDOW_PTR, nativeBrowserWindowPtr);
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
        // Arrange: create a ChromeAndroidTask and a fake native AndroidBrowserWindow pointer value.
        var chromeAndroidTask = createChromeAndroidTask();
        long nativeAndroidBrowserWindowPtr = chromeAndroidTask.getOrCreateNativeBrowserWindowPtr();

        // Act.
        chromeAndroidTask.destroy();

        // Assert.
        verify(mMockAndroidBrowserWindowNatives, times(1)).destroy(nativeAndroidBrowserWindowPtr);
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
}
