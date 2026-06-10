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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Manages the {@link WindowState} and window bounds for a {@link ChromeAndroidTaskImpl}. */
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

    private @Nullable Rect mCurrentBoundsInDp;
    private @Nullable Rect mCurrentBoundsInPx;
    private @Nullable Rect mPreviousBoundsInDp;
    private @Nullable Rect mRestoredBoundsInPx;

    /**
     * Updates the current window state, including bounds.
     *
     * <p>This method should be called when the window state may have changed, for example, after a
     * configuration change or when the {@link Activity}'s layout changes.
     *
     * @param activity The top {@link Activity} in the window.
     * @param display The {@link DisplayAndroid} the activity is on.
     */
    void update(Activity activity, DisplayAndroid display) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }

        // Update the current bounds and the previous bounds.
        Rect newBoundsInPx = activity.getWindowManager().getCurrentWindowMetrics().getBounds();
        Rect newBoundsInDp =
                DisplayUtil.scaleToEnclosingRect(newBoundsInPx, 1.0f / display.getDipScale());
        mPreviousBoundsInDp = mCurrentBoundsInDp;
        mCurrentBoundsInPx = newBoundsInPx;
        mCurrentBoundsInDp = newBoundsInDp;

        // Determine the window state using the current bounds.
        @WindowState int newWindowState = getWindowStateInternal(activity, newBoundsInPx);

        // Update "restored bounds" using the current window state.
        if (newWindowState == WindowState.NORMAL) {
            mRestoredBoundsInPx = newBoundsInPx;
        }
        mWindowState = newWindowState;
    }

    /** Returns the current {@link WindowState}. */
    @WindowState
    int getWindowState() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return WindowState.UNKNOWN;
        }

        assert mWindowState != WindowState.UNKNOWN
                : "update() must be called before getWindowState()";
        return mWindowState;
    }

    /** Returns the current window bounds (in DP). */
    Rect getCurrentBoundsInDp() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return new Rect();
        }

        assert mCurrentBoundsInDp != null : "update() must be called before getCurrentBoundsInDp()";
        return mCurrentBoundsInDp;
    }

    /** Returns the current window bounds (in pixels). */
    Rect getCurrentBoundsInPx() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return new Rect();
        }

        assert mCurrentBoundsInPx != null : "update() must be called before getCurrentBoundsInPx()";
        return mCurrentBoundsInPx;
    }

    /**
     * Returns the bounds of the window (in pixels) for its most recent {@link WindowState#NORMAL}
     * state.
     *
     * <p>These bounds will be used to restore the window from the maximized/minimized state to the
     * normal state.
     */
    @Nullable Rect getRestoredBoundsInPx() {
        return mRestoredBoundsInPx;
    }

    /**
     * Returns whether there is a change in valid window bounds (in DP) after the last call to
     * {@link #update}.
     *
     * <p>This method will only detect changes in valid window bounds, i.e., it will return false if
     * called immediately after a window is initialized, in which case there is only the "initial
     * bounds" and no "previous bounds" to compare the initial bounds with.
     */
    boolean boundsChangedInDp() {
        // Only detect changes in valid (non-null) bounds.
        if (mPreviousBoundsInDp == null || mCurrentBoundsInDp == null) {
            return false;
        }

        return !mCurrentBoundsInDp.equals(mPreviousBoundsInDp);
    }

    @RequiresApi(api = VERSION_CODES.R)
    private @WindowState int getWindowStateInternal(Activity activity, Rect currentBoundsInPx) {
        if (isMinimized(activity)) {
            return WindowState.MINIMIZED;
        }

        if (isFullscreen(activity)) {
            return WindowState.FULLSCREEN;
        }

        if (isMaximized(activity, currentBoundsInPx)) {
            return WindowState.MAXIMIZED;
        }
        return WindowState.NORMAL;
    }

    private static boolean isMinimized(Activity activity) {
        // TODO(https://crbug.com/518763461): remove flag once verified
        if (ChromeFeatureList.sTaskGetIdAnrFix.isEnabled()) {
            return !ApplicationStatus.isTaskVisible(ApplicationStatus.getTaskId(activity));
        } else {
            return !ApplicationStatus.isTaskVisible(activity.getTaskId());
        }
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
    private static boolean isMaximized(Activity activity, Rect currentBoundsInPx) {
        if (activity.isInMultiWindowMode()) {
            // Desktop windowing mode is also a multi-window mode. This should return false
            // if the task is in split-screen mode.
            Rect maxBoundsInPx =
                    ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(
                            activity.getWindowManager());
            return currentBoundsInPx.equals(maxBoundsInPx);
        } else {
            // In non-multi-window mode, Chrome is maximized by default.
            return true;
        }
    }
}
