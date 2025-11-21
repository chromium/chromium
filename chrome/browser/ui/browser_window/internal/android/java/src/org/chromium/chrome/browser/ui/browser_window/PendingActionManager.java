// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.browser_window;

import android.annotation.SuppressLint;
import android.graphics.Rect;

import androidx.annotation.GuardedBy;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.PendingTaskInfo;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskImpl.State;
import org.chromium.ui.mojom.WindowShowState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class that holds business logic to track and manage actions requested on a {@code
 * State.PENDING_CREATE} or a {@code State.PENDING_UPDATE} {@link ChromeAndroidTask}.
 */
@NullMarked
final class PendingActionManager {
    /**
     * Enumerates actions that can be requested on a {@code State.PENDING_CREATE or a {@code
     * State.PENDING_UPDATE} browser window.
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

    private final Object mPendingActionsLock = new Object();

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
    @GuardedBy("mPendingActionsLock")
    private @PendingAction int[] mPendingActions = {PendingAction.NONE, PendingAction.NONE};

    /**
     * Tracks the size a window should have when it is fully initialized based on a SET_BOUNDS
     * request.
     */
    @GuardedBy("mPendingActionsLock")
    private @Nullable Rect mPendingBoundsInDp;

    /**
     * Tracks the size a window should have when it is fully initialized based on a RESTORE request.
     */
    @GuardedBy("mPendingActionsLock")
    private @Nullable Rect mPendingRestoredBoundsInDp;

    /**
     * Tracking the future active state of the window. Null if there is no in-progress action which
     * can affect the isActive value.
     */
    @GuardedBy("mPendingActionsLock")
    private @Nullable Boolean mIsActiveFuture;

    /**
     * Tracking the future visible state of the window. Null if there is no in-progress action which
     * can affect the isVisible value.
     */
    @GuardedBy("mPendingActionsLock")
    private @Nullable Boolean mIsVisibleFuture;

    /**
     * Tracking the future maximize state of the window. Null if there is no in-progress action
     * which can affect the isMaximized value.
     */
    @GuardedBy("mPendingActionsLock")
    private @Nullable Boolean mIsMaximizedFuture;

    /**
     * Tracks the size a window should have when SET_BOUNDS is done. Null if there is no in-progress
     * action which can affect the getBounds value.
     */
    @GuardedBy("mPendingActionsLock")
    private @Nullable Rect mFutureBoundsInDp;

    /**
     * Requests an action to be performed on the pending task. Use this for actions that do not
     * require an input.
     *
     * @param action The action to be performed.
     */
    void requestAction(@PendingAction int action) {
        assert action != PendingAction.SET_BOUNDS : "Use requestSetBounds() and provide a Rect.";
        assert action != PendingAction.MAXIMIZE : "Use requestMaximize() and provide a Rect.";
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

    void requestMaximize(Rect futureBounds) {
        synchronized (mPendingActionsLock) {
            mPendingActions[0] = PendingAction.MAXIMIZE;
            mPendingActions[1] = PendingAction.NONE;

            mPendingBoundsInDp = futureBounds;
            updateFutureStatesLocked();
        }
    }

    void requestRestore(Rect futureBounds) {
        synchronized (mPendingActionsLock) {
            mPendingActions[0] = PendingAction.RESTORE;
            mPendingActions[1] = PendingAction.NONE;

            mPendingBoundsInDp = futureBounds;
            updateFutureStatesLocked();
        }
    }

    /**
     * Requests a SET_BOUNDS action to be performed on the pending task.
     *
     * @param boundsInDp The requested bounds, in dp.
     */
    void requestSetBounds(Rect boundsInDp) {
        if (boundsInDp.isEmpty()) return;

        requestGlobalOverrideAction(PendingAction.SET_BOUNDS);
        synchronized (mPendingActionsLock) {
            mPendingBoundsInDp = boundsInDp;
            // Cache last requested bounds for potential subsequent restoration. Pending restored
            // bounds will be cleared after all pending actions are dispatched.
            mPendingRestoredBoundsInDp = mPendingBoundsInDp;
            mFutureBoundsInDp = boundsInDp;
        }
    }

    /**
     * Update future states, such as isVisible, isActive based on the current pending task info.
     *
     * @param pendingTaskInfo The pending task info when task is created.
     */
    void updateFutureStates(PendingTaskInfo pendingTaskInfo) {
        synchronized (mPendingActionsLock) {
            // Future states per Android default behavior
            mIsVisibleFuture = true;
            mIsActiveFuture = true;

            // Update states based on PendingTaskInfo
            @WindowShowState.EnumType
            int initialShowState = pendingTaskInfo.mCreateParams.getInitialShowState();
            switch (initialShowState) {
                case WindowShowState.MINIMIZED:
                    requestGlobalOverrideAction(PendingAction.MINIMIZE);
                    break;
                case WindowShowState.MAXIMIZED:
                    requestGlobalOverrideAction(PendingAction.MAXIMIZE);
                    break;
                case WindowShowState.DEFAULT:
                case WindowShowState.NORMAL:
                    // No pending action needed.
                    break;
                default:
                    throw new UnsupportedOperationException(
                            "Attempting to apply an unsupported initial show state.");
            }
        }
    }

    @Nullable Rect getFutureBoundsInDp() {
        synchronized (mPendingActionsLock) {
            return mFutureBoundsInDp;
        }
    }

    @Nullable Rect getPendingBoundsInDp() {
        synchronized (mPendingActionsLock) {
            return mPendingBoundsInDp;
        }
    }

    @Nullable Rect getPendingRestoredBoundsInDp() {
        synchronized (mPendingActionsLock) {
            return mPendingRestoredBoundsInDp;
        }
    }

    /**
     * Determines whether a pending request exists for the given action.
     *
     * @param action The {@link PendingAction} that will be checked.
     * @return {@code true} if a request for {@code action} is pending, {@code false} otherwise.
     */
    boolean isActionRequested(@PendingAction int action) {
        synchronized (mPendingActionsLock) {
            if (isPrimaryAction(action)) {
                return mPendingActions[0] == action;
            }
            return mPendingActions[1] == action;
        }
    }

    /**
     * Whether isActive will return true when the in-progress event is finished.
     *
     * @param state The current state of task.
     * @return Null if there is no on-going events affecting the result at the current state. True
     *     when an event will make isActive true when finished; otherwise false.
     */
    @Nullable Boolean isActiveFuture(@State int state) {
        synchronized (mPendingActionsLock) {
            if (state == State.PENDING_CREATE) {
                return Boolean.TRUE.equals(mIsActiveFuture);
            } else if (state == State.PENDING_UPDATE) {
                return mIsActiveFuture;
            }
            return null;
        }
    }

    /**
     * Whether isMaximized will return true when the in-progress event is finished.
     *
     * @param state The current state of task.
     * @return Null if there is no on-going events affecting the result at the current state. True
     *     when an event will make isMaximized true when finished; otherwise false.
     */
    @Nullable Boolean isMaximizedFuture(@State int state) {
        synchronized (mPendingActionsLock) {
            if (state == State.PENDING_CREATE) {
                return Boolean.TRUE.equals(mIsMaximizedFuture);
            } else if (state == State.PENDING_UPDATE) {
                return mIsMaximizedFuture;
            }
            return null;
        }
    }

    /**
     * Whether isVisible will return true when the in-progress event is finished.
     *
     * @param state The current state of task.
     * @return Null if there is no on-going events affecting the result at the current state. True
     *     when an event will make isVisible true when finished; otherwise false.
     */
    @Nullable Boolean isVisibleFuture(@State int state) {
        synchronized (mPendingActionsLock) {
            if (state == State.PENDING_CREATE) {
                return Boolean.TRUE.equals(mIsVisibleFuture);
            } else if (state == State.PENDING_UPDATE) {
                return mIsVisibleFuture;
            }
            return null;
        }
    }

    @SuppressLint("WrongConstant")
    @PendingAction
    int[] getAndClearPendingActions() {
        synchronized (mPendingActionsLock) {
            var actions = mPendingActions;
            mPendingActions = new int[] {PendingAction.NONE, PendingAction.NONE};
            mPendingBoundsInDp = null;
            mPendingRestoredBoundsInDp = null;
            mIsVisibleFuture = null;
            mIsActiveFuture = null;
            return actions;
        }
    }

    @SuppressLint("WrongConstant")
    @PendingAction
    int[] getAndClearTargetPendingActions(int... targets) {
        synchronized (mPendingActionsLock) {
            var actions = mPendingActions;
            for (int target : targets) {
                for (int j = 0; j < mPendingActions.length; j++) {
                    if (target == mPendingActions[j]) {
                        mPendingActions[j] = PendingAction.NONE;
                    }
                }
            }
            updateFutureStatesLocked();
            return actions;
        }
    }

    private void requestShow() {
        synchronized (mPendingActionsLock) {
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
            updateFutureStatesLocked();
        }
    }

    private void requestShowInactive() {
        synchronized (mPendingActionsLock) {
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
            updateFutureStatesLocked();
        }
    }

    private void requestActivate() {
        synchronized (mPendingActionsLock) {
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
            updateFutureStatesLocked();
        }
    }

    private void requestDeactivate() {
        synchronized (mPendingActionsLock) {
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
            updateFutureStatesLocked();
        }
    }

    /**
     * Adds {@code action} as a new primary action request, or replaces an older primary action
     * request with {@code action}, and clears any secondary action requests.
     *
     * @param action The {@link PendingAction} that will clear any existing action request.
     */
    private void requestGlobalOverrideAction(@PendingAction int action) {
        synchronized (mPendingActionsLock) {
            mPendingActions[0] = action;
            mPendingActions[1] = PendingAction.NONE;

            // Clear pending bounds.
            mPendingBoundsInDp = null;
            updateFutureStatesLocked();
        }
    }

    @GuardedBy("mPendingActionsLock")
    private void updateFutureStatesLocked() {
        mIsActiveFuture = null;
        mIsVisibleFuture = null;
        mIsMaximizedFuture = null;
        mFutureBoundsInDp = null;
        for (int action : mPendingActions) {
            switch (action) {
                case PendingAction.SHOW:
                case PendingAction.ACTIVATE:
                case PendingAction.MAXIMIZE:
                case PendingAction.RESTORE:
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

    @PendingAction
    int[] getPendingActionsForTesting() {
        synchronized (mPendingActionsLock) {
            return mPendingActions;
        }
    }

    void clearPendingActionsForTesting() {
        synchronized (mPendingActionsLock) {
            mPendingActions[0] = PendingAction.NONE;
            mPendingActions[1] = PendingAction.NONE;
        }
    }
}
