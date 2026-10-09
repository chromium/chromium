// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.browser_window;

import android.annotation.SuppressLint;
import android.graphics.Rect;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class that holds business logic to track and manage actions requested on a {@code
 * State.PENDING_UPDATE} {@link ChromeAndroidTask}.
 *
 * <p>Actions are not queued for a {@code State.PENDING_CREATE} {@link ChromeAndroidTask}.
 */
@NullMarked
final class PendingActionManager {
    /**
     * Enumerates actions that can be requested on a {@code State.PENDING_UPDATE} browser window.
     */
    @IntDef({
        PendingAction.NONE,
        PendingAction.SET_BOUNDS,
        PendingAction.MAXIMIZE,
        PendingAction.RESTORE,
        PendingAction.SHOW,
        PendingAction.HIDE,
        PendingAction.SHOW_INACTIVE,
        PendingAction.CLOSE,
        PendingAction.ACTIVATE,
        PendingAction.DEACTIVATE,
        PendingAction.MINIMIZE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PendingAction {
        int NONE = 0; // Default value for a pending action.
        int SET_BOUNDS = 1;
        int MAXIMIZE = 2;
        int RESTORE = 3;
        int SHOW = 4;
        int HIDE = 5;
        int SHOW_INACTIVE = 6;
        int CLOSE = 7;
        int ACTIVATE = 8;
        int DEACTIVATE = 9;
        int MINIMIZE = 10;
    }

    /**
     * Tracks pending actions. A newer action request should typically replace and override any
     * existing pending action request, unless the existing request is for a higher precedence
     * action. If an existing action has higher precedence, it simply means that the action already
     * accounts for the outcome of the current action and therefore, the current action will be
     * ignored and not initiated. For example, if we get a SET_BOUNDS request followed by a SHOW
     * request, SHOW will be ignored since SET_BOUNDS will naturally show the window with the
     * requested bounds. As another example, if we get a SHOW request followed by a MAXIMIZE
     * request, MAXIMIZE will replace the lower precedence and older SHOW request.
     *
     * <p>Only a couple of actions, SHOW_INACTIVE and DEACTIVATE, can be initiated along with one of
     * MAXIMIZE, RESTORE, or SET_BOUNDS action requests, if it already exists. For example, if we
     * get a MAXIMIZE request followed by a DEACTIVATE request, we want to not only maximize the
     * browser window but also unfocus the window in maximized state.
     *
     * <p>Since we can only ever run up to two pending actions as described above, we will define a
     * concept of "primary" and "secondary" actions. SHOW_INACTIVE and DEACTIVATE are considered
     * secondary actions, since they can be initiated in conjunction with another action. All other
     * actions are considered primary actions.
     *
     * <p>mPendingActions[0] stores a primary action. mPendingActions[1] stores a secondary action.
     * By definition, a secondary action will be initiated independently (when no primary action is
     * requested) or after a primary action.
     */
    private final @PendingAction int[] mPendingActions = {PendingAction.NONE, PendingAction.NONE};

    /** Tracks the size a window should have when the pending SET_BOUNDS request is done. */
    private @Nullable Rect mPendingBoundsInDp;

    /** Tracks the bounds a window should be restored to based on a SET_BOUNDS request. */
    private @Nullable Rect mFutureRestoredBoundsInDp;

    /**
     * Tracking the future active state of the window. Null if there is no in-progress action which
     * can affect the isActive value.
     */
    private @Nullable Boolean mIsActiveFuture;

    /**
     * Tracking the future visible state of the window. Null if there is no in-progress action which
     * can affect the isVisible value.
     */
    private @Nullable Boolean mIsVisibleFuture;

    /**
     * Tracking the future maximize state of the window. Null if there is no in-progress action
     * which can affect the isMaximized value.
     */
    private @Nullable Boolean mIsMaximizedFuture;

    /**
     * Tracks the size a window should have when SET_BOUNDS is done. Null if there is no in-progress
     * action which can affect the getBounds value.
     */
    private @Nullable Rect mFutureBoundsInDp;

    /**
     * Requests an action to be performed on the task. Use this for actions that do not require an
     * input.
     *
     * @param action The action to be performed.
     */
    void requestAction(@PendingAction int action) {
        ThreadUtils.assertOnUiThread();
        assert action != PendingAction.SET_BOUNDS && action != PendingAction.MAXIMIZE
                : "Use requestSetBounds() and provide a Rect.";
        assert action != PendingAction.RESTORE : "Use requestRestore() and provide a Rect.";
        switch (action) {
            case PendingAction.SHOW:
                requestShow();
                break;
            case PendingAction.SHOW_INACTIVE:
                requestShowInactive();
                break;
            case PendingAction.ACTIVATE:
                requestActivate();
                break;
            case PendingAction.DEACTIVATE:
                requestDeactivate();
                break;
            case PendingAction.HIDE:
            case PendingAction.CLOSE:
            case PendingAction.MINIMIZE:
                requestGlobalOverrideAction(action);
                break;
            default:
                assert false : "Unsupported pending action.";
        }
    }

    void requestRestore(Rect futureBoundsInDp) {
        ThreadUtils.assertOnUiThread();
        mPendingActions[0] = PendingAction.RESTORE;
        mPendingActions[1] = PendingAction.NONE;

        mPendingBoundsInDp = futureBoundsInDp;
        updateFutureStatesInternal();
    }

    /**
     * Requests the task's bounds to be changed.
     *
     * @param boundsInDp The requested bounds, in dp.
     * @param isMaximizedBounds Whether {@code boundsInDp} are the maximum window bounds.
     */
    void requestSetBounds(Rect boundsInDp, boolean isMaximizedBounds) {
        ThreadUtils.assertOnUiThread();
        if (boundsInDp.isEmpty()) return;

        mPendingActions[0] = isMaximizedBounds ? PendingAction.MAXIMIZE : PendingAction.SET_BOUNDS;
        mPendingActions[1] = PendingAction.NONE;
        mPendingBoundsInDp = boundsInDp;
        if (!isMaximizedBounds) {
            // Cache last requested bounds for potential subsequent restoration.
            mFutureRestoredBoundsInDp = mPendingBoundsInDp;
        }
        updateFutureStatesInternal();
    }

    @Nullable Rect getFutureBoundsInDp() {
        ThreadUtils.assertOnUiThread();
        return mFutureBoundsInDp;
    }

    @Nullable Rect getFutureRestoredBoundsInDp() {
        ThreadUtils.assertOnUiThread();
        return mFutureRestoredBoundsInDp;
    }

    /**
     * Whether isActive will return true when the in-progress event is finished.
     *
     * @return Null if there is no on-going events affecting the result. True when an event will
     *     make isActive true when finished; otherwise false.
     */
    @Nullable Boolean isActiveFuture() {
        ThreadUtils.assertOnUiThread();
        return mIsActiveFuture;
    }

    /**
     * Whether isMaximized will return true when the in-progress event is finished.
     *
     * @return Null if there is no on-going events affecting the result. True when an event will
     *     make isMaximized true when finished; otherwise false.
     */
    @Nullable Boolean isMaximizedFuture() {
        ThreadUtils.assertOnUiThread();
        return mIsMaximizedFuture;
    }

    /**
     * Whether isVisible will return true when the in-progress event is finished.
     *
     * @return Null if there is no on-going events affecting the result. True when an event will
     *     make isVisible true when finished; otherwise false.
     */
    @Nullable Boolean isVisibleFuture() {
        ThreadUtils.assertOnUiThread();
        return mIsVisibleFuture;
    }

    @SuppressLint("WrongConstant")
    @PendingAction
    int[] getAndClearTargetPendingActions(int... targets) {
        ThreadUtils.assertOnUiThread();
        var actions = mPendingActions;
        for (int target : targets) {
            for (int j = 0; j < mPendingActions.length; j++) {
                if (target == mPendingActions[j]) {
                    mPendingActions[j] = PendingAction.NONE;
                }
            }
        }
        updateFutureStatesInternal();
        return actions;
    }

    private void requestShow() {
        // Clear lower precedence secondary action.
        mPendingActions[1] = PendingAction.NONE;

        // Retain higher precedence primary action and ignore SHOW.
        if (mPendingActions[0] == PendingAction.CLOSE
                || mPendingActions[0] == PendingAction.MAXIMIZE
                || mPendingActions[0] == PendingAction.RESTORE
                || mPendingActions[0] == PendingAction.SET_BOUNDS) {
            return;
        }

        // Override lower precedence primary action.
        mPendingActions[0] = PendingAction.SHOW;
        updateFutureStatesInternal();
    }

    private void requestShowInactive() {
        // Clear lower precedence primary action.
        if (mPendingActions[0] == PendingAction.SHOW
                || mPendingActions[0] == PendingAction.HIDE
                || mPendingActions[0] == PendingAction.ACTIVATE
                || mPendingActions[0] == PendingAction.DEACTIVATE
                || mPendingActions[0] == PendingAction.MINIMIZE) {
            mPendingActions[0] = PendingAction.NONE;
        }

        // Retain higher precedence action and ignore SHOW_INACTIVE.
        if (mPendingActions[0] == PendingAction.CLOSE) {
            return;
        }

        // Run SHOW_INACTIVE along with one of the other higher precedence primary actions.
        mPendingActions[1] = PendingAction.SHOW_INACTIVE;
        updateFutureStatesInternal();
    }

    private void requestActivate() {
        // Clear lower precedence secondary action.
        mPendingActions[1] = PendingAction.NONE;

        // Retain higher precedence primary action and ignore ACTIVATE.
        if (mPendingActions[0] == PendingAction.SHOW
                || mPendingActions[0] == PendingAction.CLOSE
                || mPendingActions[0] == PendingAction.MAXIMIZE
                || mPendingActions[0] == PendingAction.RESTORE
                || mPendingActions[0] == PendingAction.SET_BOUNDS) {
            return;
        }

        // Override lower precedence primary action.
        mPendingActions[0] = PendingAction.ACTIVATE;
        updateFutureStatesInternal();
    }

    private void requestDeactivate() {
        // Clear lower precedence primary action.
        if (mPendingActions[0] == PendingAction.SHOW
                || mPendingActions[0] == PendingAction.ACTIVATE) {
            mPendingActions[0] = PendingAction.NONE;
        }

        // Retain higher precedence action and ignore DEACTIVATE.
        if (mPendingActions[0] == PendingAction.HIDE
                || mPendingActions[0] == PendingAction.CLOSE
                || mPendingActions[0] == PendingAction.MINIMIZE
                || mPendingActions[1] == PendingAction.SHOW_INACTIVE) {
            return;
        }

        // Run DEACTIVATE along with one of the other higher precedence primary actions.
        mPendingActions[1] = PendingAction.DEACTIVATE;
        updateFutureStatesInternal();
    }

    /**
     * Adds {@code action} as a new primary action request, or replaces an older primary action
     * request with {@code action}, and clears any secondary action requests.
     *
     * @param action The {@link PendingAction} that will clear any existing action request.
     */
    private void requestGlobalOverrideAction(@PendingAction int action) {
        mPendingActions[0] = action;
        mPendingActions[1] = PendingAction.NONE;

        // Clear pending bounds.
        mPendingBoundsInDp = null;
        updateFutureStatesInternal();
    }

    private void updateFutureStatesInternal() {
        mIsActiveFuture = null;
        mIsVisibleFuture = null;
        mIsMaximizedFuture = null;
        mFutureBoundsInDp = null;
        for (int action : mPendingActions) {
            switch (action) {
                case PendingAction.SHOW:
                case PendingAction.ACTIVATE:
                    mIsActiveFuture = true;
                    break;
                case PendingAction.SHOW_INACTIVE:
                case PendingAction.MINIMIZE:
                case PendingAction.DEACTIVATE:
                case PendingAction.CLOSE:
                    mIsActiveFuture = false;
                    break;
                default:
                    break;
            }

            switch (action) {
                case PendingAction.SHOW:
                case PendingAction.ACTIVATE:
                case PendingAction.MAXIMIZE:
                case PendingAction.SHOW_INACTIVE:
                case PendingAction.RESTORE:
                    mIsVisibleFuture = true;
                    break;
                case PendingAction.MINIMIZE:
                case PendingAction.CLOSE:
                    mIsVisibleFuture = false;
                    break;
                default:
                    break;
            }

            switch (action) {
                case PendingAction.MAXIMIZE:
                    mIsMaximizedFuture = true;
                    break;
                case PendingAction.MINIMIZE:
                case PendingAction.CLOSE:
                case PendingAction.HIDE:
                case PendingAction.RESTORE:
                case PendingAction.SET_BOUNDS:
                    mIsMaximizedFuture = false;
                    break;
                default:
                    break;
            }

            if (action == PendingAction.SET_BOUNDS
                    || action == PendingAction.MAXIMIZE
                    || action == PendingAction.RESTORE) {
                mFutureBoundsInDp = mPendingBoundsInDp;
            }
        }
    }

    @VisibleForTesting
    static boolean isPrimaryAction(@PendingAction int action) {
        return action != PendingAction.SHOW_INACTIVE && action != PendingAction.DEACTIVATE;
    }

    @Nullable Rect getPendingBoundsInDpForTesting() {
        ThreadUtils.assertOnUiThread();
        return mPendingBoundsInDp;
    }

    @PendingAction
    int[] getPendingActionsForTesting() {
        ThreadUtils.assertOnUiThread();
        return mPendingActions;
    }

    void clearPendingActionsForTesting() {
        ThreadUtils.assertOnUiThread();
        mPendingActions[0] = PendingAction.NONE;
        mPendingActions[1] = PendingAction.NONE;
    }
}
