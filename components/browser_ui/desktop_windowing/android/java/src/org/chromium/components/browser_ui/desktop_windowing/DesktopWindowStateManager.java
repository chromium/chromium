// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.desktop_windowing;

import android.graphics.Rect;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/**
 * Interface to observe and retrieve desktop windowing mode state and app header customization
 * terminology.
 */
@NullMarked
public interface DesktopWindowStateManager extends AppHeaderStateProvider {

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

        /**
         * Called when the activity focus state changes. See {@link
         * Activity#onTopResumedActivityChanged(boolean)}.
         *
         * @param isFocused Whether the activity is in a focused state. {@code true} when the
         *     activity is focused, {@code false} when the activity is unfocused.
         */
        default void onActivityFocusStateChanged(boolean isFocused) {}

        /**
         * Notifies interested {@link AppHeaderObserver}s of an app-header provided system gesture
         * exclusion rects change.
         *
         * @param rects The list of Rects that should not respond to system gestures.
         */
        default void onSystemGestureExclusionRectsChanged(List<Rect> rects) {}
    }

    /**
     * @return The window's {@link AppHeaderState} information.
     */
    @Override
    @Nullable AppHeaderState getAppHeaderState();

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

    /**
     * Updates the system UI header foreground color when the app header background color changes.
     *
     * @param backgroundColor The app header background color.
     */
    void updateForegroundColor(@ColorInt int backgroundColor);

    /**
     * Called when app-header provided system gesture exclusion rects change.
     *
     * @param rects The list of Rects that should not respond to system gestures.
     */
    void updateSystemGestureExclusionRects(List<Rect> rects);

    /** Destroy the instances and remove all the dependencies. */
    void destroy();
}
