// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import androidx.annotation.GuardedBy;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.ui.browser_window.PendingActionManager.PendingAction;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.insets.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.ui.mojom.WindowShowState;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

/** Implements {@link ChromeAndroidTask}. */
@NullMarked
final class ChromeAndroidTaskImpl
        implements ChromeAndroidTask,
                ConfigurationChangedObserver,
                TopResumedActivityChangedWithNativeObserver,
                TabModelObserver {

    private static final String TAG = "ChromeAndroidTask";

    /** States of this {@link ChromeAndroidTask}. */
    @VisibleForTesting
    enum State {
        /** The Task is not yet initialized. */
        UNKNOWN,

        /** The Task is pending and not yet associated with an Activity. */
        PENDING_CREATE,

        /** The Task has a state being updated but not finished yet. */
        PENDING_UPDATE,

        /* The Task is alive without any pending state change. */
        IDLE,

        /** The Task is being destroyed, but the destruction hasn't been completed. */
        DESTROYING,

        /** The Task has been destroyed. */
        DESTROYED,
    }

    private final AtomicReference<State> mState = new AtomicReference<>(State.UNKNOWN);

    private final PendingActionManager mPendingActionManager = new PendingActionManager();

    private final @BrowserWindowType int mBrowserWindowType;

    private @Nullable Integer mId;

    private final AndroidBrowserWindow mAndroidBrowserWindow;
    private final Profile mInitialProfile;

    /**
     * Contains all {@link ChromeAndroidTaskFeature}s associated with this {@link
     * ChromeAndroidTask}.
     */
    @GuardedBy("mFeaturesLock")
    private final List<ChromeAndroidTaskFeature> mFeatures = new ArrayList<>();

    private final AtomicLong mLastActivatedTimeMillis = new AtomicLong(-1);

    private final Object mActivityScopedObjectsLock = new Object();
    private final Object mFeaturesLock = new Object();

    private @Nullable PendingTaskInfo mPendingTaskInfo;

    /**
     * The {@link ActivityScopedObjects} in this Task.
     *
     * <p>As a {@link ChromeAndroidTask} is meant to track an Android Task, but {@link
     * ActivityScopedObjects} is associated with a {@code ChromeActivity}, {@link
     * ActivityScopedObjects} should be nullable and set/cleared per the {@code ChromeActivity}
     * lifecycle.
     *
     * @see #setActivityScopedObjects
     * @see #clearActivityScopedObjects
     */
    @GuardedBy("mActivityScopedObjectsLock")
    private @Nullable ActivityScopedObjects mActivityScopedObjects;

    /** Last Task (window) bounds updated by {@link #onConfigurationChanged(Configuration)}. */
    private @Nullable Rect mLastBoundsInDpOnConfigChanged;

    /** Non-maximized bounds of the task even when currently maximized or minimized. */
    @GuardedBy("mActivityScopedObjectsLock")
    private @Nullable Rect mRestoredBoundsInPx;

    private boolean mShouldDispatchPendingDeactivate;

    /**
     * Listener for window insets animation.
     *
     * <p>This listener is used to detect when the window insets animation ends and the window
     * bounds change.
     */
    @RequiresApi(VERSION_CODES.R)
    private final WindowInsetsAnimationListener mWindowInsetsAnimationListener =
            new WindowInsetsAnimationListener() {
                private boolean mIsRestored;
                private Rect mBoundsBeforeAnimation = new Rect(0, 0, 0, 0);

                @Override
                public void onPrepare(WindowInsetsAnimationCompat animation) {
                    synchronized (mActivityScopedObjectsLock) {
                        var activityWindowAndroid =
                                getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
                        if (activityWindowAndroid == null) return;
                        mIsRestored = isRestoredInternalLocked(activityWindowAndroid);
                        mBoundsBeforeAnimation = getCurrentBoundsInPxLocked(activityWindowAndroid);
                    }
                }

                @Override
                public void onStart(
                        WindowInsetsAnimationCompat animation,
                        WindowInsetsAnimationCompat.BoundsCompat bounds) {}

                @Override
                public void onProgress(
                        WindowInsetsCompat windowInsetsCompat,
                        List<WindowInsetsAnimationCompat> list) {}

                @Override
                public void onEnd(WindowInsetsAnimationCompat animation) {
                    synchronized (mActivityScopedObjectsLock) {
                        var activityWindowAndroid =
                                getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);

                        boolean isFullscreen = isFullscreenInternalLocked(activityWindowAndroid);
                        if (mIsRestored && isFullscreen) {
                            mRestoredBoundsInPx = mBoundsBeforeAnimation;
                        }
                    }
                }
            };

    private static Activity getActivity(ActivityWindowAndroid activityWindowAndroid) {
        Activity activity = activityWindowAndroid.getActivity().get();
        assert activity != null : "ActivityWindowAndroid should have an Activity.";

        return activity;
    }

    private static ActivityLifecycleDispatcher getActivityLifecycleDispatcher(
            ActivityWindowAndroid activityWindowAndroid) {
        var activity = getActivity(activityWindowAndroid);
        assert activity instanceof ActivityLifecycleDispatcherProvider
                : "Unsupported Activity: the Activity isn't an"
                        + " ActivityLifecycleDispatcherProvider.";

        return ((ActivityLifecycleDispatcherProvider) activity).getLifecycleDispatcher();
    }

    ChromeAndroidTaskImpl(
            @BrowserWindowType int browserWindowType, ActivityScopedObjects activityScopedObjects) {
        mBrowserWindowType = browserWindowType;
        mId = getActivity(activityScopedObjects.mActivityWindowAndroid).getTaskId();
        mAndroidBrowserWindow = new AndroidBrowserWindow(/* chromeAndroidTask= */ this);

        Profile initialProfile = activityScopedObjects.mTabModel.getProfile();
        assert initialProfile != null
                : "ChromeAndroidTask must be initialized with a non-null profile";
        mInitialProfile = initialProfile;

        mState.set(State.IDLE);
        setActivityScopedObjectsInternal(activityScopedObjects);
    }

    ChromeAndroidTaskImpl(PendingTaskInfo pendingTaskInfo) {
        mPendingTaskInfo = pendingTaskInfo;

        mBrowserWindowType = pendingTaskInfo.mCreateParams.getWindowType();
        mAndroidBrowserWindow = new AndroidBrowserWindow(/* chromeAndroidTask= */ this);
        mInitialProfile = pendingTaskInfo.mCreateParams.getProfile();
        mState.set(State.PENDING_CREATE);
    }

    @Override
    public @Nullable Integer getId() {
        return mId;
    }

    @Override
    public @Nullable PendingTaskInfo getPendingTaskInfo() {
        return mPendingTaskInfo;
    }

    @Override
    public @BrowserWindowType int getBrowserWindowType() {
        return mBrowserWindowType;
    }

    @Override
    public void setActivityScopedObjects(ActivityScopedObjects activityScopedObjects) {
        setActivityScopedObjectsInternal(activityScopedObjects);
    }

    @Override
    public @Nullable ActivityWindowAndroid getActivityWindowAndroid() {
        synchronized (mActivityScopedObjectsLock) {
            return getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        }
    }

    @Override
    public void clearActivityScopedObjects() {
        clearActivityScopedObjectsInternal();
    }

    @Override
    public void addFeature(ChromeAndroidTaskFeature feature) {
        synchronized (mFeaturesLock) {
            assertAlive();
            mFeatures.add(feature);
            feature.onAddedToTask();
        }
    }

    @Override
    public long getOrCreateNativeBrowserWindowPtr() {
        assert getState() == State.PENDING_CREATE || getState() == State.IDLE
                : "This Task is not pending or alive.";
        return mAndroidBrowserWindow.getOrCreateNativePtr();
    }

    @Override
    public void destroy() {
        // Immediately change the state to "DESTROYING" to block access to public methods that
        // should only be called when the state is "ALIVE".
        //
        // One case where the "DESTROYING" state is crucial:
        //
        // If a ChromeAndroidTaskFeature ("Feature") holds a ChromeAndroidTask ("Task") reference,
        // the Feature could call the Task's APIs during Feature#onTaskRemoved(). Since mState won't
        // become "DESTROYED" until after Feature#onTaskRemoved(), we need the "DESTROYING" state to
        // prevent the Feature from accessing the Task's APIs that should only be called when mState
        // is "ALIVE".
        if (!mState.compareAndSet(State.IDLE, State.DESTROYING)) {
            return;
        }

        if (mPendingTaskInfo != null) {
            mPendingTaskInfo.destroy();
            mPendingTaskInfo = null;
        }

        clearActivityScopedObjectsInternal();
        destroyFeatures();

        mAndroidBrowserWindow.destroy();
        mState.set(State.DESTROYED);
    }

    @Override
    public boolean isDestroyed() {
        return mState.get() == State.DESTROYED;
    }

    @Override
    public boolean isActive() {
        @Nullable Boolean isActiveFuture = mPendingActionManager.isActiveFuture(mState.get());
        if (isActiveFuture != null) {
            return isActiveFuture;
        }

        synchronized (mActivityScopedObjectsLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null) return false;
            return isActiveInternalLocked(activityWindowAndroid);
        }
    }

    @Override
    public boolean isMaximized() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "isMaximized() requires Android R+; returning a false");
            return false;
        }

        if (mState.get() == State.PENDING_CREATE) {
            return mPendingActionManager.isActionRequested(PendingAction.MAXIMIZE)
                    || assumeNonNull(mPendingTaskInfo).mCreateParams.getInitialShowState()
                            == WindowShowState.MAXIMIZED;
        } else if (mState.get() == State.PENDING_UPDATE) {
            Boolean isMaximized = mPendingActionManager.isMaximizedFuture();
            if (isMaximized != null) return isMaximized;
        }

        synchronized (mActivityScopedObjectsLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            return isMaximizedInternalLocked(activityWindowAndroid);
        }
    }

    @Override
    public boolean isMinimized() {
        if (mState.get() == State.PENDING_CREATE) {
            return mPendingActionManager.isActionRequested(PendingAction.MINIMIZE)
                    || assumeNonNull(mPendingTaskInfo).mCreateParams.getInitialShowState()
                            == WindowShowState.MINIMIZED;
        }

        synchronized (mActivityScopedObjectsLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            return isMinimizedInternalLocked(activityWindowAndroid);
        }
    }

    @Override
    public boolean isFullscreen() {
        if (mState.get() == State.PENDING_CREATE) {
            return false;
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "isFullscreen() requires Android R+; returning false");
            return false;
        }

        synchronized (mActivityScopedObjectsLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            return isFullscreenInternalLocked(activityWindowAndroid);
        }
    }

    @Override
    public Rect getRestoredBoundsInDp() {
        if (mState.get() == State.PENDING_CREATE) {
            var initialBounds = assumeNonNull(mPendingTaskInfo).mCreateParams.getInitialBounds();
            if (mPendingActionManager.isActionRequested(PendingAction.SET_BOUNDS)) {
                return assertNonNull(mPendingActionManager.getPendingBoundsInDp());
            } else if (mPendingActionManager.isActionRequested(PendingAction.RESTORE)) {
                var pendingRestoredBounds = mPendingActionManager.getPendingRestoredBoundsInDp();
                return pendingRestoredBounds == null ? initialBounds : pendingRestoredBounds;
            }
            return initialBounds;
        }

        synchronized (mActivityScopedObjectsLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null) {
                return new Rect();
            }

            Rect restoredBoundsInPx =
                    mRestoredBoundsInPx == null
                            ? getCurrentBoundsInPxLocked(activityWindowAndroid)
                            : mRestoredBoundsInPx;
            return DisplayUtil.scaleToEnclosingRect(
                    restoredBoundsInPx, 1.0f / activityWindowAndroid.getDisplay().getDipScale());
        }
    }

    @Override
    public long getLastActivatedTimeMillis() {
        long lastActivatedTimeMillis = mLastActivatedTimeMillis.get();
        assert lastActivatedTimeMillis > 0;
        return lastActivatedTimeMillis;
    }

    @Override
    public Profile getProfile() {
        return mInitialProfile;
    }

    @Override
    public Rect getBoundsInDp() {
        if (mState.get() == State.PENDING_CREATE) {
            if (mPendingActionManager.isActionRequested(PendingAction.SET_BOUNDS)) {
                return assertNonNull(mPendingActionManager.getPendingBoundsInDp());
            }
            return assumeNonNull(mPendingTaskInfo).mCreateParams.getInitialBounds();

        } else if (mState.get() == State.PENDING_UPDATE) {
            var bounds = mPendingActionManager.getFutureBoundsInDp();
            if (bounds != null) return bounds;
        }

        synchronized (mActivityScopedObjectsLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null) {
                return new Rect();
            }

            return getCurrentBoundsInDpLocked(activityWindowAndroid);
        }
    }

    @Override
    public void show() {
        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.SHOW);
            return;
        }

        synchronized (mActivityScopedObjectsLock) {
            showInternalLocked();
        }
    }

    @Override
    public boolean isVisible() {
        if (mState.get() == State.PENDING_CREATE) {
            return assumeNonNull(mPendingTaskInfo).mCreateParams.getInitialShowState()
                    != WindowShowState.MINIMIZED;
        } else if (mState.get() == State.PENDING_UPDATE) {
            Boolean isVisible = mPendingActionManager.isVisibleFuture();
            if (isVisible != null) return isVisible;
        }

        synchronized (mActivityScopedObjectsLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null) return false;
            return isVisibleInternalLocked(activityWindowAndroid);
        }
    }

    @Override
    public void showInactive() {
        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.SHOW_INACTIVE);
            return;
        }

        ChromeAndroidTaskTrackerImpl.getInstance().activatePenultimatelyActivatedTask();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        // Note:
        //
        // (1) Not all Configuration changes include a window bounds change, so we need to check
        // whether the bounds have changed.
        //
        // (2) As of Aug 12, 2025, Configuration doesn't provide a public API to get the window
        // bounds. Its "windowConfiguration" field is marked as @TestApi:
        // https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/content/res/Configuration.java;l=417-418;drc=64130047e019cee612a85dde07755efd8f356f12
        // Therefore, we obtain the new bounds using an Activity API (see
        // getBoundsInternalLocked()).
        synchronized (mFeaturesLock) {
            synchronized (mActivityScopedObjectsLock) {
                var activityWindowAndroid =
                        getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
                if (activityWindowAndroid == null) {
                    return;
                }

                var newBoundsInDp = getCurrentBoundsInDpLocked(activityWindowAndroid);
                if (newBoundsInDp.equals(mLastBoundsInDpOnConfigChanged)) {
                    return;
                }

                mLastBoundsInDpOnConfigChanged = newBoundsInDp;
                for (var feature : mFeatures) {
                    feature.onTaskBoundsChanged(newBoundsInDp);
                }
            }
        }
    }

    @Override
    public void close() {
        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.CLOSE);
            return;
        }

        synchronized (mActivityScopedObjectsLock) {
            closeInternalLocked();
        }
    }

    @Override
    public void activate() {
        if (Boolean.TRUE.equals(mPendingActionManager.isActiveFuture(mState.get()))) return;

        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.ACTIVATE);
            return;
        }

        synchronized (mActivityScopedObjectsLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null || isActiveInternalLocked(activityWindowAndroid)) {
                return;
            }
            activateInternalLocked();
        }
    }

    @Override
    public void deactivate() {
        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.DEACTIVATE);
            return;
        }

        if (!isActive()) return;
        ChromeAndroidTaskTrackerImpl.getInstance().activatePenultimatelyActivatedTask();
    }

    @Override
    public void maximize() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "maximize() requires Android R+; does nothing");
            return;
        }

        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.MAXIMIZE);
            return;
        }

        synchronized (mActivityScopedObjectsLock) {
            maximizeInternalLocked();
        }
    }

    @Override
    public void minimize() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "minimize() requires Android R+; does nothing");
            return;
        }

        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.MINIMIZE);
            return;
        }

        synchronized (mActivityScopedObjectsLock) {
            minimizeInternalLocked();
        }
    }

    @Override
    public void restore() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "restore() requires Android R+; does nothing");
            return;
        }
        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.RESTORE);
            return;
        }

        synchronized (mActivityScopedObjectsLock) {
            restoreInternalLocked();
        }
    }

    @Override
    public void setBoundsInDp(Rect boundsInDp) {
        if (mState.get() == State.PENDING_CREATE) {
            if (!boundsInDp.isEmpty()) {
                mPendingActionManager.requestSetBounds(boundsInDp);
            }
            return;
        }

        synchronized (mActivityScopedObjectsLock) {
            mPendingActionManager.requestSetBounds(boundsInDp);
            mState.set(State.PENDING_UPDATE);
            setBoundsInDpInternalLocked(boundsInDp);
        }
    }

    @Override
    public void onTopResumedActivityChangedWithNative(boolean isTopResumedActivity) {
        if (isTopResumedActivity) {
            mLastActivatedTimeMillis.set(TimeUtils.elapsedRealtimeMillis());
            if (mShouldDispatchPendingDeactivate) {
                ChromeAndroidTaskTrackerImpl.getInstance().activatePenultimatelyActivatedTask();
                mShouldDispatchPendingDeactivate = false;
            }
            if (mState.get() == State.PENDING_UPDATE) {
                var actions =
                        mPendingActionManager.getAndClearTargetPendingActions(
                                PendingAction.ACTIVATE, PendingAction.SHOW);
                maybeSetStateIdle(actions);
            }
        }

        synchronized (mFeaturesLock) {
            for (var feature : mFeatures) {
                feature.onTaskFocusChanged(isTopResumedActivity);
            }
        }
    }

    @Override
    public void didAddTab(
            Tab tab,
            @TabLaunchType int type,
            @TabCreationState int creationState,
            boolean markedForSelection) {
        if (isDestroyed()) return;

        Profile newTabProfile = tab.getProfile();
        assert mInitialProfile.equals(newTabProfile)
                : "A tab with a different profile was added to this task. Initial: "
                        + mInitialProfile
                        + ", New: "
                        + newTabProfile;
    }

    @Nullable ActivityScopedObjects getActivityScopedObjectsForTesting() {
        synchronized (mActivityScopedObjectsLock) {
            return mActivityScopedObjects;
        }
    }

    @Override
    public List<ChromeAndroidTaskFeature> getAllFeaturesForTesting() {
        synchronized (mFeaturesLock) {
            return mFeatures;
        }
    }

    @Override
    public @Nullable Integer getSessionIdForTesting() {
        return mAndroidBrowserWindow.getNativeSessionIdForTesting();
    }

    @VisibleForTesting
    State getState() {
        return assumeNonNull(mState.get());
    }

    private void setActivityScopedObjectsInternal(ActivityScopedObjects activityScopedObjects) {
        synchronized (mActivityScopedObjectsLock) {
            assert mActivityScopedObjects == null
                    : "This Task is already associated with an Activity.";
            mActivityScopedObjects = activityScopedObjects;

            var activityWindowAndroid = activityScopedObjects.mActivityWindowAndroid;
            switch (getState()) {
                case PENDING_CREATE:
                    assert mId == null;
                    assert mPendingTaskInfo != null;
                    break;
                case IDLE:
                    assert mPendingTaskInfo == null;
                    assert mId != null;
                    assert mId == getActivity(activityWindowAndroid).getTaskId()
                            : "The new ActivityWindowAndroid doesn't belong to this Task.";
                    break;
                default:
                    assert false : "Found unexpected Task state.";
            }

            // Register Activity LifecycleObservers
            getActivityLifecycleDispatcher(activityWindowAndroid).register(this);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                    && activityWindowAndroid.getInsetObserver() != null) {
                activityWindowAndroid
                        .getInsetObserver()
                        .addWindowInsetsAnimationListener(mWindowInsetsAnimationListener);
            }

            // Register TabModel observer.
            activityScopedObjects.mTabModel.addObserver(this);
            activityScopedObjects.mTabModel.associateWithBrowserWindow(
                    mAndroidBrowserWindow.getOrCreateNativePtr());

            // Transition from PENDING_CREATE to IDLE.
            if (mState.get() == State.PENDING_CREATE) {
                mId = getActivity(activityWindowAndroid).getTaskId();

                mState.set(State.IDLE);
                dispatchPendingActionsLocked(activityWindowAndroid);

                JniOnceCallback<Long> taskCreationCallbackForNative =
                        assertNonNull(mPendingTaskInfo).mTaskCreationCallbackForNative;
                if (taskCreationCallbackForNative != null) {
                    taskCreationCallbackForNative.onResult(
                            mAndroidBrowserWindow.getOrCreateNativePtr());
                }

                mPendingTaskInfo = null;
            }
        }
    }

    @GuardedBy("mActivityScopedObjectsLock")
    @SuppressLint("NewApi")
    private void dispatchPendingActionsLocked(ActivityWindowAndroid activityWindowAndroid) {
        // Initiate actions on a live Task.
        assertAlive();
        Rect boundsInDp = mPendingActionManager.getPendingBoundsInDp();
        Rect restoredBoundsInDp = mPendingActionManager.getPendingRestoredBoundsInDp();
        @PendingAction int[] pendingActions = mPendingActionManager.getAndClearPendingActions();
        for (@PendingAction int action : pendingActions) {
            if (action == PendingAction.NONE) continue;
            switch (action) {
                case PendingAction.SHOW:
                    showInternalLocked();
                    break;
                case PendingAction.SHOW_INACTIVE:
                case PendingAction.DEACTIVATE:
                    // We will not activate the penultimately active task just yet (in order to
                    // deactivate the current task) because at the time this method is invoked, the
                    // current task's activated time is not guaranteed to be set in order to
                    // correctly determine the penultimate task. We will therefore dispatch this
                    // action after the current task's activated time is set.
                    mShouldDispatchPendingDeactivate = true;
                    break;
                case PendingAction.CLOSE:
                    closeInternalLocked();
                    break;
                case PendingAction.ACTIVATE:
                    activateInternalLocked();
                    break;
                case PendingAction.MAXIMIZE:
                    maximizeInternalLocked();
                    break;
                case PendingAction.MINIMIZE:
                    minimizeInternalLocked();
                    break;
                case PendingAction.RESTORE:
                    // RESTORE should be ignored to fall back to default startup bounds if
                    // non-empty, non-default bounds are not requested in pending state.
                    if (restoredBoundsInDp != null && !restoredBoundsInDp.isEmpty()) {
                        mRestoredBoundsInPx =
                                DisplayUtil.scaleToEnclosingRect(
                                        restoredBoundsInDp,
                                        activityWindowAndroid.getDisplay().getDipScale());
                        restoreInternalLocked();
                    }
                    break;
                case PendingAction.SET_BOUNDS:
                    assert boundsInDp != null;
                    setBoundsInDpInternalLocked(boundsInDp);
                    break;
                default:
                    assert false : "Unsupported pending action.";
            }
        }
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private @Nullable ActivityWindowAndroid getActivityWindowAndroidInternalLocked(
            boolean assertAlive) {
        if (assertAlive) {
            assertAlive();
        }

        return mActivityScopedObjects == null
                ? null
                : mActivityScopedObjects.mActivityWindowAndroid;
    }

    private void clearActivityScopedObjectsInternal() {
        synchronized (mActivityScopedObjectsLock) {
            if (mActivityScopedObjects == null) {
                return;
            }

            // Unregister Activity LifecycleObservers.
            var activityWindowAndroid = mActivityScopedObjects.mActivityWindowAndroid;
            getActivityLifecycleDispatcher(activityWindowAndroid).unregister(this);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                    && activityWindowAndroid.getInsetObserver() != null) {
                // Unregister WindowInsetsAnimationListener.
                activityWindowAndroid
                        .getInsetObserver()
                        .removeWindowInsetsAnimationListener(mWindowInsetsAnimationListener);
            }

            // Unregister TabModel observer
            mActivityScopedObjects.mTabModel.removeObserver(this);

            mActivityScopedObjects = null;
        }
    }

    private void destroyFeatures() {
        synchronized (mFeaturesLock) {
            for (var feature : mFeatures) {
                feature.onTaskRemoved();
            }
            mFeatures.clear();
        }
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private Rect getCurrentBoundsInDpLocked(ActivityWindowAndroid activityWindowAndroid) {
        Rect boundsInPx = getCurrentBoundsInPxLocked(activityWindowAndroid);
        return DisplayUtil.scaleToEnclosingRect(
                boundsInPx, 1.0f / activityWindowAndroid.getDisplay().getDipScale());
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private Rect getCurrentBoundsInPxLocked(ActivityWindowAndroid activityWindowAndroid) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "getBoundsInPxLocked() requires Android R+; returning an empty Rect()");
            return new Rect();
        }

        Activity activity = activityWindowAndroid.getActivity().get();
        if (activity == null) {
            return new Rect();
        }

        return activity.getWindowManager().getCurrentWindowMetrics().getBounds();
    }

    private void assertAlive() {
        assert mState.get() == State.IDLE || mState.get() == State.PENDING_UPDATE
                : "This Task is not alive.";
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private boolean isActiveInternalLocked(ActivityWindowAndroid activityWindowAndroid) {
        return activityWindowAndroid.isTopResumedActivity();
    }

    @GuardedBy("mActivityScopedObjectsLock")
    @RequiresApi(api = VERSION_CODES.R)
    private boolean isRestoredInternalLocked(ActivityWindowAndroid activityWindowAndroid) {
        return !isMinimizedInternalLocked(activityWindowAndroid)
                && !isMaximizedInternalLocked(activityWindowAndroid)
                && !isFullscreenInternalLocked(activityWindowAndroid);
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private boolean isVisibleInternalLocked(@Nullable ActivityWindowAndroid activityWindowAndroid) {
        if (activityWindowAndroid == null) return false;
        return ApplicationStatus.isTaskVisible(getActivity(activityWindowAndroid).getTaskId());
    }

    @GuardedBy("mActivityScopedObjectsLock")
    @RequiresApi(api = VERSION_CODES.R)
    private boolean isMaximizedInternalLocked(
            @Nullable ActivityWindowAndroid activityWindowAndroid) {
        if (activityWindowAndroid == null) return false;
        var activity = activityWindowAndroid.getActivity().get();
        if (activity == null) return false;
        if (activity.isInMultiWindowMode()) {
            // Desktop windowing mode is also a multi-window mode.
            Rect maxBoundsInPx =
                    ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(
                            activity.getWindowManager());
            return getCurrentBoundsInPxLocked(activityWindowAndroid).equals(maxBoundsInPx);
        } else {
            // In non-multi-window mode, Chrome is maximized by default.
            return true;
        }
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private boolean isMinimizedInternalLocked(
            @Nullable ActivityWindowAndroid activityWindowAndroid) {
        return !isVisibleInternalLocked(activityWindowAndroid);
    }

    @GuardedBy("mActivityScopedObjectsLock")
    @RequiresApi(api = VERSION_CODES.R)
    private boolean isFullscreenInternalLocked(
            @Nullable ActivityWindowAndroid activityWindowAndroid) {
        if (activityWindowAndroid == null) return false;
        Activity activity = activityWindowAndroid.getActivity().get();
        if (activity == null) return false;
        Window window = activity.getWindow();
        var windowManager = activity.getWindowManager();
        /** See {@link CompositorViewHolder#isInFullscreenMode}. */
        return !windowManager
                        .getMaximumWindowMetrics()
                        .getWindowInsets()
                        .isVisible(WindowInsets.Type.statusBars())
                || (window.getInsetsController() != null
                        && window.getInsetsController().getSystemBarsBehavior()
                                == WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private void setBoundsInPxLocked(Activity activity, DisplayAndroid display, Rect boundsInPx) {
        var aconfigFlaggedApiDelegate = AconfigFlaggedApiDelegate.getInstance();
        if (aconfigFlaggedApiDelegate == null) {
            Log.w(TAG, "Unable to set bounds: AconfigFlaggedApiDelegate isn't available");
            return;
        }

        var appTask = AndroidTaskUtils.getAppTaskFromId(activity, activity.getTaskId());
        if (appTask == null) return;
        var displayId = display.getDisplayId();
        ActivityManager activityManager =
                (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);

        if (!aconfigFlaggedApiDelegate.isTaskMoveAllowedOnDisplay(activityManager, displayId)) {
            Log.w(TAG, "Unable to set bounds: not allowed on display");
            return;
        }

        aconfigFlaggedApiDelegate
                .moveTaskToWithPromise(appTask, displayId, boundsInPx)
                .then(
                        (pair) -> {
                            var actions =
                                    mPendingActionManager.getAndClearTargetPendingActions(
                                            PendingAction.MAXIMIZE, PendingAction.SET_BOUNDS);
                            maybeSetStateIdle(actions);
                        },
                        (e) -> {
                            if (e == null) return;
                            Log.w(TAG, "Fail to move task to bounds: %s", e.getMessage());
                        });
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private void showInternalLocked() {
        var activityWindowAndroid = getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        var activity =
                activityWindowAndroid != null ? activityWindowAndroid.getActivity().get() : null;
        if (activity == null) return;
        // Activate the Task if it's already visible.
        if (isVisibleInternalLocked(activityWindowAndroid)) {
            ActivityManager activityManager =
                    (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
            mPendingActionManager.requestAction(PendingAction.SHOW);
            mState.set(State.PENDING_UPDATE);
            activityManager.moveTaskToFront(activity.getTaskId(), 0);
        }
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private void closeInternalLocked() {
        var activityWindowAndroid = getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        if (activityWindowAndroid == null) return;
        Activity activity = activityWindowAndroid.getActivity().get();
        if (activity == null) return;
        activity.finishAndRemoveTask();
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private void activateInternalLocked() {
        var activityWindowAndroid = getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        if (activityWindowAndroid == null) return;
        Activity activity = activityWindowAndroid.getActivity().get();
        if (activity == null) return;

        ActivityManager activityManager =
                (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
        mPendingActionManager.requestAction(PendingAction.ACTIVATE);
        mState.set(State.PENDING_UPDATE);
        activityManager.moveTaskToFront(activity.getTaskId(), 0);
    }

    @GuardedBy("mActivityScopedObjectsLock")
    @RequiresApi(api = VERSION_CODES.R)
    private void maximizeInternalLocked() {
        var activityWindowAndroid = getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        if (activityWindowAndroid == null) return;
        Activity activity = activityWindowAndroid.getActivity().get();
        if (activity == null) return;
        // No maximize action in non desktop window mode.
        if (!activity.isInMultiWindowMode()) return;
        if (isRestoredInternalLocked(activityWindowAndroid)) {
            mRestoredBoundsInPx = getCurrentBoundsInPxLocked(activityWindowAndroid);
        }
        Rect maxBoundsInPx =
                ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(activity.getWindowManager());
        mPendingActionManager.requestAction(PendingAction.MAXIMIZE);
        mState.set(State.PENDING_UPDATE);
        setBoundsInPxLocked(activity, activityWindowAndroid.getDisplay(), maxBoundsInPx);
    }

    @GuardedBy("mActivityScopedObjectsLock")
    @RequiresApi(api = VERSION_CODES.R)
    private void minimizeInternalLocked() {
        var activityWindowAndroid = getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        // https://crbug.com/445247646: minimize an app which is already minimized might make
        // app unable to be activated again.
        if (activityWindowAndroid == null || isMinimizedInternalLocked(activityWindowAndroid)) {
            return;
        }
        if (isRestoredInternalLocked(activityWindowAndroid)) {
            mRestoredBoundsInPx = getCurrentBoundsInPxLocked(activityWindowAndroid);
        }
        getActivity(activityWindowAndroid).moveTaskToBack(/* nonRoot= */ true);
    }

    @RequiresApi(api = VERSION_CODES.R)
    @GuardedBy("mActivityScopedObjectsLock")
    private void restoreInternalLocked() {
        var activityWindowAndroid = getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        if (activityWindowAndroid == null) return;
        Activity activity = activityWindowAndroid.getActivity().get();
        if (activity == null || mRestoredBoundsInPx == null) return;
        if (isMinimizedInternalLocked(activityWindowAndroid)) {
            activateInternalLocked();
        }
        setBoundsInPxLocked(activity, activityWindowAndroid.getDisplay(), mRestoredBoundsInPx);
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private void setBoundsInDpInternalLocked(Rect boundsInDp) {
        var activityWindowAndroid = getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        if (activityWindowAndroid == null) return;
        Activity activity = activityWindowAndroid.getActivity().get();
        if (activity == null) return;

        Rect boundsInPx =
                DisplayUtil.scaleToEnclosingRect(
                        boundsInDp, activityWindowAndroid.getDisplay().getDipScale());
        Rect adjustedBoundsInPx =
                ChromeAndroidTaskBoundsConstraints.apply(
                        boundsInPx,
                        activityWindowAndroid.getDisplay(),
                        activity.getWindowManager());
        setBoundsInPxLocked(activity, activityWindowAndroid.getDisplay(), adjustedBoundsInPx);
    }

    private void maybeSetStateIdle(int[] actions) {
        for (int action : actions) {
            if (action != PendingAction.NONE) {
                return;
            }
        }
        mState.set(State.IDLE);
    }

    @Nullable Rect getRestoredBoundsInPxForTesting() {
        synchronized (mActivityScopedObjectsLock) {
            return mRestoredBoundsInPx;
        }
    }

    PendingActionManager getPendingActionManagerForTesting() {
        return mPendingActionManager;
    }
}
