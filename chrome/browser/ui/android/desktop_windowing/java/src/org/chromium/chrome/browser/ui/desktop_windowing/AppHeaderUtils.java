// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
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
        // The ActivityState.DESTROYED check here is for when the activity state is unknown, when
        // invoked during app startup.
        return lifecycleDispatcher == null
                || lifecycleDispatcher.getCurrentActivityState()
                        <= ActivityState.RESUMED_WITH_NATIVE
                || lifecycleDispatcher.getCurrentActivityState() == ActivityState.DESTROYED;
    }

    /**
     * @param desktopWindowModeSupplier Supplier to determine whether the current activity is in a
     *     desktop window.
     * @return {@code true} if the current activity is in a desktop window, {@code false} otherwise.
     */
    public static boolean isAppInDesktopWindow(
            @Nullable Supplier<Boolean> desktopWindowModeSupplier) {
        // TODO (crbug/332784708): Assert that the supplier is an instance of AppHeaderCoordinator.
        return desktopWindowModeSupplier != null
                && Boolean.TRUE.equals(desktopWindowModeSupplier.get());
    }
}
