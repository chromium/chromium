// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;

import org.chromium.base.ApplicationStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Manages the state of the window for a {@link ChromeAndroidTaskImpl}. This class is responsible
 * for determining if the window is minimized, maximized, fullscreen, or in a normal state.
 */
@NullMarked
final class WindowStateManager {

    /** Enumerates the state of the current window. */
    @IntDef({
        WindowState.UNKNOWN,
        WindowState.NORMAL,
        WindowState.MAXIMIZED,
        WindowState.MINIMIZED,
        WindowState.FULLSCREEN
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface WindowState {
        /** The window state is unknown. */
        int UNKNOWN = 0;

        /** The window is in a normal state (not maximized, minimized, or fullscreen). */
        int NORMAL = 1;

        /** The window is maximized. */
        int MAXIMIZED = 2;

        /** The window is minimized. */
        int MINIMIZED = 3;

        /** The window is in fullscreen mode. */
        int FULLSCREEN = 4;
    }

    private @WindowState int mWindowState = WindowState.UNKNOWN;
    private @Nullable Rect mRestoredRect;

    /**
     * Updates the current window state and caches the restored bounds if the window is in a normal
     * state. This method should be called when the window state might have changed, for example,
     * after a configuration change or when the activity's layout changes.
     *
     * @param activity The current {@link Activity}.
     */
    public void update(Activity activity) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }
        @WindowState int newWindowState = getWindowState(activity);

        if (newWindowState == WindowState.NORMAL) {
            mRestoredRect = activity.getWindowManager().getCurrentWindowMetrics().getBounds();
        }
        mWindowState = newWindowState;
    }

    /**
     * Gets the current state of the window.
     *
     * @return The current {@link WindowState}.
     */
    public @WindowState int getWindowState() {
        assert mWindowState != WindowState.UNKNOWN;
        return mWindowState;
    }

    /**
     * Returns the bounds of the window when it was last in the {@link WindowState#NORMAL} state.
     * These bounds can be used to restore the window to its previous size and position.
     *
     * @return The restored bounds in pixels.
     */
    public @Nullable Rect getRestoredRectInPx() {
        return mRestoredRect;
    }

    @RequiresApi(api = VERSION_CODES.R)
    private @WindowState int getWindowState(Activity activity) {
        if (isMinimized(activity)) {
            return WindowState.MINIMIZED;
        }

        if (isFullscreen(activity)) {
            return WindowState.FULLSCREEN;
        }

        if (isMaximized(activity)) {
            return WindowState.MAXIMIZED;
        }
        return WindowState.NORMAL;
    }

    private static boolean isMinimized(Activity activity) {
        return !ApplicationStatus.isTaskVisible(activity.getTaskId());
    }

    @RequiresApi(api = Build.VERSION_CODES.R)
    private static boolean isFullscreen(Activity activity) {
        var window = activity.getWindow();
        var windowManager = activity.getWindowManager();

        // See CompositorViewHolder#isInFullscreenMode
        return !windowManager
                        .getMaximumWindowMetrics()
                        .getWindowInsets()
                        .isVisible(WindowInsets.Type.statusBars())
                || (window.getInsetsController() != null
                        && window.getInsetsController().getSystemBarsBehavior()
                                == WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    }

    @RequiresApi(api = Build.VERSION_CODES.R)
    private static boolean isMaximized(Activity activity) {
        if (activity.isInMultiWindowMode()) {
            // Desktop windowing mode is also a multi-window mode. This should return false
            // if the task is in split-screen mode.
            Rect maxBoundsInPx =
                    ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(
                            activity.getWindowManager());
            return getCurrentBoundsInPx(activity).equals(maxBoundsInPx);
        } else {
            // In non-multi-window mode, Chrome is maximized by default.
            return true;
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.R)
    private static Rect getCurrentBoundsInPx(Activity activity) {
        return activity.getWindowManager().getCurrentWindowMetrics().getBounds();
    }
}
