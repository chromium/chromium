// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;

/** Interface to observe and retrieve desktop windowing mode state and updates. */
public interface DesktopWindowStateProvider
        extends TopResumedActivityChangedObserver, SaveInstanceStateObserver {

    interface AppHeaderObserver {

        /**
         * Called when the app header state changes while the app is in desktop windowing mode.
         *
         * @param newState The new {@link AppHeaderState}.
         */
        default void onAppHeaderStateChanged(AppHeaderState newState) {}

        /**
         * Called when the app enters or exits desktop windowing mode.
         *
         * @param isInDesktopWindow Whether the app is in a desktop window. {@code true} when the
         *     app enters desktop windowing mode, {@code false} when the app exits desktop windowing
         *     mode.
         */
        default void onDesktopWindowingModeChanged(boolean isInDesktopWindow) {}
    }

    /**
     * @return The window's {@link AppHeaderState} information.
     */
    AppHeaderState getAppHeaderState();

    /**
     * @return {@code true} if the activity is in a desktop window irrespective of focus state,
     *     {@code false} otherwise.
     */
    boolean isInDesktopWindow();

    /**
     * @return {@code true} if the activity is in an unfocused desktop window, {@code false}
     *     otherwise.
     */
    boolean isInUnfocusedDesktopWindow();

    /**
     * Adds an observer to be notified of desktop windowing state changes.
     *
     * @param observer The {@link AppHeaderObserver} to be added.
     * @return {@code true} if the observer was successfully added, {@code false} otherwise.
     */
    boolean addObserver(AppHeaderObserver observer);

    /**
     * Removes an observer that is notified of desktop windowing state changes.
     *
     * @param observer The {@link AppHeaderObserver} to be removed.
     * @return {@code true} if the observer was successfully removed, {@code false} otherwise.
     */
    boolean removeObserver(AppHeaderObserver observer);
}
