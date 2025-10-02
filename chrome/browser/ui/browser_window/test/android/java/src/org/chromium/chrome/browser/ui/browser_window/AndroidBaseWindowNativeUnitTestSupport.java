// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockingDetails;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;

/**
 * Supports {@code android_base_window_unittest.cc}.
 *
 * <p>The native unit test will use this class to:
 *
 * <ul>
 *   <li>Instantiate a Java {@code AndroidBaseWindow} and its native counterpart; and
 *   <li>Test the Java {@code AndroidBaseWindow} methods.
 * </ul>
 */
@NullMarked
final class AndroidBaseWindowNativeUnitTestSupport {
    private final AndroidBaseWindow mAndroidBaseWindow;
    private final ChromeAndroidTask mChromeAndroidTask;
    private final WindowAndroid mWindowAndroid;

    @CalledByNative
    private AndroidBaseWindowNativeUnitTestSupport(boolean useRealWindowAndroid) {
        if (useRealWindowAndroid) {
            // 1. Get a real context to provide a Theme and Resources for the activity.
            Context realContext = ContextUtils.getApplicationContext();

            // 2. Create a mock Activity and mock its functionality.
            Activity mockActivity =
                    mock(
                            Activity.class,
                            withSettings()
                                    .extraInterfaces(ActivityLifecycleDispatcherProvider.class));
            when(mockActivity.getResources()).thenReturn(realContext.getResources());
            when(mockActivity.getTheme()).thenReturn(realContext.getTheme());
            ActivityLifecycleDispatcher mockDispatcher = mock(ActivityLifecycleDispatcher.class);
            when(((ActivityLifecycleDispatcherProvider) mockActivity).getLifecycleDispatcher())
                    .thenReturn(mockDispatcher);
            when(mockActivity.getTaskId()).thenReturn(1);

            // 3. Create a mock TabModel and mock its functionality
            TabModel tabModel = mock(TabModel.class);
            when(tabModel.getProfile()).thenReturn(mock(Profile.class));

            // 4. Create a real tracker and WindowAndroid.
            IntentRequestTracker tracker = IntentRequestTracker.createFromActivity(mockActivity);
            mWindowAndroid =
                    new ActivityWindowAndroid(
                            mockActivity,
                            /* listenToActivityState= */ false,
                            tracker,
                            /* insetObserver= */ null,
                            /* trackOcclusion= */ false);

            mChromeAndroidTask =
                    new ChromeAndroidTaskImpl(
                            BrowserWindowType.NORMAL,
                            (ActivityWindowAndroid) mWindowAndroid,
                            tabModel);
        } else {
            mWindowAndroid = mock(WindowAndroid.class);
            mChromeAndroidTask = mock(ChromeAndroidTask.class);
        }
        mAndroidBaseWindow = new AndroidBaseWindow(mChromeAndroidTask);
    }

    @CalledByNative
    private WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    @CalledByNative
    private long invokeGetOrCreateNativePtr() {
        return mAndroidBaseWindow.getOrCreateNativePtr();
    }

    @CalledByNative
    private long invokeGetNativePtrForTesting() {
        return mAndroidBaseWindow.getNativePtrForTesting();
    }

    @CalledByNative
    private void invokeDestroy() {
        mAndroidBaseWindow.destroy();
    }

    @CalledByNative
    private void verifyBoundsToSet(int left, int top, int right, int bottom) {
        assert mockingDetails(mChromeAndroidTask).isMock();
        verify(mChromeAndroidTask).setBoundsInDp(new Rect(left, top, right, bottom));
    }

    @CalledByNative
    private void setFakeBounds(int left, int top, int right, int bottom) {
        assert mockingDetails(mChromeAndroidTask).isMock();
        when(mChromeAndroidTask.getBoundsInDp()).thenReturn(new Rect(left, top, right, bottom));
    }
}
