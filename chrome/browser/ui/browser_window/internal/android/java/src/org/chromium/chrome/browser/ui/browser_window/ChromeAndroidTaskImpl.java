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
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.util.Pair;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import androidx.annotation.GuardedBy;
import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.TaskVisibilityListener;
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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

/** Implements {@link ChromeAndroidTask}. */
@NullMarked
final class ChromeAndroidTaskImpl
        implements ChromeAndroidTask,
                ConfigurationChangedObserver,
                TopResumedActivityChangedWithNativeObserver,
                TabModelObserver,
                TaskVisibilityListener {

    private static final String TAG = "ChromeAndroidTask";

    /** States of this {@link ChromeAndroidTask}. */
    @VisibleForTesting
    @IntDef({
        State.UNKNOWN,
        State.PENDING_CREATE,
        State.PENDING_UPDATE,
        State.IDLE,
        State.DESTROYING,
        State.DESTROYED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        /** The Task is not yet initialized. */
        int UNKNOWN = 0;

        /** The Task is pending and not yet associated with an Activity. */
        int PENDING_CREATE = 1;

        /** The Task has a state being updated but not finished yet. */
        int PENDING_UPDATE = 2;

        /* The Task is alive without any pending state change. */
        int IDLE = 3;

        /** The Task is being destroyed, but the destruction hasn't been completed. */
        int DESTROYING = 4;

        /** The Task has been destroyed. */
        int DESTROYED = 5;
    }

    /** Interface for logic using an {@link Activity} and its {@link ActivityWindowAndroid}. */
    private interface ActivityUser<T> {
        T use(Activity activity, ActivityWindowAndroid activityWindowAndroid);
    }

    private final AtomicInteger mState = new AtomicInteger(State.UNKNOWN);

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
                    useActivity(
                            new ActivityUser<>() {
                                @Override
                                @GuardedBy("mActivityScopedObjectsLock")
                                public Void use(
                                        Activity activity,
                                        ActivityWindowAndroid activityWindowAndroid) {
                                    mIsRestored = isRestoredInternalLocked(activity);
                                    mBoundsBeforeAnimation = getCurrentBoundsInPxLocked(activity);
                                    return null;
                                }
                            });
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
                    useActivity(
                            new ActivityUser<>() {
                                @Override
                                @GuardedBy("mActivityScopedObjectsLock")
                                public Void use(
                                        Activity activity,
                                        ActivityWindowAndroid activityWindowAndroid) {
                                    boolean isFullscreen = isFullscreenInternalLocked(activity);
                                    if (mIsRestored && isFullscreen) {
                                        mRestoredBoundsInPx = mBoundsBeforeAnimation;
                                    }
                                    return null;
                                }
                            });
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
        mPendingActionManager.updateFutureStates(mPendingTaskInfo);
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
    public void onNativeInitializationFinished() {
        synchronized (mActivityScopedObjectsLock) {
            if (mPendingTaskInfo == null || mActivityScopedObjects == null) {
                return;
            }

            // Transition from PENDING_CREATE to IDLE.
            assert mState.get() == State.PENDING_CREATE;
            assert mId == null;

            var activityWindowAndroid = mActivityScopedObjects.mActivityWindowAndroid;
            var activity = getActivity(activityWindowAndroid);
            mId = activity.getTaskId();
            mState.set(State.IDLE);
            dispatchPendingActionsLocked(activity, activityWindowAndroid);

            JniOnceCallback<Long> taskCreationCallbackForNative =
                    mPendingTaskInfo.mTaskCreationCallbackForNative;
            if (taskCreationCallbackForNative != null) {
                taskCreationCallbackForNative.onResult(
                        mAndroidBrowserWindow.getOrCreateNativePtr());
            }

            mPendingTaskInfo = null;
        }
    }

    @Override
    public @Nullable ActivityWindowAndroid getActivityWindowAndroid() {
        synchronized (mActivityScopedObjectsLock) {
            Pair<Activity, ActivityWindowAndroid> pair = getActivityAndWindowAndroidLocked();
            return pair == null ? null : pair.second;
        }
    }

    @Override
    public void clearActivityScopedObjects() {
        clearActivityScopedObjectsInternal();
    }

    @Override
    public void addFeature(ChromeAndroidTaskFeature feature) {
        synchronized (mFeaturesLock) {
            assertPendingCreateOrIdle();
            mFeatures.add(feature);
            feature.onAddedToTask();
        }
    }

    @Override
    public @Nullable Intent createIntentForNormalBrowserWindow(boolean isIncognito) {
        synchronized (mActivityScopedObjectsLock) {
            if (mActivityScopedObjects == null) {
                return null;
            }

            var multiInstanceManager = mActivityScopedObjects.mMultiInstanceManager;
            if (multiInstanceManager == null) {
                return null;
            }

            return multiInstanceManager.createNewWindowIntent(isIncognito);
        }
    }

    @Override
    public long getOrCreateNativeBrowserWindowPtr() {
        assert getState() == State.PENDING_CREATE
                        || getState() == State.IDLE
                        || getState() == State.PENDING_UPDATE
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

        return useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Boolean use(
                            Activity unused, ActivityWindowAndroid activityWindowAndroid) {
                        return isActiveInternalLocked(activityWindowAndroid);
                    }
                },
                /* defaultValue= */ false);
    }

    @Override
    public boolean isMaximized() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "isMaximized() requires Android R+; returning a false");
            return false;
        }

        @Nullable Boolean isMaximizedFuture = mPendingActionManager.isMaximizedFuture(mState.get());
        if (isMaximizedFuture != null) {
            return isMaximizedFuture;
        }

        return useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Boolean use(Activity activity, ActivityWindowAndroid unused) {
                        return isMaximizedInternalLocked(activity);
                    }
                },
                /* defaultValue= */ false);
    }

    @Override
    public boolean isMinimized() {
        @Nullable Boolean isVisibleFuture = mPendingActionManager.isVisibleFuture(mState.get());
        if (isVisibleFuture != null) {
            return !isVisibleFuture;
        }

        return useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Boolean use(Activity activity, ActivityWindowAndroid unused) {
                        return isMinimizedInternalLocked(activity);
                    }
                },
                /* defaultValue= */ false);
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

        return useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Boolean use(Activity activity, ActivityWindowAndroid unused) {
                        return isFullscreenInternalLocked(activity);
                    }
                },
                /* defaultValue= */ false);
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

        return useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Rect use(
                            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
                        Rect restoredBoundsInPx =
                                mRestoredBoundsInPx == null
                                        ? getCurrentBoundsInPxLocked(activity)
                                        : mRestoredBoundsInPx;
                        return DisplayUtil.scaleToEnclosingRect(
                                restoredBoundsInPx,
                                1.0f / activityWindowAndroid.getDisplay().getDipScale());
                    }
                },
                /* defaultValue= */ new Rect());
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

        return useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Rect use(
                            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
                        return getCurrentBoundsInDpLocked(activity, activityWindowAndroid);
                    }
                },
                /* defaultValue= */ new Rect());
    }

    @Override
    public void show() {
        if (Boolean.TRUE.equals(mPendingActionManager.isActiveFuture(mState.get()))) {
            return;
        }

        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.SHOW);
            return;
        }

        useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Void use(
                            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
                        showInternalLocked(activity, activityWindowAndroid);
                        return null;
                    }
                });
    }

    @Override
    public boolean isVisible() {
        Boolean isVisible = mPendingActionManager.isVisibleFuture(mState.get());
        if (isVisible != null) return isVisible;

        return useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Boolean use(
                            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
                        return isVisibleInternalLocked(activity);
                    }
                },
                /* defaultValue= */ false);
    }

    @Override
    public void showInactive() {
        // ShowInactive is used to create an unfocused window. Due to the current api limitation,
        // an active window is created first and then deactivated to activate another window.
        if (Boolean.FALSE.equals(mPendingActionManager.isActiveFuture(mState.get()))) return;

        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.SHOW_INACTIVE);
            return;
        }

        useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Void use(
                            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
                        if (!isActiveInternalLocked(activityWindowAndroid)) {
                            return null;
                        }

                        mPendingActionManager.requestAction(PendingAction.SHOW_INACTIVE);
                        mState.set(State.PENDING_UPDATE);
                        ChromeAndroidTaskTrackerImpl.getInstance()
                                .activatePenultimatelyActivatedTask();
                        return null;
                    }
                });
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
        useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Void use(
                            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
                        var newBoundsInDp =
                                getCurrentBoundsInDpLocked(activity, activityWindowAndroid);
                        if (newBoundsInDp.equals(mLastBoundsInDpOnConfigChanged)) {
                            return null;
                        }

                        mLastBoundsInDpOnConfigChanged = newBoundsInDp;
                        synchronized (mFeaturesLock) {
                            for (var feature : mFeatures) {
                                feature.onTaskBoundsChanged(newBoundsInDp);
                            }
                        }

                        return null;
                    }
                });
    }

    @Override
    public void close() {
        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.CLOSE);
            return;
        }

        useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Void use(Activity activity, ActivityWindowAndroid unused) {
                        closeInternalLocked(activity);
                        return null;
                    }
                });
    }

    @Override
    public void activate() {
        if (Boolean.TRUE.equals(mPendingActionManager.isActiveFuture(mState.get()))) return;

        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.ACTIVATE);
            return;
        }

        useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Void use(
                            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
                        if (!isActiveInternalLocked(activityWindowAndroid)) {
                            activateInternalLocked(activity);
                        }
                        return null;
                    }
                });
    }

    @Override
    public void deactivate() {
        if (Boolean.FALSE.equals(mPendingActionManager.isActiveFuture(mState.get()))) return;

        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.DEACTIVATE);
            return;
        }

        useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Void use(Activity unused, ActivityWindowAndroid activityWindowAndroid) {
                        if (!isActiveInternalLocked(activityWindowAndroid)) {
                            return null;
                        }

                        mPendingActionManager.requestAction(PendingAction.DEACTIVATE);
                        mState.set(State.PENDING_UPDATE);

                        ChromeAndroidTaskTrackerImpl.getInstance()
                                .activatePenultimatelyActivatedTask();
                        return null;
                    }
                });
    }

    @Override
    public void maximize() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "maximize() requires Android R+; does nothing");
            return;
        }

        if (Boolean.TRUE.equals(mPendingActionManager.isMaximizedFuture(mState.get()))) return;

        if (mState.get() == State.PENDING_CREATE) {
            // TODO(crbug.com/459857984): remove empty bound and set a correct bound.
            mPendingActionManager.requestMaximize(new Rect());
            return;
        }

        useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Void use(
                            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
                        maximizeInternalLocked(activity, activityWindowAndroid);
                        return null;
                    }
                });
    }

    @Override
    public void minimize() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "minimize() requires Android R+; does nothing");
            return;
        }

        if (Boolean.FALSE.equals(mPendingActionManager.isVisibleFuture(mState.get()))) return;

        if (mState.get() == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.MINIMIZE);
            return;
        }

        useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Void use(Activity activity, ActivityWindowAndroid unused) {
                        minimizeInternalLocked(activity);
                        return null;
                    }
                });
    }

    @Override
    public void restore() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "restore() requires Android R+; does nothing");
            return;
        }
        if (mState.get() == State.PENDING_CREATE) {
            // TODO(crbug.com/459857984): remove empty bound and set a correct bound.
            mPendingActionManager.requestRestore(new Rect());
            return;
        }

        useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Void use(
                            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
                        restoreInternalLocked(activity, activityWindowAndroid);
                        return null;
                    }
                });
    }

    @Override
    public void setBoundsInDp(Rect boundsInDp) {
        if (mState.get() == State.PENDING_CREATE) {
            if (!boundsInDp.isEmpty()) {
                mPendingActionManager.requestSetBounds(boundsInDp);
            }
            return;
        }

        useActivity(
                new ActivityUser<>() {
                    @Override
                    @GuardedBy("mActivityScopedObjectsLock")
                    public Void use(
                            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
                        mPendingActionManager.requestSetBounds(boundsInDp);
                        mState.set(State.PENDING_UPDATE);
                        setBoundsInDpInternalLocked(activity, activityWindowAndroid, boundsInDp);
                        return null;
                    }
                });
    }

    @Override
    public void onTopResumedActivityChangedWithNative(boolean isTopResumedActivity) {
        if (isTopResumedActivity) {
            mLastActivatedTimeMillis.set(TimeUtils.elapsedRealtimeMillis());
            if (mShouldDispatchPendingDeactivate) {
                ChromeAndroidTaskTrackerImpl.getInstance().activatePenultimatelyActivatedTask();
                mShouldDispatchPendingDeactivate = false;
            }
        }
        if (mState.get() == State.PENDING_UPDATE) {
            int[] settledActions =
                    isTopResumedActivity
                            ? new int[] {PendingAction.ACTIVATE, PendingAction.SHOW}
                            : new int[] {PendingAction.DEACTIVATE, PendingAction.SHOW_INACTIVE};
            var actions = mPendingActionManager.getAndClearTargetPendingActions(settledActions);
            maybeSetStateIdle(actions);
        }

        synchronized (mFeaturesLock) {
            for (var feature : mFeatures) {
                feature.onTaskFocusChanged(isTopResumedActivity);
            }
        }
    }

    @Override
    public void onTaskVisibilityChanged(int taskId, boolean isVisible) {
        if (mId == null || taskId != mId || mState.get() != State.PENDING_UPDATE) return;
        if (!isVisible) {
            @PendingAction
            int[] actions =
                    mPendingActionManager.getAndClearTargetPendingActions(PendingAction.MINIMIZE);
            maybeSetStateIdle(actions);
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
    @State
    int getState() {
        return assumeNonNull(mState.get());
    }

    private void setActivityScopedObjectsInternal(ActivityScopedObjects activityScopedObjects) {
        synchronized (mActivityScopedObjectsLock) {
            assert mActivityScopedObjects == null
                    : "This Task is already associated with an Activity.";
            mActivityScopedObjects = activityScopedObjects;

            var activityWindowAndroid = activityScopedObjects.mActivityWindowAndroid;
            assertPendingCreateOrIdle();
            if (getState() == State.IDLE) {
                assert mId != null;
                assert mId == getActivity(activityWindowAndroid).getTaskId()
                        : "The new ActivityWindowAndroid doesn't belong to this Task.";
            } else {
                assert mId == null;
            }

            // Register Activity LifecycleObservers
            getActivityLifecycleDispatcher(activityWindowAndroid).register(this);

            // Register Task VisibilityListener
            ApplicationStatus.registerTaskVisibilityListener(this);

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
        }
    }

    @GuardedBy("mActivityScopedObjectsLock")
    @SuppressLint("NewApi")
    private void dispatchPendingActionsLocked(
            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
        // Initiate actions on a live Task.
        assertAlive();

        Rect boundsInDp = mPendingActionManager.getPendingBoundsInDp();
        Rect restoredBoundsInDp = mPendingActionManager.getPendingRestoredBoundsInDp();
        @PendingAction int[] pendingActions = mPendingActionManager.getAndClearPendingActions();
        for (@PendingAction int action : pendingActions) {
            if (action == PendingAction.NONE) continue;
            switch (action) {
                case PendingAction.SHOW:
                    showInternalLocked(activity, activityWindowAndroid);
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
                    closeInternalLocked(activity);
                    break;
                case PendingAction.ACTIVATE:
                    activateInternalLocked(activity);
                    break;
                case PendingAction.MAXIMIZE:
                    maximizeInternalLocked(activity, activityWindowAndroid);
                    break;
                case PendingAction.MINIMIZE:
                    minimizeInternalLocked(activity);
                    break;
                case PendingAction.RESTORE:
                    // RESTORE should be ignored to fall back to default startup bounds if
                    // non-empty, non-default bounds are not requested in pending state.
                    if (restoredBoundsInDp != null && !restoredBoundsInDp.isEmpty()) {
                        mRestoredBoundsInPx =
                                DisplayUtil.scaleToEnclosingRect(
                                        restoredBoundsInDp,
                                        activityWindowAndroid.getDisplay().getDipScale());
                        restoreInternalLocked(activity, activityWindowAndroid);
                    }
                    break;
                case PendingAction.SET_BOUNDS:
                    assert boundsInDp != null;
                    setBoundsInDpInternalLocked(activity, activityWindowAndroid, boundsInDp);
                    break;
                default:
                    assert false : "Unsupported pending action.";
            }
        }
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private @Nullable Pair<Activity, ActivityWindowAndroid> getActivityAndWindowAndroidLocked() {
        assertAlive();
        var activityWindowAndroid =
                mActivityScopedObjects == null
                        ? null
                        : mActivityScopedObjects.mActivityWindowAndroid;
        var activity =
                activityWindowAndroid == null ? null : activityWindowAndroid.getActivity().get();
        return activityWindowAndroid == null || activity == null
                ? null
                : Pair.create(activity, activityWindowAndroid);
    }

    private void clearActivityScopedObjectsInternal() {
        synchronized (mActivityScopedObjectsLock) {
            if (mActivityScopedObjects == null) {
                return;
            }

            // Unregister Activity LifecycleObservers.
            var activityWindowAndroid = mActivityScopedObjects.mActivityWindowAndroid;
            getActivityLifecycleDispatcher(activityWindowAndroid).unregister(this);

            // Unregister Task VisibilityListener.
            ApplicationStatus.unregisterTaskVisibilityListener(this);

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

    private void useActivity(ActivityUser<Void> user) {
        synchronized (mActivityScopedObjectsLock) {
            Pair<Activity, ActivityWindowAndroid> pair = getActivityAndWindowAndroidLocked();
            if (pair != null) {
                user.use(pair.first, pair.second);
            }
        }
    }

    private <T> T useActivity(ActivityUser<T> user, T defaultValue) {
        synchronized (mActivityScopedObjectsLock) {
            Pair<Activity, ActivityWindowAndroid> pair = getActivityAndWindowAndroidLocked();
            return pair == null ? defaultValue : user.use(pair.first, pair.second);
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
    private Rect getCurrentBoundsInDpLocked(
            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
        Rect boundsInPx = getCurrentBoundsInPxLocked(activity);
        return convertBoundsInPxToDp(boundsInPx, activityWindowAndroid.getDisplay());
    }

    private static Rect getCurrentBoundsInPxLocked(Activity activity) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "getBoundsInPxLocked() requires Android R+; returning an empty Rect()");
            return new Rect();
        }

        return activity.getWindowManager().getCurrentWindowMetrics().getBounds();
    }

    private void assertAlive() {
        assert mState.get() == State.IDLE || mState.get() == State.PENDING_UPDATE
                : "This Task is not alive.";
    }

    private void assertPendingCreateOrIdle() {
        assert mState.get() == State.IDLE || mState.get() == State.PENDING_CREATE
                : "This Task is neither pending create nor idle.";
    }

    private static boolean isActiveInternalLocked(ActivityWindowAndroid activityWindowAndroid) {
        return activityWindowAndroid.isTopResumedActivity();
    }

    @RequiresApi(api = VERSION_CODES.R)
    private static boolean isRestoredInternalLocked(Activity activity) {
        return !isMinimizedInternalLocked(activity)
                && !isMaximizedInternalLocked(activity)
                && !isFullscreenInternalLocked(activity);
    }

    private static boolean isVisibleInternalLocked(Activity activity) {
        return ApplicationStatus.isTaskVisible(activity.getTaskId());
    }

    @RequiresApi(api = VERSION_CODES.R)
    private static boolean isMaximizedInternalLocked(Activity activity) {
        if (activity.isInMultiWindowMode()) {
            // Desktop windowing mode is also a multi-window mode.
            Rect maxBoundsInPx =
                    ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(
                            activity.getWindowManager());
            return getCurrentBoundsInPxLocked(activity).equals(maxBoundsInPx);
        } else {
            // In non-multi-window mode, Chrome is maximized by default.
            return true;
        }
    }

    private static boolean isMinimizedInternalLocked(Activity activity) {
        return !isVisibleInternalLocked(activity);
    }

    @RequiresApi(api = VERSION_CODES.R)
    private static boolean isFullscreenInternalLocked(Activity activity) {
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
                                            PendingAction.MAXIMIZE,
                                            PendingAction.SET_BOUNDS,
                                            PendingAction.RESTORE);
                            maybeSetStateIdle(actions);
                        },
                        (e) -> {
                            if (e == null) return;
                            Log.w(TAG, "Fail to move task to bounds: %s", e.getMessage());
                        });
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private void showInternalLocked(
            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
        // No-op if already active.
        if (isActiveInternalLocked(activityWindowAndroid)) return;

        // Activate the Task if it's already visible.
        if (isVisibleInternalLocked(activity)) {
            ActivityManager activityManager =
                    (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
            mPendingActionManager.requestAction(PendingAction.SHOW);
            mState.set(State.PENDING_UPDATE);
            activityManager.moveTaskToFront(activity.getTaskId(), 0);
        }
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private void closeInternalLocked(Activity activity) {
        activity.finishAndRemoveTask();
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private void activateInternalLocked(Activity activity) {
        ActivityManager activityManager =
                (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
        mPendingActionManager.requestAction(PendingAction.ACTIVATE);
        mState.set(State.PENDING_UPDATE);
        activityManager.moveTaskToFront(activity.getTaskId(), 0);
    }

    @GuardedBy("mActivityScopedObjectsLock")
    @RequiresApi(api = VERSION_CODES.R)
    private void maximizeInternalLocked(
            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
        // No maximize action in non desktop window mode.
        if (!activity.isInMultiWindowMode()) return;

        if (isRestoredInternalLocked(activity)) {
            mRestoredBoundsInPx = getCurrentBoundsInPxLocked(activity);
        }

        Rect maxBoundsInPx =
                ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(activity.getWindowManager());
        mPendingActionManager.requestMaximize(
                convertBoundsInPxToDp(maxBoundsInPx, activityWindowAndroid.getDisplay()));
        mState.set(State.PENDING_UPDATE);
        setBoundsInPxLocked(activity, activityWindowAndroid.getDisplay(), maxBoundsInPx);
    }

    @GuardedBy("mActivityScopedObjectsLock")
    @RequiresApi(api = VERSION_CODES.R)
    private void minimizeInternalLocked(Activity activity) {
        if (isMinimizedInternalLocked(activity)) {
            return;
        }
        if (isRestoredInternalLocked(activity)) {
            mRestoredBoundsInPx = getCurrentBoundsInPxLocked(activity);
        }
        mPendingActionManager.requestAction(PendingAction.MINIMIZE);
        mState.set(State.PENDING_UPDATE);
        activity.moveTaskToBack(/* nonRoot= */ true);
    }

    @RequiresApi(api = VERSION_CODES.R)
    @GuardedBy("mActivityScopedObjectsLock")
    private void restoreInternalLocked(
            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
        if (mRestoredBoundsInPx == null) return;

        if (isMinimizedInternalLocked(activity)) {
            activateInternalLocked(activity);
        }

        mPendingActionManager.requestRestore(
                convertBoundsInPxToDp(mRestoredBoundsInPx, activityWindowAndroid.getDisplay()));
        mState.set(State.PENDING_UPDATE);
        setBoundsInPxLocked(activity, activityWindowAndroid.getDisplay(), mRestoredBoundsInPx);
    }

    @GuardedBy("mActivityScopedObjectsLock")
    private void setBoundsInDpInternalLocked(
            Activity activity, ActivityWindowAndroid activityWindowAndroid, Rect boundsInDp) {
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

    @VisibleForTesting
    static Rect convertBoundsInPxToDp(Rect boundsInPx, DisplayAndroid displayAndroid) {
        return DisplayUtil.scaleToEnclosingRect(boundsInPx, 1.0f / displayAndroid.getDipScale());
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
