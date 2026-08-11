// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowMetrics;

import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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

    private @Nullable Float mCurrentDipScale;
    private @Nullable Rect mCurrentDecorViewBoundsInPx;
    private @Nullable Rect mCurrentBoundsInDp;
    private @Nullable Rect mCurrentBoundsInPx;
    private @Nullable Rect mPreviousBoundsInDp;
    private @Nullable Rect mRestoredBoundsInPx;

    /**
     * To be called only by {@code ChromeAndroidTaskImpl#mDecorViewLayoutChangeListener}. Do
     * <i>not</i> call this method for other purposes.
     *
     * <p>{@code ChromeAndroidTaskImpl#mDecorViewLayoutChangeListener} is triggered frequently, and
     * calling {@link WindowStateManager#update} too often can cause ANRs. This method is
     * specifically created to call {@link WindowStateManager#update} when necessary.
     *
     * @param activity The top {@link Activity} in the window.
     * @param display The {@link DisplayAndroid} the activity is on.
     * @return Whether there is a change in window bounds (in DP) after the decor View's layout
     *     change.
     */
    boolean updateForDecorViewLayoutChange(Activity activity, DisplayAndroid display) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return false;
        }

        // Calling WindowStateManager#updateInternal too frequently can cause ANRs. If there is no
        // change in the decor View's bounds or the scaling factor, the window state won't change,
        // and we can skip WindowStateManager#updateInternal.
        Rect newDecorViewBoundsInPx = getDecorViewBoundsInPx(activity);
        float newDipScale = display.getDipScale();
        boolean decorViewBoundsChanged =
                newDecorViewBoundsInPx != null
                        && !newDecorViewBoundsInPx.equals(mCurrentDecorViewBoundsInPx);
        boolean dipScaleChanged =
                mCurrentDipScale == null
                        || !MathUtils.areFloatsEqual(newDipScale, mCurrentDipScale);

        if (!decorViewBoundsChanged && !dipScaleChanged) {
            return false;
        }

        updateInternal(activity, display);

        // Only detect changes in valid (non-null) window bounds in DP.
        return mPreviousBoundsInDp != null
                && mCurrentBoundsInDp != null
                && !mPreviousBoundsInDp.equals(mCurrentBoundsInDp);
    }

    /**
     * Updates the current window state, including bounds.
     *
     * <p>This method should be called when the window state may have changed, for example, when the
     * {@link Activity}'s layout changes.
     *
     * <p>Calling this method too frequently may cause an ANR due to synchronous IPC, such as when
     * obtaining {@link WindowMetrics}.
     *
     * @param activity The top {@link Activity} in the window.
     * @param display The {@link DisplayAndroid} the activity is on.
     */
    void update(Activity activity, DisplayAndroid display) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }

        updateInternal(activity, display);
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
    Rect getWindowBoundsInDp() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return new Rect();
        }

        assert mCurrentBoundsInDp != null : "update() must be called before getWindowBoundsInDp()";
        return mCurrentBoundsInDp;
    }

    /** Returns the current window bounds (in pixels). */
    Rect getWindowBoundsInPx() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return new Rect();
        }

        assert mCurrentBoundsInPx != null : "update() must be called before getWindowBoundsInPx()";
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

    @RequiresApi(api = VERSION_CODES.R)
    private void updateInternal(Activity activity, DisplayAndroid display) {
        float dipScale = display.getDipScale();
        Rect newBoundsInPx = activity.getWindowManager().getCurrentWindowMetrics().getBounds();
        Rect newBoundsInDp = DisplayUtil.scaleToEnclosingRect(newBoundsInPx, 1.0f / dipScale);
        mCurrentDipScale = dipScale;
        mCurrentDecorViewBoundsInPx = getDecorViewBoundsInPx(activity);
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

    private static @Nullable Rect getDecorViewBoundsInPx(Activity activity) {
        var window = activity.getWindow();
        if (window == null) {
            return null;
        }

        // Note that a View's left/top/right/bottom properties are relative to its parent, not the
        // screen. We should map a View's bounds to the screen's coordinate space when working with
        // both View bounds and window bounds.
        View decorView = window.getDecorView();
        int[] locationOnScreen = new int[2];
        decorView.getLocationOnScreen(locationOnScreen);
        int x = locationOnScreen[0];
        int y = locationOnScreen[1];
        return new Rect(
                /* left= */ x,
                /* top= */ y,
                /* right= */ x + decorView.getWidth(),
                /* bottom= */ y + decorView.getHeight());
    }

    @RequiresApi(api = VERSION_CODES.R)
    private static @WindowState int getWindowStateInternal(
            Activity activity, Rect currentBoundsInPx) {
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
        return !ApplicationStatus.isTaskVisible(ApplicationStatus.getTaskId(activity));
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
