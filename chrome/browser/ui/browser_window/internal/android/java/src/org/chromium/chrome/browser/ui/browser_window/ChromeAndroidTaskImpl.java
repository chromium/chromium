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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.ui.browser_window.PendingActionManager.PendingAction;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.insets.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.ui.mojom.WindowShowState;

import java.lang.ref.WeakReference;
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
        PENDING,

        /* The Task is alive. */
        ALIVE,

        /** The Task is being destroyed, but the destruction hasn't been completed. */
        DESTROYING,

        /** The Task has been destroyed. */
        DESTROYED,
    }

    private final AtomicReference<State> mState = new AtomicReference<>(State.UNKNOWN);

    private final PendingActionManager mPendingActionManager = new PendingActionManager();

    private final @BrowserWindowType int mBrowserWindowType;

    private @Nullable Integer mId;
    private @Nullable Integer mPendingId;

    /**
     * Callback for native callers of {@link BrowserWindowCreatorBridge#createBrowserWindowAsync} to
     * be aware of when a native {@code AndroidBrowserWindow} is created and fully initialized.
     *
     * <p>The type of the callback is the address of the native {@code AndroidBrowserWindow}.
     */
    private @Nullable JniOnceCallback<Long> mCreationCallbackForNative;

    private final AndroidBrowserWindow mAndroidBrowserWindow;
    private final Profile mInitialProfile;
    private @Nullable TabModel mObservedTabModel;

    /**
     * Contains all {@link ChromeAndroidTaskFeature}s associated with this {@link
     * ChromeAndroidTask}.
     */
    @GuardedBy("mFeaturesLock")
    private final List<ChromeAndroidTaskFeature> mFeatures = new ArrayList<>();

    private final AtomicLong mLastActivatedTimeMillis = new AtomicLong(-1);

    private final Object mActivityWindowAndroidLock = new Object();
    private final Object mFeaturesLock = new Object();

    /**
     * The {@link ActivityWindowAndroid} in this Task.
     *
     * <p>As a {@link ChromeAndroidTask} is meant to track an Android Task, but an {@link
     * ActivityWindowAndroid} is associated with a {@code ChromeActivity}, we keep {@link
     * ActivityWindowAndroid} as a {@link WeakReference} to preempt memory leaks.
     *
     * <p>The {@link WeakReference} still needs to be explicitly cleared at the right time so {@link
     * ChromeAndroidTask} can always get the right state of {@link ActivityWindowAndroid}.
     *
     * @see #clearActivityWindowAndroid()
     */
    @GuardedBy("mActivityWindowAndroidLock")
    private WeakReference<ActivityWindowAndroid> mActivityWindowAndroid = new WeakReference<>(null);

    /** Last Task (window) bounds updated by {@link #onConfigurationChanged(Configuration)}. */
    private @Nullable Rect mLastBoundsInDpOnConfigChanged;

    /** Non-maximized bounds of the task even when currently maximized or minimized. */
    @GuardedBy("mActivityWindowAndroidLock")
    private @Nullable Rect mRestoredBoundsInPx;

    private @Nullable AndroidBrowserWindowCreateParams mCreateParams;

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
                    synchronized (mActivityWindowAndroidLock) {
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
                    synchronized (mActivityWindowAndroidLock) {
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
            @BrowserWindowType int browserWindowType,
            ActivityWindowAndroid activityWindowAndroid,
            TabModel tabModel) {
        mBrowserWindowType = browserWindowType;
        mId = getActivity(activityWindowAndroid).getTaskId();
        mPendingId = null;
        mCreationCallbackForNative = null;
        mAndroidBrowserWindow = new AndroidBrowserWindow(/* chromeAndroidTask= */ this);
        assert tabModel.getProfile() != null
                : "ChromeAndroidTask must be initialized with a non-null profile";
        mInitialProfile = tabModel.getProfile();
        mState.set(State.ALIVE);
        setActivityWindowAndroidInternal(activityWindowAndroid, tabModel);
    }

    ChromeAndroidTaskImpl(int pendingId, AndroidBrowserWindowCreateParams createParams) {
        this(pendingId, createParams, null);
    }

    ChromeAndroidTaskImpl(
            int pendingId,
            AndroidBrowserWindowCreateParams createParams,
            @Nullable JniOnceCallback<Long> callback) {
        mCreateParams = createParams;
        mBrowserWindowType = createParams.getWindowType();
        mId = null;
        mPendingId = pendingId;
        mAndroidBrowserWindow = new AndroidBrowserWindow(/* chromeAndroidTask= */ this);
        mInitialProfile = createParams.getProfile();
        mState.set(State.PENDING);
        mCreationCallbackForNative = callback;
    }

    @Override
    public @Nullable Integer getId() {
        return mId;
    }

    @Override
    public @Nullable Integer getPendingId() {
        return mPendingId;
    }

    @Override
    public @BrowserWindowType int getBrowserWindowType() {
        return mBrowserWindowType;
    }

    @Override
    public void setActivityWindowAndroid(
            ActivityWindowAndroid activityWindowAndroid, TabModel tabModel) {
        setActivityWindowAndroidInternal(activityWindowAndroid, tabModel);
    }

    @Override
    public @Nullable ActivityWindowAndroid getActivityWindowAndroid() {
        synchronized (mActivityWindowAndroidLock) {
            return getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        }
    }

    @Override
    public void clearActivityWindowAndroid() {
        clearActivityWindowAndroidInternal();
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
        assert getState() == State.PENDING || getState() == State.ALIVE
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
        if (!mState.compareAndSet(State.ALIVE, State.DESTROYING)) {
            return;
        }

        if (mCreationCallbackForNative != null) {
            mCreationCallbackForNative.destroy();
            mCreationCallbackForNative = null;
        }

        clearActivityWindowAndroidInternal();
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
        if (mState.get() == State.PENDING) {
            return mPendingActionManager.isActionRequested(PendingAction.SHOW)
                    || mPendingActionManager.isActionRequested(PendingAction.ACTIVATE);
        }

        synchronized (mActivityWindowAndroidLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null) return false;
            return activityWindowAndroid.isTopResumedActivity();
        }
    }

    @Override
    public boolean isMaximized() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "isMaximized() requires Android R+; returning a false");
            return false;
        }

        if (mState.get() == State.PENDING) {
            return mPendingActionManager.isActionRequested(PendingAction.MAXIMIZE)
                    || assumeNonNull(mCreateParams).getInitialShowState()
                            == WindowShowState.MAXIMIZED;
        }

        synchronized (mActivityWindowAndroidLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            return isMaximizedInternalLocked(activityWindowAndroid);
        }
    }

    @Override
    public boolean isMinimized() {
        if (mState.get() == State.PENDING) {
            return mPendingActionManager.isActionRequested(PendingAction.MINIMIZE)
                    || assumeNonNull(mCreateParams).getInitialShowState()
                            == WindowShowState.MINIMIZED;
        }

        synchronized (mActivityWindowAndroidLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            return isMinimizedInternalLocked(activityWindowAndroid);
        }
    }

    @Override
    public boolean isFullscreen() {
        if (mState.get() == State.PENDING) {
            return false;
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "isFullscreen() requires Android R+; returning false");
            return false;
        }

        synchronized (mActivityWindowAndroidLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            return isFullscreenInternalLocked(activityWindowAndroid);
        }
    }

    @Override
    public Rect getRestoredBoundsInDp() {
        if (mState.get() == State.PENDING) {
            var initialBounds = assumeNonNull(mCreateParams).getInitialBounds();
            if (mPendingActionManager.isActionRequested(PendingAction.SET_BOUNDS)) {
                return assertNonNull(mPendingActionManager.getPendingBoundsInDp());
            } else if (mPendingActionManager.isActionRequested(PendingAction.RESTORE)) {
                var pendingRestoredBounds = mPendingActionManager.getPendingRestoredBoundsInDp();
                return pendingRestoredBounds == null ? initialBounds : pendingRestoredBounds;
            }
            return initialBounds;
        }

        synchronized (mActivityWindowAndroidLock) {
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
        if (mState.get() == State.PENDING) {
            if (mPendingActionManager.isActionRequested(PendingAction.SET_BOUNDS)) {
                return assertNonNull(mPendingActionManager.getPendingBoundsInDp());
            }
            return assumeNonNull(mCreateParams).getInitialBounds();
        }

        synchronized (mActivityWindowAndroidLock) {
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
        if (mState.get() == State.PENDING) {
            mPendingActionManager.requestAction(PendingAction.SHOW);
            return;
        }

        synchronized (mActivityWindowAndroidLock) {
            showInternalLocked();
        }
    }

    @Override
    public boolean isVisible() {
        if (mState.get() == State.PENDING) {
            return assumeNonNull(mCreateParams).getInitialShowState() != WindowShowState.MINIMIZED;
        }

        synchronized (mActivityWindowAndroidLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null) return false;
            var activity = activityWindowAndroid.getActivity().get();
            if (activity == null) return false;
            return isVisibleInternalLocked(activity);
        }
    }

    @Override
    public void showInactive() {
        if (mState.get() == State.PENDING) {
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
            synchronized (mActivityWindowAndroidLock) {
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
        if (mState.get() == State.PENDING) {
            mPendingActionManager.requestAction(PendingAction.CLOSE);
            return;
        }

        synchronized (mActivityWindowAndroidLock) {
            closeInternalLocked();
        }
    }

    @Override
    public void activate() {
        if (mState.get() == State.PENDING) {
            mPendingActionManager.requestAction(PendingAction.ACTIVATE);
            return;
        }

        synchronized (mActivityWindowAndroidLock) {
            activateInternalLocked();
        }
    }

    @Override
    public void deactivate() {
        if (mState.get() == State.PENDING) {
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

        if (mState.get() == State.PENDING) {
            mPendingActionManager.requestAction(PendingAction.MAXIMIZE);
            return;
        }

        synchronized (mActivityWindowAndroidLock) {
            maximizeInternalLocked();
        }
    }

    @Override
    public void minimize() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "minimize() requires Android R+; does nothing");
            return;
        }

        if (mState.get() == State.PENDING) {
            mPendingActionManager.requestAction(PendingAction.MINIMIZE);
            return;
        }

        synchronized (mActivityWindowAndroidLock) {
            minimizeInternalLocked();
        }
    }

    @Override
    public void restore() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "restore() requires Android R+; does nothing");
            return;
        }
        if (mState.get() == State.PENDING) {
            mPendingActionManager.requestAction(PendingAction.RESTORE);
            return;
        }

        synchronized (mActivityWindowAndroidLock) {
            restoreInternalLocked();
        }
    }

    @Override
    public void setBoundsInDp(Rect boundsInDp) {
        if (mState.get() == State.PENDING) {
            if (!boundsInDp.isEmpty()) {
                mPendingActionManager.requestSetBounds(boundsInDp);
            }
            return;
        }

        synchronized (mActivityWindowAndroidLock) {
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

    /**
     * Same as {@link #getActivityWindowAndroid()}, but skips asserting that the {@link
     * ChromeAndroidTask} is alive.
     *
     * <p>This method should only be used in tests.
     */
    @Nullable ActivityWindowAndroid getActivityWindowAndroidForTesting() {
        synchronized (mActivityWindowAndroidLock) {
            return getActivityWindowAndroidInternalLocked(/* assertAlive= */ false);
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

    @Nullable TabModel getObservedTabModelForTesting() {
        return mObservedTabModel;
    }

    @VisibleForTesting
    State getState() {
        return assumeNonNull(mState.get());
    }

    private void setActivityWindowAndroidInternal(
            ActivityWindowAndroid activityWindowAndroid, TabModel tabModel) {
        synchronized (mActivityWindowAndroidLock) {
            assert mActivityWindowAndroid.get() == null
                    : "This Task already has an ActivityWindowAndroid.";
            switch (getState()) {
                case PENDING:
                    assert mId == null;
                    assert mPendingId != null;
                    break;
                case ALIVE:
                    assert mPendingId == null;
                    assert mId != null;
                    assert mId == getActivity(activityWindowAndroid).getTaskId()
                            : "The new ActivityWindowAndroid doesn't belong to this Task.";
                    break;
                default:
                    assert false : "Found unexpected Task state.";
            }

            mActivityWindowAndroid = new WeakReference<>(activityWindowAndroid);

            // Register Activity LifecycleObservers
            getActivityLifecycleDispatcher(activityWindowAndroid).register(this);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                    && activityWindowAndroid.getInsetObserver() != null) {
                activityWindowAndroid
                        .getInsetObserver()
                        .addWindowInsetsAnimationListener(mWindowInsetsAnimationListener);
            }
            // Update and register TabModel
            tabModel.addObserver(this);
            mObservedTabModel = tabModel;

            // Transition from PENDING to ALIVE.
            if (mState.get() == State.PENDING) {
                mId = getActivity(activityWindowAndroid).getTaskId();
                mPendingId = null;
                mState.set(State.ALIVE);
                dispatchPendingActionsLocked(activityWindowAndroid);
                if (mCreationCallbackForNative != null) {
                    mCreationCallbackForNative.onResult(getOrCreateNativeBrowserWindowPtr());
                    mCreationCallbackForNative = null;
                }
            }
        }
    }

    @GuardedBy("mActivityWindowAndroidLock")
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

    @GuardedBy("mActivityWindowAndroidLock")
    private @Nullable ActivityWindowAndroid getActivityWindowAndroidInternalLocked(
            boolean assertAlive) {
        if (assertAlive) {
            assertAlive();
        }

        return mActivityWindowAndroid.get();
    }

    private void clearActivityWindowAndroidInternal() {
        synchronized (mActivityWindowAndroidLock) {
            var activityWindowAndroid = mActivityWindowAndroid.get();
            if (activityWindowAndroid != null) {
                // Unregister Activity LifecycleObservers.
                getActivityLifecycleDispatcher(activityWindowAndroid).unregister(this);

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                        && activityWindowAndroid.getInsetObserver() != null) {
                    // Unregister WindowInsetsAnimationListener.
                    activityWindowAndroid
                            .getInsetObserver()
                            .removeWindowInsetsAnimationListener(mWindowInsetsAnimationListener);
                }
            }
            if (mObservedTabModel != null) {
                mObservedTabModel.removeObserver(this);
                mObservedTabModel = null;
            }

            mActivityWindowAndroid.clear();
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

    @GuardedBy("mActivityWindowAndroidLock")
    private Rect getCurrentBoundsInDpLocked(ActivityWindowAndroid activityWindowAndroid) {
        Rect boundsInPx = getCurrentBoundsInPxLocked(activityWindowAndroid);
        return DisplayUtil.scaleToEnclosingRect(
                boundsInPx, 1.0f / activityWindowAndroid.getDisplay().getDipScale());
    }

    @GuardedBy("mActivityWindowAndroidLock")
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
        assert mState.get() == State.ALIVE : "This Task is not alive.";
    }

    @GuardedBy("mActivityWindowAndroidLock")
    @RequiresApi(api = VERSION_CODES.R)
    private boolean isRestoredInternalLocked(ActivityWindowAndroid activityWindowAndroid) {
        return !isMinimizedInternalLocked(activityWindowAndroid)
                && !isMaximizedInternalLocked(activityWindowAndroid)
                && !isFullscreenInternalLocked(activityWindowAndroid);
    }

    @GuardedBy("mActivityWindowAndroidLock")
    private boolean isVisibleInternalLocked(@Nullable ActivityWindowAndroid activityWindowAndroid) {
        if (activityWindowAndroid == null) return false;
        return ApplicationStatus.isTaskVisible(getActivity(activityWindowAndroid).getTaskId());
    }

    @GuardedBy("mActivityWindowAndroidLock")
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

    @GuardedBy("mActivityWindowAndroidLock")
    private boolean isMinimizedInternalLocked(
            @Nullable ActivityWindowAndroid activityWindowAndroid) {
        return !isVisibleInternalLocked(activityWindowAndroid);
    }

    @GuardedBy("mActivityWindowAndroidLock")
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

    @GuardedBy("mActivityWindowAndroidLock")
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

        aconfigFlaggedApiDelegate.moveTaskTo(appTask, displayId, boundsInPx);
    }

    @GuardedBy("mActivityWindowAndroidLock")
    private boolean isVisibleInternalLocked(Activity activity) {
        return ApplicationStatus.isTaskVisible(activity.getTaskId());
    }

    @GuardedBy("mActivityWindowAndroidLock")
    private void showInternalLocked() {
        var activityWindowAndroid = getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        var activity =
                activityWindowAndroid != null ? activityWindowAndroid.getActivity().get() : null;
        if (activity == null) return;
        // Activate the Task if it's already visible.
        if (isVisibleInternalLocked(activity)) {
            ActivityManager activityManager =
                    (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
            activityManager.moveTaskToFront(activity.getTaskId(), 0);
        }
    }

    @GuardedBy("mActivityWindowAndroidLock")
    private void closeInternalLocked() {
        var activityWindowAndroid = getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        if (activityWindowAndroid == null) return;
        Activity activity = activityWindowAndroid.getActivity().get();
        if (activity == null) return;
        activity.finishAndRemoveTask();
    }

    @GuardedBy("mActivityWindowAndroidLock")
    private void activateInternalLocked() {
        var activityWindowAndroid = getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        if (activityWindowAndroid == null) return;
        Activity activity = activityWindowAndroid.getActivity().get();
        if (activity == null) return;

        ActivityManager activityManager =
                (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
        activityManager.moveTaskToFront(activity.getTaskId(), 0);
    }

    @GuardedBy("mActivityWindowAndroidLock")
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
        setBoundsInPxLocked(activity, activityWindowAndroid.getDisplay(), maxBoundsInPx);
    }

    @GuardedBy("mActivityWindowAndroidLock")
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
    @GuardedBy("mActivityWindowAndroidLock")
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

    @GuardedBy("mActivityWindowAndroidLock")
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

    @Nullable Rect getRestoredBoundsInPxForTesting() {
        synchronized (mActivityWindowAndroidLock) {
            return mRestoredBoundsInPx;
        }
    }

    PendingActionManager getPendingActionManagerForTesting() {
        return mPendingActionManager;
    }
}
