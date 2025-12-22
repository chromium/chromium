// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.chromium.build.NullUtil.assertNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.ArrayMap;
import android.util.Pair;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

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
import org.chromium.base.ThreadUtils;
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
import java.util.Map;
import java.util.function.Supplier;

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

        /** The Task is alive without any pending state change. */
        int IDLE = 3;

        /** The Task is being destroyed, but the destruction hasn't been completed. */
        int DESTROYING = 4;

        /** The Task has been destroyed. */
        int DESTROYED = 5;
    }

    /**
     * Interface for logic reading a value from an {@link Activity} and its {@link
     * ActivityWindowAndroid}.
     */
    private interface ActivityReader<T> {
        T read(Activity activity, ActivityWindowAndroid activityWindowAndroid);
    }

    /**
     * Interface for logic updating an {@link Activity} and/or its {@link ActivityWindowAndroid}.
     */
    private interface ActivityUpdater {
        void update(Activity activity, ActivityWindowAndroid activityWindowAndroid);
    }

    private final PendingActionManager mPendingActionManager = new PendingActionManager();

    private final @BrowserWindowType int mBrowserWindowType;

    private final AndroidBrowserWindow mAndroidBrowserWindow;
    private final Profile mInitialProfile;

    /**
     * Contains all {@link ChromeAndroidTaskFeature}s associated with this {@link
     * ChromeAndroidTask}.
     */
    private final Map<Class<? extends ChromeAndroidTaskFeature>, ChromeAndroidTaskFeature>
            mFeatures = new ArrayMap<>();

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
    private @Nullable ActivityScopedObjects mActivityScopedObjects;

    private @Nullable Integer mId;
    private @Nullable Long mLastActivatedTimeMillis;
    private @Nullable PendingTaskInfo mPendingTaskInfo;

    /** Last Task (window) bounds updated by {@link #onConfigurationChanged(Configuration)}. */
    private @Nullable Rect mLastBoundsInDpOnConfigChanged;

    /** Non-maximized bounds of the task even when currently maximized or minimized. */
    private @Nullable Rect mRestoredBoundsInPx;

    private @State int mState;
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
                            (activity, unused) -> {
                                mIsRestored = isRestoredInternal(activity);
                                mBoundsBeforeAnimation = getCurrentBoundsInPx(activity);
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
                            (activity, unused) -> {
                                boolean isFullscreen = isFullscreenInternal(activity);
                                if (mIsRestored && isFullscreen) {
                                    mRestoredBoundsInPx = mBoundsBeforeAnimation;
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

        mState = State.IDLE;
        setActivityScopedObjectsInternal(activityScopedObjects);
    }

    ChromeAndroidTaskImpl(PendingTaskInfo pendingTaskInfo) {
        mPendingTaskInfo = pendingTaskInfo;

        mBrowserWindowType = pendingTaskInfo.mCreateParams.getWindowType();
        mAndroidBrowserWindow = new AndroidBrowserWindow(/* chromeAndroidTask= */ this);
        mInitialProfile = pendingTaskInfo.mCreateParams.getProfile();
        mState = State.PENDING_CREATE;
        mPendingActionManager.updateFutureStates(mPendingTaskInfo);
    }

    @Override
    public @Nullable Integer getId() {
        ThreadUtils.assertOnUiThread();
        return mId;
    }

    @Override
    public @Nullable PendingTaskInfo getPendingTaskInfo() {
        ThreadUtils.assertOnUiThread();
        return mPendingTaskInfo;
    }

    @Override
    public @BrowserWindowType int getBrowserWindowType() {
        ThreadUtils.assertOnUiThread();
        return mBrowserWindowType;
    }

    @Override
    public void setActivityScopedObjects(ActivityScopedObjects activityScopedObjects) {
        ThreadUtils.assertOnUiThread();
        setActivityScopedObjectsInternal(activityScopedObjects);
    }

    @Override
    public void onNativeInitializationFinished() {
        ThreadUtils.assertOnUiThread();
        if (mPendingTaskInfo == null || mActivityScopedObjects == null) {
            return;
        }

        // Transition from PENDING_CREATE to IDLE.
        assert mState == State.PENDING_CREATE;
        assert mId == null;

        var activityWindowAndroid = mActivityScopedObjects.mActivityWindowAndroid;
        var activity = getActivity(activityWindowAndroid);
        mId = activity.getTaskId();
        @Nullable Rect futureBounds = mPendingActionManager.getFutureBoundsInDp();
        @Nullable Rect futureRestoredBounds = mPendingActionManager.getFutureRestoredBoundsInDp();
        mState = State.IDLE;
        dispatchPendingActions(activity, activityWindowAndroid, futureBounds, futureRestoredBounds);

        JniOnceCallback<Long> taskCreationCallbackForNative =
                mPendingTaskInfo.mTaskCreationCallbackForNative;
        if (taskCreationCallbackForNative != null) {
            taskCreationCallbackForNative.onResult(mAndroidBrowserWindow.getOrCreateNativePtr());
        }

        mPendingTaskInfo = null;
    }

    @Override
    public @Nullable ActivityWindowAndroid getActivityWindowAndroid() {
        ThreadUtils.assertOnUiThread();
        Pair<Activity, ActivityWindowAndroid> pair = getActivityAndWindowAndroid();
        return pair == null ? null : pair.second;
    }

    @Override
    public void clearActivityScopedObjects() {
        ThreadUtils.assertOnUiThread();
        clearActivityScopedObjectsInternal();
    }

    @Override
    public <T extends ChromeAndroidTaskFeature> void addFeature(
            Class<T> featureClazz, Supplier<@Nullable T> featureSupplier) {
        ThreadUtils.assertOnUiThread();
        assertPendingCreateOrIdle();

        if (mFeatures.containsKey(featureClazz)) {
            return;
        }

        var feature = featureSupplier.get();
        if (feature != null) {
            mFeatures.put(featureClazz, feature);
            feature.onAddedToTask();
        }
    }

    @Override
    public @Nullable Intent createIntentForNormalBrowserWindow(boolean isIncognito) {
        ThreadUtils.assertOnUiThread();
        if (mActivityScopedObjects == null) {
            return null;
        }

        var multiInstanceManager = mActivityScopedObjects.mMultiInstanceManager;
        if (multiInstanceManager == null) {
            return null;
        }

        return multiInstanceManager.createNewWindowIntent(isIncognito);
    }

    @Override
    public long getOrCreateNativeBrowserWindowPtr() {
        ThreadUtils.assertOnUiThread();
        assert mState == State.PENDING_CREATE
                        || mState == State.IDLE
                        || mState == State.PENDING_UPDATE
                : "This Task is not pending or alive.";
        return mAndroidBrowserWindow.getOrCreateNativePtr();
    }

    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        if (mState != State.IDLE) {
            return;
        }

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
        mState = State.DESTROYING;

        if (mPendingTaskInfo != null) {
            mPendingTaskInfo.destroy();
            mPendingTaskInfo = null;
        }

        clearActivityScopedObjectsInternal();
        destroyFeatures();

        mAndroidBrowserWindow.destroy();
        mState = State.DESTROYED;
    }

    @Override
    public boolean isDestroyed() {
        ThreadUtils.assertOnUiThread();
        return mState == State.DESTROYED;
    }

    @Override
    public boolean isActive() {
        ThreadUtils.assertOnUiThread();
        @Nullable Boolean isActiveFuture = mPendingActionManager.isActiveFuture(mState);
        if (isActiveFuture != null) {
            return isActiveFuture;
        }

        return useActivity(
                (unused, activityWindowAndroid) -> isActiveInternal(activityWindowAndroid),
                /* defaultValue= */ false);
    }

    @Override
    public boolean isMaximized() {
        ThreadUtils.assertOnUiThread();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "isMaximized() requires Android R+; returning a false");
            return false;
        }

        @Nullable Boolean isMaximizedFuture = mPendingActionManager.isMaximizedFuture(mState);
        if (isMaximizedFuture != null) {
            return isMaximizedFuture;
        }

        return useActivity(
                (activity, unused) -> isMaximizedInternal(activity), /* defaultValue= */ false);
    }

    @Override
    public boolean isMinimized() {
        ThreadUtils.assertOnUiThread();
        @Nullable Boolean isVisibleFuture = mPendingActionManager.isVisibleFuture(mState);
        if (isVisibleFuture != null) {
            return !isVisibleFuture;
        }

        return useActivity(
                (activity, unused) -> isMinimizedInternal(activity), /* defaultValue= */ false);
    }

    @Override
    public boolean isFullscreen() {
        ThreadUtils.assertOnUiThread();
        if (mState == State.PENDING_CREATE) {
            return false;
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "isFullscreen() requires Android R+; returning false");
            return false;
        }

        return useActivity(
                (activity, unused) -> isFullscreenInternal(activity), /* defaultValue= */ false);
    }

    @Override
    public Rect getRestoredBoundsInDp() {
        ThreadUtils.assertOnUiThread();
        Rect futureRestoredBounds = mPendingActionManager.getFutureRestoredBoundsInDp();
        if (futureRestoredBounds != null) {
            return futureRestoredBounds;
        }

        return useActivity(
                (activity, activityWindowAndroid) -> {
                    Rect restoredBoundsInPx =
                            mRestoredBoundsInPx == null
                                    ? getCurrentBoundsInPx(activity)
                                    : mRestoredBoundsInPx;
                    return DisplayUtil.scaleToEnclosingRect(
                            restoredBoundsInPx,
                            1.0f / activityWindowAndroid.getDisplay().getDipScale());
                },
                /* defaultValue= */ new Rect());
    }

    @Override
    public long getLastActivatedTimeMillis() {
        ThreadUtils.assertOnUiThread();
        return assertNonNull(mLastActivatedTimeMillis);
    }

    @Override
    public Profile getProfile() {
        ThreadUtils.assertOnUiThread();
        return mInitialProfile;
    }

    @Override
    public Rect getBoundsInDp() {
        ThreadUtils.assertOnUiThread();
        var futureBounds = mPendingActionManager.getFutureBoundsInDp();
        if (futureBounds != null) return futureBounds;

        return useActivity(this::getCurrentBoundsInDp, /* defaultValue= */ new Rect());
    }

    @Override
    public void show() {
        ThreadUtils.assertOnUiThread();
        if (Boolean.TRUE.equals(mPendingActionManager.isActiveFuture(mState))) {
            return;
        }

        if (mState == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.SHOW);
            return;
        }

        useActivity(this::showInternal);
    }

    @Override
    public boolean isVisible() {
        ThreadUtils.assertOnUiThread();
        Boolean isVisible = mPendingActionManager.isVisibleFuture(mState);
        if (isVisible != null) return isVisible;

        return useActivity(
                (activity, unused) -> isVisibleInternal(activity), /* defaultValue= */ false);
    }

    @Override
    public void showInactive() {
        ThreadUtils.assertOnUiThread();
        // ShowInactive is used to create an unfocused window. Due to the current api limitation,
        // an active window is created first and then deactivated to activate another window.
        if (Boolean.FALSE.equals(mPendingActionManager.isActiveFuture(mState))) return;

        if (mState == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.SHOW_INACTIVE);
            return;
        }

        useActivity(
                (activity, activityWindowAndroid) -> {
                    if (!isDesktopWindowingMode(activity)
                            || !isActiveInternal(activityWindowAndroid)) {
                        return;
                    }

                    // Do nothing is there is only one task left.
                    if (ChromeAndroidTaskTrackerImpl.getInstance().countOfTasks() <= 1) {
                        return;
                    }

                    mPendingActionManager.requestAction(PendingAction.SHOW_INACTIVE);
                    mState = State.PENDING_UPDATE;
                    ChromeAndroidTaskTrackerImpl.getInstance().activatePenultimatelyActivatedTask();
                });
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        ThreadUtils.assertOnUiThread();

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
                (activity, activityWindowAndroid) -> {
                    var newBoundsInDp = getCurrentBoundsInDp(activity, activityWindowAndroid);
                    if (newBoundsInDp.equals(mLastBoundsInDpOnConfigChanged)) {
                        return;
                    }

                    mLastBoundsInDpOnConfigChanged = newBoundsInDp;
                    for (var feature : mFeatures.values()) {
                        feature.onTaskBoundsChanged(newBoundsInDp);
                    }
                });
    }

    @Override
    public void close() {
        ThreadUtils.assertOnUiThread();
        if (mState == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.CLOSE);
            return;
        }

        useActivity((activity, unused) -> closeInternal(activity));
    }

    @Override
    public void activate() {
        ThreadUtils.assertOnUiThread();
        if (Boolean.TRUE.equals(mPendingActionManager.isActiveFuture(mState))) return;

        if (mState == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.ACTIVATE);
            return;
        }

        useActivity(
                (activity, activityWindowAndroid) -> {
                    if (!isActiveInternal(activityWindowAndroid)) {
                        activateInternal(activity);
                    }
                });
    }

    @Override
    public void deactivate() {
        ThreadUtils.assertOnUiThread();
        if (Boolean.FALSE.equals(mPendingActionManager.isActiveFuture(mState))) return;

        if (mState == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.DEACTIVATE);
            return;
        }

        useActivity(
                (activity, activityWindowAndroid) -> {
                    if (!isDesktopWindowingMode(activity)
                            || !isActiveInternal(activityWindowAndroid)) {
                        return;
                    }

                    // Do nothing if there is only one task left.
                    if (ChromeAndroidTaskTrackerImpl.getInstance().countOfTasks() <= 1) {
                        return;
                    }

                    mPendingActionManager.requestAction(PendingAction.DEACTIVATE);
                    mState = State.PENDING_UPDATE;

                    ChromeAndroidTaskTrackerImpl.getInstance().activatePenultimatelyActivatedTask();
                });
    }

    @Override
    public void maximize() {
        ThreadUtils.assertOnUiThread();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "maximize() requires Android R+; does nothing");
            return;
        }

        if (Boolean.TRUE.equals(mPendingActionManager.isMaximizedFuture(mState))) return;

        if (mState == State.PENDING_CREATE) {
            mPendingActionManager.requestMaximize();
            return;
        }

        useActivity(
                (activity, activityWindowAndroid) -> {
                    // No maximize action in non desktop window mode.
                    if (isDesktopWindowingMode(activity)) {
                        maximizeInternal(activity, activityWindowAndroid);
                    }
                });
    }

    @Override
    public void minimize() {
        ThreadUtils.assertOnUiThread();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "minimize() requires Android R+; does nothing");
            return;
        }

        if (Boolean.FALSE.equals(mPendingActionManager.isVisibleFuture(mState))) return;

        if (mState == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.MINIMIZE);
            return;
        }

        useActivity((activity, unused) -> minimizeInternal(activity));
    }

    @Override
    public void restore() {
        ThreadUtils.assertOnUiThread();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "restore() requires Android R+; does nothing");
            return;
        }
        if (mState == State.PENDING_CREATE) {
            // TODO(crbug.com/459857984): remove empty bound and set a correct bound.
            mPendingActionManager.requestRestore(new Rect());
            return;
        }

        useActivity(
                (activity, activityWindowAndroid) -> {
                    if (isDesktopWindowingMode(activity)) {
                        restoreInternal(activity, activityWindowAndroid);
                    }
                });
    }

    @Override
    public void setBoundsInDp(Rect boundsInDp) {
        ThreadUtils.assertOnUiThread();
        var futureBounds = mPendingActionManager.getFutureBoundsInDp();
        if (futureBounds != null && futureBounds.equals(boundsInDp)) {
            return;
        }

        if (mState == State.PENDING_CREATE) {
            mPendingActionManager.requestSetBounds(boundsInDp);
            return;
        }

        useActivity(
                (activity, activityWindowAndroid) -> {
                    if (!isDesktopWindowingMode(activity)
                            || getCurrentBoundsInDp(activity, activityWindowAndroid)
                                    .equals(boundsInDp)) {
                        return;
                    }

                    mPendingActionManager.requestSetBounds(boundsInDp);
                    mState = State.PENDING_UPDATE;
                    setBoundsInDpInternal(activity, activityWindowAndroid, boundsInDp);
                });
    }

    @Override
    public void onTopResumedActivityChangedWithNative(boolean isTopResumedActivity) {
        ThreadUtils.assertOnUiThread();

        if (isTopResumedActivity) {
            mLastActivatedTimeMillis = TimeUtils.elapsedRealtimeMillis();
            if (mShouldDispatchPendingDeactivate) {
                ChromeAndroidTaskTrackerImpl.getInstance().activatePenultimatelyActivatedTask();
                mShouldDispatchPendingDeactivate = false;
            }
        }
        if (mState == State.PENDING_UPDATE) {
            int[] settledActions =
                    isTopResumedActivity
                            ? new int[] {PendingAction.ACTIVATE, PendingAction.SHOW}
                            : new int[] {PendingAction.DEACTIVATE, PendingAction.SHOW_INACTIVE};
            var actions = mPendingActionManager.getAndClearTargetPendingActions(settledActions);
            maybeSetStateIdle(actions);
        }

        for (var feature : mFeatures.values()) {
            feature.onTaskFocusChanged(isTopResumedActivity);
        }
    }

    @Override
    public void onTaskVisibilityChanged(int taskId, boolean isVisible) {
        ThreadUtils.assertOnUiThread();
        if (mId == null || taskId != mId || mState != State.PENDING_UPDATE) return;
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
        ThreadUtils.assertOnUiThread();
        return mActivityScopedObjects;
    }

    @Override
    public @Nullable ChromeAndroidTaskFeature getFeatureForTesting(
            Class<? extends ChromeAndroidTaskFeature> featureClazz) {
        ThreadUtils.assertOnUiThread();
        return mFeatures.get(featureClazz);
    }

    @Override
    public List<ChromeAndroidTaskFeature> getAllFeaturesForTesting() {
        ThreadUtils.assertOnUiThread();
        return new ArrayList<>(mFeatures.values());
    }

    @Override
    public @Nullable Integer getSessionIdForTesting() {
        return mAndroidBrowserWindow.getNativeSessionIdForTesting();
    }

    @VisibleForTesting
    @State
    int getState() {
        return mState;
    }

    private void setActivityScopedObjectsInternal(ActivityScopedObjects activityScopedObjects) {
        assert mActivityScopedObjects == null : "This Task is already associated with an Activity.";
        mActivityScopedObjects = activityScopedObjects;

        var activityWindowAndroid = activityScopedObjects.mActivityWindowAndroid;
        assertPendingCreateOrIdle();
        if (mState == State.IDLE) {
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

        // Cache the maximize bound.
        if (VERSION.SDK_INT >= VERSION_CODES.R) {
            var activity = getActivity(activityWindowAndroid);
            var maximumBoundsInDp =
                    convertBoundsInPxToDp(
                            ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(
                                    activity.getWindowManager()),
                            activityWindowAndroid.getDisplay());
            PendingActionManager.setMaximumBounds(maximumBoundsInDp);
        }
    }

    /**
     * @param activityWindowAndroid The associated {@link ActivityWindowAndroid}.
     * @param futureBoundsInDp The future bounds the task is supposed to be when becoming alive.
     * @param futureRestoredBoundsInDp The restored bounds recorded before becoming alive.
     */
    @SuppressLint("NewApi")
    private void dispatchPendingActions(
            Activity activity,
            ActivityWindowAndroid activityWindowAndroid,
            @Nullable Rect futureBoundsInDp,
            @Nullable Rect futureRestoredBoundsInDp) {
        // Initiate actions on a live Task.
        assertAlive();

        @PendingAction int[] pendingActions = mPendingActionManager.getAndClearPendingActions();
        for (@PendingAction int action : pendingActions) {
            if (action == PendingAction.NONE) continue;
            switch (action) {
                case PendingAction.SHOW:
                    showInternal(activity, activityWindowAndroid);
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
                    closeInternal(activity);
                    break;
                case PendingAction.ACTIVATE:
                    activateInternal(activity);
                    break;
                case PendingAction.MAXIMIZE:
                    maximizeInternal(activity, activityWindowAndroid);
                    break;
                case PendingAction.MINIMIZE:
                    minimizeInternal(activity);
                    break;
                case PendingAction.RESTORE:
                    // RESTORE should be ignored to fall back to default startup bounds if
                    // non-empty, non-default bounds are not requested in pending state.
                    if (futureRestoredBoundsInDp != null && !futureRestoredBoundsInDp.isEmpty()) {
                        mRestoredBoundsInPx =
                                DisplayUtil.scaleToEnclosingRect(
                                        futureRestoredBoundsInDp,
                                        activityWindowAndroid.getDisplay().getDipScale());
                        restoreInternal(activity, activityWindowAndroid);
                    }
                    break;
                case PendingAction.SET_BOUNDS:
                    assert futureBoundsInDp != null;
                    setBoundsInDpInternal(activity, activityWindowAndroid, futureBoundsInDp);
                    break;
                default:
                    assert false : "Unsupported pending action.";
            }
        }
    }

    private @Nullable Pair<Activity, ActivityWindowAndroid> getActivityAndWindowAndroid() {
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

    private void useActivity(ActivityUpdater updater) {
        Pair<Activity, ActivityWindowAndroid> pair = getActivityAndWindowAndroid();
        if (pair != null) {
            updater.update(pair.first, pair.second);
        }
    }

    private <T> T useActivity(ActivityReader<T> reader, T defaultValue) {
        Pair<Activity, ActivityWindowAndroid> pair = getActivityAndWindowAndroid();
        return pair == null ? defaultValue : reader.read(pair.first, pair.second);
    }

    private void destroyFeatures() {
        for (var feature : mFeatures.values()) {
            feature.onTaskRemoved();
        }
        mFeatures.clear();
    }

    private Rect getCurrentBoundsInDp(
            Activity activity, ActivityWindowAndroid activityWindowAndroid) {
        Rect boundsInPx = getCurrentBoundsInPx(activity);
        return convertBoundsInPxToDp(boundsInPx, activityWindowAndroid.getDisplay());
    }

    private static Rect getCurrentBoundsInPx(Activity activity) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "getBoundsInPx() requires Android R+; returning an empty Rect()");
            return new Rect();
        }

        return activity.getWindowManager().getCurrentWindowMetrics().getBounds();
    }

    private void assertAlive() {
        assert mState == State.IDLE || mState == State.PENDING_UPDATE : "This Task is not alive.";
    }

    private void assertPendingCreateOrIdle() {
        assert mState == State.IDLE || mState == State.PENDING_CREATE
                : "This Task is neither pending create nor idle.";
    }

    private static boolean isDesktopWindowingMode(Activity activity) {
        // TODO(crbug.com/467457794): Identify a more reliable alternative for desktop windowing
        //  mode detection.
        return activity.isInMultiWindowMode();
    }

    private static boolean isActiveInternal(ActivityWindowAndroid activityWindowAndroid) {
        return activityWindowAndroid.isTopResumedActivity();
    }

    @RequiresApi(api = VERSION_CODES.R)
    private static boolean isRestoredInternal(Activity activity) {
        return !isMinimizedInternal(activity)
                && !isMaximizedInternal(activity)
                && !isFullscreenInternal(activity);
    }

    private static boolean isVisibleInternal(Activity activity) {
        return ApplicationStatus.isTaskVisible(activity.getTaskId());
    }

    @RequiresApi(api = VERSION_CODES.R)
    private static boolean isMaximizedInternal(Activity activity) {
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

    private static boolean isMinimizedInternal(Activity activity) {
        return !isVisibleInternal(activity);
    }

    @RequiresApi(api = VERSION_CODES.R)
    private static boolean isFullscreenInternal(Activity activity) {
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

    private void setBoundsInPx(Activity activity, DisplayAndroid display, Rect boundsInPx) {
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

    private void showInternal(Activity activity, ActivityWindowAndroid activityWindowAndroid) {
        // No-op if already active.
        if (isActiveInternal(activityWindowAndroid)) return;

        // Activate the Task if it's already visible.
        if (isVisibleInternal(activity)) {
            ActivityManager activityManager =
                    (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
            mPendingActionManager.requestAction(PendingAction.SHOW);
            mState = State.PENDING_UPDATE;
            activityManager.moveTaskToFront(activity.getTaskId(), 0);
        }
    }

    private void closeInternal(Activity activity) {
        activity.finishAndRemoveTask();
    }

    private void activateInternal(Activity activity) {
        ActivityManager activityManager =
                (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
        mPendingActionManager.requestAction(PendingAction.ACTIVATE);
        mState = State.PENDING_UPDATE;
        activityManager.moveTaskToFront(activity.getTaskId(), 0);
    }

    @RequiresApi(api = VERSION_CODES.R)
    private void maximizeInternal(Activity activity, ActivityWindowAndroid activityWindowAndroid) {
        if (isRestoredInternal(activity)) {
            mRestoredBoundsInPx = getCurrentBoundsInPx(activity);
        }

        if (isMinimizedInternal(activity)) {
            activateInternal(activity);
        }

        Rect maxBoundsInPx =
                ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(activity.getWindowManager());
        mPendingActionManager.requestMaximize();
        mState = State.PENDING_UPDATE;
        setBoundsInPx(activity, activityWindowAndroid.getDisplay(), maxBoundsInPx);
    }

    @RequiresApi(api = VERSION_CODES.R)
    private void minimizeInternal(Activity activity) {
        if (isMinimizedInternal(activity)) {
            return;
        }
        if (isRestoredInternal(activity)) {
            mRestoredBoundsInPx = getCurrentBoundsInPx(activity);
        }
        mPendingActionManager.requestAction(PendingAction.MINIMIZE);
        mState = State.PENDING_UPDATE;
        activity.moveTaskToBack(/* nonRoot= */ true);
    }

    @RequiresApi(api = VERSION_CODES.R)
    private void restoreInternal(Activity activity, ActivityWindowAndroid activityWindowAndroid) {
        if (mRestoredBoundsInPx == null) return;
        var restoredBoundsInDp =
                convertBoundsInPxToDp(mRestoredBoundsInPx, activityWindowAndroid.getDisplay());
        var futureBounds = mPendingActionManager.getFutureBoundsInDp();
        if (restoredBoundsInDp.equals(futureBounds)) return;

        if (isMinimizedInternal(activity)) {
            activateInternal(activity);
        }

        mPendingActionManager.requestRestore(
                convertBoundsInPxToDp(mRestoredBoundsInPx, activityWindowAndroid.getDisplay()));
        mState = State.PENDING_UPDATE;
        setBoundsInPx(activity, activityWindowAndroid.getDisplay(), mRestoredBoundsInPx);
    }

    private void setBoundsInDpInternal(
            Activity activity, ActivityWindowAndroid activityWindowAndroid, Rect boundsInDp) {
        Rect boundsInPx =
                DisplayUtil.scaleToEnclosingRect(
                        boundsInDp, activityWindowAndroid.getDisplay().getDipScale());
        Rect adjustedBoundsInPx =
                ChromeAndroidTaskBoundsConstraints.apply(
                        boundsInPx,
                        activityWindowAndroid.getDisplay(),
                        activity.getWindowManager());
        setBoundsInPx(activity, activityWindowAndroid.getDisplay(), adjustedBoundsInPx);
    }

    private void maybeSetStateIdle(int[] actions) {
        for (int action : actions) {
            if (action != PendingAction.NONE) {
                return;
            }
        }
        mState = State.IDLE;
    }

    @VisibleForTesting
    static Rect convertBoundsInPxToDp(Rect boundsInPx, DisplayAndroid displayAndroid) {
        return DisplayUtil.scaleToEnclosingRect(boundsInPx, 1.0f / displayAndroid.getDipScale());
    }

    @Nullable Rect getRestoredBoundsInPxForTesting() {
        ThreadUtils.assertOnUiThread();
        return mRestoredBoundsInPx;
    }

    PendingActionManager getPendingActionManagerForTesting() {
        return mPendingActionManager;
    }
}
