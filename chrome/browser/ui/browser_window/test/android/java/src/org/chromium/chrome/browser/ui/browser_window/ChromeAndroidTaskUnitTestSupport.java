// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.lang.ref.WeakReference;

/** Supports Robolectric and native unit tests relevant to {@link ChromeAndroidTask}. */
@NullMarked
public final class ChromeAndroidTaskUnitTestSupport {
    private ChromeAndroidTaskUnitTestSupport() {}

    /**
     * Creates a real {@link ChromeAndroidTask} with mock dependencies.
     *
     * @param taskId ID for {@link ChromeAndroidTask#getId()}.
     * @return A new instance of {@link ChromeAndroidTask}.
     */
    public static ChromeAndroidTask createChromeAndroidTaskWithMockDeps(int taskId) {
        var mockActivityWindowAndroid = createMockActivityWindowAndroid(taskId);
        return new ChromeAndroidTaskImpl(mockActivityWindowAndroid);
    }

    /**
     * Creates a mock {@link ActivityWindowAndroid}, which has a mock {@link Activity} with the
     * given {@code taskId}.
     */
    static ActivityWindowAndroid createMockActivityWindowAndroid(int taskId) {
        var mockActivityWindowAndroid = mock(ActivityWindowAndroid.class);
        var mockActivity = mock(Activity.class);

        when(mockActivity.getTaskId()).thenReturn(taskId);
        when(mockActivityWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mockActivity));

        return mockActivityWindowAndroid;
    }

    /**
     * Creates a mock {@link AndroidBrowserWindow.Natives} that returns the given {@code
     * fakeNativePtr} when {@link AndroidBrowserWindow.Natives#create()} is called.
     */
    static AndroidBrowserWindow.Natives createMockAndroidBrowserWindowNatives(long fakeNativePtr) {
        var mockAndroidBrowserWindowNatives = mock(AndroidBrowserWindow.Natives.class);

        when(mockAndroidBrowserWindowNatives.create(any())).thenReturn(fakeNativePtr);

        return mockAndroidBrowserWindowNatives;
    }
}
