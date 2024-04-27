// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;

public class AppHeaderUtils {
    /**
     * Determine if the currently starting activity is focused, based on the {@link
     * ActivityLifecycleDispatcher} instance associated with it. Note that this method is intended
     * to be used during app startup flows and may not return the correct value at other times.
     *
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} instance associated with
     *     the current activity. When {@code null}, it will be assumed that the starting activity is
     *     focused.
     * @return {@code true} if the currently starting activity is focused, {@code false} otherwise.
     */
    public static boolean isActivityFocusedAtStartup(
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher) {
        return lifecycleDispatcher == null
                || lifecycleDispatcher.getCurrentActivityState()
                        <= ActivityState.RESUMED_WITH_NATIVE;
    }

    /**
     * @param desktopWindowStateProvider The {@link DesktopWindowStateProvider} instance.
     * @return {@code true} if the current activity is in a desktop window, {@code false} otherwise.
     */
    public static boolean isAppInDesktopWindow(
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider) {
        if (desktopWindowStateProvider == null) return false;
        var appHeaderState = desktopWindowStateProvider.getAppHeaderState();

        return appHeaderState != null && appHeaderState.isInDesktopWindow();
    }
}
