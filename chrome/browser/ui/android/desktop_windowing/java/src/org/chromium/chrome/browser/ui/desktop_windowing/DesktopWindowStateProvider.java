// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

/** Interface to observe and retrieve desktop windowing mode state and updates. */
public interface DesktopWindowStateProvider {

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

    AppHeaderState getAppHeaderState();

    boolean isInDesktopWindow();

    boolean addObserver(AppHeaderObserver observer);

    boolean removeObserver(AppHeaderObserver observer);
}
