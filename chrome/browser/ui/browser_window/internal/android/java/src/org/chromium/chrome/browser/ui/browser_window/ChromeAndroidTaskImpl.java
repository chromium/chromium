// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.ArrayMap;
import android.view.ViewTreeObserver;

import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.TaskVisibilityListener;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.browser_window.PendingActionManager.PendingAction;
import org.chromium.chrome.browser.ui.browser_window.WindowStateManager.WindowState;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.insets.InsetObserver.WindowInsetsAnimationListener;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.function.Supplier;

/** Implements {@link ChromeAndroidTask}. */
@NullMarked
final class ChromeAndroidTaskImpl
        implements ChromeAndroidTask,
                ConfigurationChangedObserver,
                TopResumedActivityChangedWithNativeObserver,
                TaskVisibilityListener,
                ViewTreeObserver.OnGlobalLayoutListener,
                ActivityStateObserver {

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

        /**
         * The Task has a state that's being updated but not finished yet.
         *
         * <p>This state should only be set when all preconditions for the state update have been
         * met and we are confident that the Android OS will successfully complete the update.
         *
         * <p>Otherwise:
         *
         * <ul>
         *   <li>If the Android framework doesn't provide an API to listen for failures, the {@link
         *       ChromeAndroidTask} will be stuck in {@code PENDING_UPDATE}.
         *   <li>The predicted future state {@link ChromeAndroidTask} provides during {@code
         *       PENDING_UPDATE} will be wrong.
         * </ul>
         */
        int PENDING_UPDATE = 2;

        /** The Task is alive without any pending state change. */
        int IDLE = 3;

        /** The Task is being destroyed, but the destruction hasn't been completed. */
        int DESTROYING = 4;

        /** The Task has been destroyed. */
        int DESTROYED = 5;
    }

    /**
     * Contains objects whose lifecycle is in sync with the top {@code Activity} tracked by this
     * {@link ChromeAndroidTask}.
     *
     * <p>An instance of this class should always be derived from the top {@link
     * ActivityScopedObjects} in {@link #mActivityScopedObjectsDeque} using {@link #obtain}.
     *
     * <p>The differences between this class and {@link ActivityScopedObjects} are:
     *
     * <ul>
     *   <li>{@link ActivityScopedObjects} is part of {@link ChromeAndroidTask}'s public APIs, where
     *       its {@code Activity} can be null as it's held as a weak reference in {@link
     *       ActivityWindowAndroid}.
     *   <li>This class is for {@link ChromeAndroidTaskImpl}'s internal logic. A non-null instance
     *       guarantees a non-null {@code Activity}.
     * </ul>
     */
    private static final class TopActivityScopedObjects {
        final Activity mActivity;
        final ActivityWindowAndroid mActivityWindowAndroid;
        final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;

        static @Nullable TopActivityScopedObjects obtain(ChromeAndroidTaskImpl chromeAndroidTask) {
            var activityScopedObjects = chromeAndroidTask.mActivityScopedObjectsDeque.peekFirst();
            var activityWindowAndroid =
                    activityScopedObjects == null
                            ? null
                            : activityScopedObjects.mActivityWindowAndroid;
            var activity =
                    activityWindowAndroid == null
                            ? null
                            : activityWindowAndroid.getActivity().get();
            return activityWindowAndroid == null || activity == null
                    ? null
                    : new TopActivityScopedObjects(
                            activity,
                            activityWindowAndroid,
                            activityScopedObjects.mDesktopWindowStateManager);
        }

        private TopActivityScopedObjects(
                Activity activity,
                ActivityWindowAndroid activityWindowAndroid,
                @Nullable DesktopWindowStateManager desktopWindowStateManager) {
            mActivity = activity;
            mActivityWindowAndroid = activityWindowAndroid;
            mDesktopWindowStateManager = desktopWindowStateManager;
        }
    }

    /** Interface for logic reading a value from {@link TopActivityScopedObjects}. */
    private interface ActivityReader<T> {
        T read(TopActivityScopedObjects activityScopedObjects);
    }

    /** Interface for logic updating an {@link Activity}. */
    private interface ActivityUpdater {
        void update(TopActivityScopedObjects activityScopedObjects);
    }

    private final PendingActionManager mPendingActionManager = new PendingActionManager();

    private final @BrowserWindowType int mBrowserWindowType;

    // TODO(crbug.com/475200706): Consider removing this field and just relying on the
    // TabModelSelector to determine the profile.
    private final Profile mInitialProfile;

    /**
     * Each {@link AndroidBrowserWindow} is associated with a {@link Profile}. A task tracks the
     * state of a particular OS level window, which may contain multiple virtual {@link
     * AndroidBrowserWindow}s if the current activity supports multiple profiles i.e. {@code
     * mSupportedProfileType} is {@link SupportedProfileType#MIXED}.
     */
    private final Map<Profile, AndroidBrowserWindow> mAndroidBrowserWindows = new ArrayMap<>();

    private final ObserverList<AndroidBrowserWindowObserver> mAndroidBrowserWindowObservers =
            new ObserverList<>();

    private final WindowStateManager mWindowStateManager = new WindowStateManager();

    /**
     * Contains all {@link ChromeAndroidTaskFeature}s associated with this {@link
     * ChromeAndroidTask}.
     */
    private final Map<ChromeAndroidTaskFeatureKey, ChromeAndroidTaskFeature> mFeatures =
            new ArrayMap<>();

    /**
     * All {@link ActivityScopedObjects} instances associated with this Task.
     *
     * <p>As a {@link ChromeAndroidTask} is meant to track an Android Task, but {@link
     * ActivityScopedObjects} is associated with a {@code ChromeActivity}, {@link
     * ActivityScopedObjects} should be added/removed per the {@code ChromeActivity} lifecycle.
     *
     * @see #addActivityScopedObjects
     * @see #removeActivityScopedObjects
     */
    private final Deque<ActivityScopedObjects> mActivityScopedObjectsDeque = new ArrayDeque<>();

    /**
     * Observer for profile removal. This is attached to the {@link ProfileManager} in the
     * constructor and is removed when the {@link ChromeAndroidTask} is destroyed.
     */
    private final ProfileManager.Observer mProfileObserver =
            new ProfileManager.Observer() {
                @Override
                public void onProfileAdded(Profile profile) {}

                @Override
                public void onProfileDestroyed(Profile profile) {
                    var iterator = mFeatures.entrySet().iterator();
                    while (iterator.hasNext()) {
                        var entry = iterator.next();
                        var key = entry.getKey();
                        if (profile.equals(key.mProfile)) {
                            entry.getValue().onFeatureRemoved();
                            iterator.remove();
                        }
                    }

                    // TODO(crbug.com/479566813): Several objects for desktop Android related to
                    // extensions do not handle the BrowserWindow destruction happening when the
                    // profile is destroyed. This should be fixed. For now we can just defer the
                    // destruction until the activity is destroyed since there should never be more
                    // than one profile/window on desktop Android.
                    if (!BuildConfig.IS_DESKTOP_ANDROID) {
                        var browserWindow = mAndroidBrowserWindows.remove(profile);
                        if (browserWindow != null) {
                            destroyBrowserWindow(profile, browserWindow);
                        }
                    }
                }
            };

    private final Callback<TabModel> mOnTabModelSelectedCallback = this::onTabModelSelected;

    private final IncognitoTabModelObserver mIncognitoTabModelObserver =
            new IncognitoTabModelObserver() {
                @Override
                public void onIncognitoModelCreated() {
                    var activityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
                    assert activityScopedObjects != null
                            : "ActivityScopedObjects should not be null if the"
                                    + " mIncognitoTabModelObserver is registered.";
                    var tabModelSelector = activityScopedObjects.mTabModelSelector;
                    var incognitoModel = tabModelSelector.getModel(/* incognito= */ true);

                    var incognitoProfile = incognitoModel.getProfile();
                    assert incognitoProfile != null : "Incognito profile should not be null.";
                    assert !mAndroidBrowserWindows.containsKey(incognitoProfile)
                            : "AndroidBrowserWindow should not be associated with the incognito"
                                    + " profile yet.";

                    associateTabModelWithBrowserWindow(incognitoModel);
                }
            };

    private @Nullable Integer mId;
    private long mLastActivatedTimeMillis;
    private @Nullable PendingTaskInfo mPendingTaskInfo;

    /** Last Task (window) bounds updated by {@link #onConfigurationChanged(Configuration)}. */
    private @Nullable Rect mLastBoundsInDpOnConfigChanged;

    private @State int mState;

    /**
     * Whether this Task has seen its top Activity becomes the top-resumed Activity for the first
     * time.
     *
     * <p>This is set by {@link ActivityStateObserver#onActivityTopResumedChanged}, i.e., it doesn't
     * indicate whether native initialization is completed.
     */
    private boolean mReceivedFirstTopResumedActivity;

    /**
     * Whether this Task has seen its top Activity becomes the top-resumed Activity for the first
     * time after native initialization is completed.
     *
     * <p>This is set by {@link TopResumedActivityChangedWithNativeObserver}, i.e., it may or may
     * not become true before {@link #mReceivedFirstTopResumedActivity}.
     */
    private boolean mReceivedFirstTopResumedActivityWithNative;

    /**
     * Listener for window insets animation.
     *
     * <p>This listener is used to detect when the window insets animation ends and the window
     * bounds change.
     */
    @RequiresApi(VERSION_CODES.R)
    private final WindowInsetsAnimationListener mWindowInsetsAnimationListener =
            new WindowInsetsAnimationListener() {

                @Override
                public void onPrepare(WindowInsetsAnimationCompat animation) {}

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
                            topActivityScopedObjects ->
                                    mWindowStateManager.update(topActivityScopedObjects.mActivity));
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

    /**
     * Returns true if the Task (window) bounds for the top {@code Activity} can be changed.
     *
     * <p>This method checks all preconditions on changing Task bounds. It should be called before
     * {@link #mState} is set to {@link State#PENDING_UPDATE}.
     */
    private static boolean canSetBounds(TopActivityScopedObjects topActivityScopedObjects) {
        // The Android API to change window bounds is available on BAKLAVA+.
        if (Build.VERSION.SDK_INT < VERSION_CODES.BAKLAVA) {
            Log.w(TAG, "Unable to set bounds: unsupported API level");
            return false;
        }

        // For the window bounds to be changed, the app must hold the browser role.
        var roleManager = ContextUtils.getApplicationContext().getSystemService(RoleManager.class);
        if (!roleManager.isRoleHeld(RoleManager.ROLE_BROWSER)) {
            Log.w(TAG, "Unable to set bounds: the app doesn't hold the browser role");
            return false;
        }

        // Only free-form windows can change bounds.
        if (!AppHeaderUtils.isAppInDesktopWindow(
                topActivityScopedObjects.mDesktopWindowStateManager)) {
            Log.w(TAG, "Unable to set bounds: the app isn't in desktop windowing mode");
            return false;
        }

        // The Android API to change window bounds is accessed via AppTask, so AppTask must be
        // non-null.
        // AppTask can be null when ChromeAndroidTask is for a CCT window. Please see
        // http://crbug.com/468113288 for details.
        var activity = topActivityScopedObjects.mActivity;
        var appTask = AndroidTaskUtils.getAppTaskFromId(activity, activity.getTaskId());
        if (appTask == null) {
            Log.w(TAG, "Unable to set bounds: null AppTask");
            return false;
        }

        // Chrome wraps the Android API in AconfigFlaggedApiDelegate, so AconfigFlaggedApiDelegate
        // must be non-null.
        var aconfigFlaggedApiDelegate = AconfigFlaggedApiDelegate.getInstance();
        if (aconfigFlaggedApiDelegate == null) {
            Log.w(TAG, "Unable to set bounds: null AconfigFlaggedApiDelegate");
            return false;
        }

        return true;
    }

    ChromeAndroidTaskImpl(
            @BrowserWindowType int browserWindowType, ActivityScopedObjects activityScopedObjects) {
        mBrowserWindowType = browserWindowType;
        mId = getActivity(activityScopedObjects.mActivityWindowAndroid).getTaskId();

        Profile initialProfile =
                activityScopedObjects.mTabModelSelector.getCurrentModel().getProfile();
        assert initialProfile != null
                : "ChromeAndroidTask must be initialized with a non-null profile";
        mInitialProfile = initialProfile;

        // The AndroidBrowserWindowObserver list will be empty at this point, so it's safe to not
        // notify the observers.
        mAndroidBrowserWindows.put(
                mInitialProfile,
                new AndroidBrowserWindow(/* chromeAndroidTask= */ this, mInitialProfile));
        ProfileManager.addObserver(mProfileObserver);

        mState = State.IDLE;
        addActivityScopedObjectsInternal(activityScopedObjects);
    }

    ChromeAndroidTaskImpl(PendingTaskInfo pendingTaskInfo) {
        mPendingTaskInfo = pendingTaskInfo;

        mBrowserWindowType = pendingTaskInfo.mCreateParams.getWindowType();
        mInitialProfile = pendingTaskInfo.mCreateParams.getProfile();
        assert mInitialProfile != null
                : "PendingTaskInfo must be initialized with a non-null profile";

        // The AndroidBrowserWindowObserver list will be empty at this point, so it's safe to not
        // notify the observers.
        mAndroidBrowserWindows.put(
                mInitialProfile,
                new AndroidBrowserWindow(/* chromeAndroidTask= */ this, mInitialProfile));

        ProfileManager.addObserver(mProfileObserver);

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
    public void addActivityScopedObjects(ActivityScopedObjects activityScopedObjects) {
        ThreadUtils.assertOnUiThread();
        addActivityScopedObjectsInternal(activityScopedObjects);
    }

    private void completePendingCreate() {
        var topActivityScopedObjects = TopActivityScopedObjects.obtain(this);
        if (mPendingTaskInfo == null || topActivityScopedObjects == null) {
            return;
        }

        // Transition from PENDING_CREATE to IDLE.
        assert mState == State.PENDING_CREATE;
        assert mId == null;

        mWindowStateManager.update(topActivityScopedObjects.mActivity);
        mId = topActivityScopedObjects.mActivity.getTaskId();
        @Nullable Rect futureBounds = mPendingActionManager.getFutureBoundsInDp();
        @Nullable Rect futureRestoredBounds = mPendingActionManager.getFutureRestoredBoundsInDp();
        mState = State.IDLE;
        dispatchPendingActions(topActivityScopedObjects, futureBounds, futureRestoredBounds);

        JniOnceCallback<Long> taskCreationCallbackForNative =
                mPendingTaskInfo.mTaskCreationCallbackForNative;
        if (taskCreationCallbackForNative != null) {
            var browserWindow = mAndroidBrowserWindows.get(mInitialProfile);
            assert browserWindow != null;
            taskCreationCallbackForNative.onResult(browserWindow.getOrCreateNativePtr());
        }
        mPendingTaskInfo = null;
    }

    @Override
    public @Nullable ActivityWindowAndroid getTopActivityWindowAndroid() {
        ThreadUtils.assertOnUiThread();
        assertAlive();
        var topActivityScopedObjects = TopActivityScopedObjects.obtain(this);
        return topActivityScopedObjects == null
                ? null
                : topActivityScopedObjects.mActivityWindowAndroid;
    }

    @Override
    public void removeActivityScopedObjects(ActivityWindowAndroid activityWindowAndroid) {
        ThreadUtils.assertOnUiThread();

        // (1) Check whether the Activity to remove is the top Activity.
        var topActivityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
        if (topActivityScopedObjects == null) {
            return;
        }
        boolean isActivityToRemoveAtTop =
                (activityWindowAndroid == topActivityScopedObjects.mActivityWindowAndroid);

        // (2) If the Activity to remove is the top Activity, unregister its listeners first.
        if (isActivityToRemoveAtTop) {
            unregisterListenersForTopActivity();
        }

        // (3) Remove the ActivityScopedObjects.
        removeActivityScopedObjectsInternal(activityWindowAndroid);

        // (4) If we removed the top ActivityScopedObjects, register listeners for the new top
        // Activity.
        if (isActivityToRemoveAtTop) {
            registerListenersForTopActivity();
        }
    }

    @Override
    public <T extends ChromeAndroidTaskFeature> void addFeature(
            ChromeAndroidTaskFeatureKey featureKey, Supplier<@Nullable T> featureSupplier) {
        ThreadUtils.assertOnUiThread();
        assertPendingCreateOrIdle();

        if (mFeatures.containsKey(featureKey)) {
            return;
        }

        var topActivityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
        var tabModelSelector =
                topActivityScopedObjects == null
                        ? null
                        : topActivityScopedObjects.mTabModelSelector;

        var feature = featureSupplier.get();
        if (feature != null) {
            mFeatures.put(featureKey, feature);
            feature.onAddedToTask();
            if (tabModelSelector != null) {
                feature.onTabModelSelected(tabModelSelector.getCurrentModel());
            }
        }
    }

    @Override
    public @Nullable Intent createIntentForNormalBrowserWindow(boolean isIncognito) {
        ThreadUtils.assertOnUiThread();
        var topActivityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
        if (topActivityScopedObjects == null) {
            return null;
        }

        var multiInstanceManager = topActivityScopedObjects.mMultiInstanceManager;
        if (multiInstanceManager == null) {
            return null;
        }

        return multiInstanceManager.createNewWindowIntent(
                isIncognito, NewWindowAppSource.BROWSER_WINDOW_CREATOR);
    }

    @Override
    public long getOrCreateNativeBrowserWindowPtr(Profile profile) {
        ThreadUtils.assertOnUiThread();
        assert mState == State.PENDING_CREATE
                        || mState == State.IDLE
                        || mState == State.PENDING_UPDATE
                : "This Task is not pending or alive.";
        var browserWindow = mAndroidBrowserWindows.get(profile);
        assert browserWindow != null : "Profile not found in AndroidBrowserWindows map.";
        return browserWindow.getOrCreateNativePtr();
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
        // the Feature could call the Task's APIs during Feature#onFeatureRemoved(). Since mState
        // won't become "DESTROYED" until after Feature#onFeatureRemoved(), we need the "DESTROYING"
        // state to prevent the Feature from accessing the Task's APIs that should only be called
        // when mState is "ALIVE".
        mState = State.DESTROYING;

        if (mPendingTaskInfo != null) {
            mPendingTaskInfo.destroy();
            mPendingTaskInfo = null;
        }

        removeAllActivityScopedObjects();
        destroyFeatures();
        ProfileManager.removeObserver(mProfileObserver);

        for (var profileAndbrowserWindow : mAndroidBrowserWindows.entrySet()) {
            destroyBrowserWindow(
                    profileAndbrowserWindow.getKey(), profileAndbrowserWindow.getValue());
        }
        mAndroidBrowserWindows.clear();
        mState = State.DESTROYED;
    }

    private void destroyBrowserWindow(Profile profile, AndroidBrowserWindow browserWindow) {
        long ptr = browserWindow.getOrCreateNativePtr();
        for (var observer : mAndroidBrowserWindowObservers) {
            observer.onBrowserWindowRemoved(ptr);
        }
        var activityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
        if (activityScopedObjects != null) {
            activityScopedObjects
                    .mTabModelSelector
                    .getModel(profile.isOffTheRecord())
                    .dissociateWithBrowserWindow();
        }
        browserWindow.destroy();
    }

    @Override
    public void onGlobalLayout() {
        useActivity(
                topActivityScopedObjects ->
                        mWindowStateManager.update(topActivityScopedObjects.mActivity));
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

        return useActivity(ChromeAndroidTaskImpl::isActiveInternal, /* defaultValue= */ false);
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

        return isMaximizedInternal();
    }

    @Override
    public boolean isMinimized() {
        ThreadUtils.assertOnUiThread();
        @Nullable Boolean isVisibleFuture = mPendingActionManager.isVisibleFuture(mState);
        if (isVisibleFuture != null) {
            return !isVisibleFuture;
        }
        return isMinimizedInternal();
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

        return isFullscreenInternal();
    }

    @Override
    public Rect getRestoredBoundsInDp() {
        ThreadUtils.assertOnUiThread();
        Rect futureRestoredBounds = mPendingActionManager.getFutureRestoredBoundsInDp();
        if (futureRestoredBounds != null) {
            return futureRestoredBounds;
        }

        return useActivity(
                topActivityScopedObjects -> {
                    Rect restoredBoundsInPx = mWindowStateManager.getRestoredRectInPx();
                    if (restoredBoundsInPx == null) {
                        restoredBoundsInPx = getCurrentBoundsInPx(topActivityScopedObjects);
                    }

                    float dipScale =
                            topActivityScopedObjects
                                    .mActivityWindowAndroid
                                    .getDisplay()
                                    .getDipScale();
                    return DisplayUtil.scaleToEnclosingRect(restoredBoundsInPx, 1.0f / dipScale);
                },
                /* defaultValue= */ new Rect());
    }

    private void setLastActivatedTimeMillis() {
        mLastActivatedTimeMillis = TimeUtils.elapsedRealtimeMillis();
    }

    @Override
    public long getLastActivatedTimeMillis() {
        ThreadUtils.assertOnUiThread();
        return mLastActivatedTimeMillis;
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

        return useActivity(unused -> !isMinimizedInternal(), /* defaultValue= */ false);
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
                topActivityScopedObjects -> {
                    if (!AppHeaderUtils.isAppInDesktopWindow(
                                    topActivityScopedObjects.mDesktopWindowStateManager)
                            || !isActiveInternal(topActivityScopedObjects)) {
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
                topActivityScopedObjects -> {
                    var newBoundsInDp = getCurrentBoundsInDp(topActivityScopedObjects);
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

        useActivity(this::closeInternal);
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
                topActivityScopedObjects -> {
                    if (!isActiveInternal(topActivityScopedObjects)) {
                        activateInternal(topActivityScopedObjects);
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
                topActivityScopedObjects -> {
                    if (!AppHeaderUtils.isAppInDesktopWindow(
                                    topActivityScopedObjects.mDesktopWindowStateManager)
                            || !isActiveInternal(topActivityScopedObjects)) {
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

        useActivity(this::maximizeInternal);
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

        useActivity(this::minimizeInternal);
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

        useActivity(this::restoreInternal);
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
                topActivityScopedObjects ->
                        setBoundsInDpInternal(topActivityScopedObjects, boundsInDp));
    }

    @Override
    public void onActivityTopResumedChanged(boolean isTopResumedActivity) {
        if (isTopResumedActivity && !mReceivedFirstTopResumedActivity) {
            mReceivedFirstTopResumedActivity = true;
        }

        if (mReceivedFirstTopResumedActivity && mReceivedFirstTopResumedActivityWithNative) {
            completePendingCreate();
        }
    }

    @Override
    public void onTopResumedActivityChangedWithNative(boolean isTopResumedActivity) {
        ThreadUtils.assertOnUiThread();
        if (isTopResumedActivity) {
            setLastActivatedTimeMillis();
        }

        if (isTopResumedActivity && !mReceivedFirstTopResumedActivityWithNative) {
            mReceivedFirstTopResumedActivityWithNative = true;
        }

        if (mReceivedFirstTopResumedActivity && mReceivedFirstTopResumedActivityWithNative) {
            completePendingCreate();
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
        useActivity(
                topActivityScopedObjects ->
                        mWindowStateManager.update(topActivityScopedObjects.mActivity));
        if (mId == null || taskId != mId || mState != State.PENDING_UPDATE) return;
        if (!isVisible) {
            @PendingAction
            int[] actions =
                    mPendingActionManager.getAndClearTargetPendingActions(PendingAction.MINIMIZE);
            maybeSetStateIdle(actions);
        }
    }

    List<ActivityScopedObjects> getActivityScopedObjectsListForTesting() {
        ThreadUtils.assertOnUiThread();
        return new ArrayList<>(mActivityScopedObjectsDeque);
    }

    @Override
    public @Nullable ChromeAndroidTaskFeature getFeatureForTesting(
            ChromeAndroidTaskFeatureKey featureKey) {
        ThreadUtils.assertOnUiThread();
        return mFeatures.get(featureKey);
    }

    @Override
    public List<ChromeAndroidTaskFeature> getAllFeaturesForTesting() {
        ThreadUtils.assertOnUiThread();
        return new ArrayList<>(mFeatures.values());
    }

    @Override
    public @Nullable Integer getSessionIdForTesting(Profile profile) {
        var browserWindow = mAndroidBrowserWindows.get(profile);
        return browserWindow == null ? null : browserWindow.getNativeSessionIdForTesting();
    }

    @Override
    public List<Long> getAllNativeBrowserWindowPtrs() {
        ThreadUtils.assertOnUiThread();
        List<Long> allNativeBrowserWindowPtrs = new ArrayList<>();
        for (var browserWindow : mAndroidBrowserWindows.values()) {
            allNativeBrowserWindowPtrs.add(browserWindow.getOrCreateNativePtr());
        }
        return allNativeBrowserWindowPtrs;
    }

    @Override
    public void addAndroidBrowserWindowObserver(AndroidBrowserWindowObserver observer) {
        mAndroidBrowserWindowObservers.addObserver(observer);
    }

    @Override
    public void removeAndroidBrowserWindowObserver(AndroidBrowserWindowObserver observer) {
        mAndroidBrowserWindowObservers.removeObserver(observer);
    }

    @Override
    public boolean hasAndroidBrowserWindowObserver(AndroidBrowserWindowObserver observer) {
        return mAndroidBrowserWindowObservers.hasObserver(observer);
    }

    @VisibleForTesting
    @State
    int getState() {
        return mState;
    }

    private void addActivityScopedObjectsInternal(ActivityScopedObjects activityScopedObjects) {
        assertPendingCreateOrIdle();

        // Unregister all listeners for the current top Activity.
        unregisterListenersForTopActivity();

        // If the ActivityScopedObjects to be added already exists, remove it first.
        removeActivityScopedObjectsInternal(activityScopedObjects.mActivityWindowAndroid);

        var activityWindowAndroid = activityScopedObjects.mActivityWindowAndroid;
        if (mState == State.IDLE) {
            assert mId != null;
            assert mId == getActivity(activityWindowAndroid).getTaskId()
                    : "The new ActivityWindowAndroid doesn't belong to this Task.";
        } else {
            assert mId == null;
        }

        // Add the ActivityScopedObjects and register listeners.
        mActivityScopedObjectsDeque.addFirst(activityScopedObjects);
        registerListenersForTopActivity();

        // Cache the maximize bound.
        if (VERSION.SDK_INT >= VERSION_CODES.R) {
            var activity = getActivity(activityWindowAndroid);
            var maximizedBounds =
                    convertBoundsInPxToDp(
                            ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(
                                    activity.getWindowManager()),
                            activityWindowAndroid.getDisplay());
            PendingActionManager.setMaximumBounds(maximizedBounds);
        }
    }

    private void registerListenersForTopActivity() {
        var topActivityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
        if (topActivityScopedObjects == null) {
            return;
        }

        var topActivityWindowAndroid = topActivityScopedObjects.mActivityWindowAndroid;

        // Register Activity LifecycleObservers
        var lifecycleDispatcher = getActivityLifecycleDispatcher(topActivityWindowAndroid);
        lifecycleDispatcher.register(this);
        if (lifecycleDispatcher.getCurrentActivityState() == ActivityState.RESUMED_WITH_NATIVE) {
            mReceivedFirstTopResumedActivityWithNative = true;
        }
        if (topActivityWindowAndroid.isTopResumedActivity()) {
            mReceivedFirstTopResumedActivity = true;
        }
        if (mReceivedFirstTopResumedActivity && mReceivedFirstTopResumedActivityWithNative) {
            completePendingCreate();
        }
        topActivityWindowAndroid.addActivityStateObserver(this);

        // Register Task VisibilityListener
        ApplicationStatus.registerTaskVisibilityListener(this);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && topActivityWindowAndroid.getInsetObserver() != null) {
            topActivityWindowAndroid
                    .getInsetObserver()
                    .addWindowInsetsAnimationListener(mWindowInsetsAnimationListener);
        }

        TabModelSelector tabModelSelector = topActivityScopedObjects.mTabModelSelector;
        if (topActivityScopedObjects.mSupportedProfileType == SupportedProfileType.MIXED) {
            // Associate regular model.
            var regularModel = tabModelSelector.getModel(/* incognito= */ false);
            associateTabModelWithBrowserWindow(regularModel);

            // Associate incognito model if it exists, otherwise observe.
            var incognitoModel =
                    (IncognitoTabModel) tabModelSelector.getModel(/* incognito= */ true);
            var incognitoProfile = incognitoModel.getProfile();
            if (incognitoProfile != null) {
                associateTabModelWithBrowserWindow(incognitoModel);
            }
            incognitoModel.addIncognitoObserver(mIncognitoTabModelObserver);
        } else {
            associateTabModelWithBrowserWindow(tabModelSelector.getCurrentModel());
        }

        tabModelSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndPostIfNonNull(mOnTabModelSelectedCallback);
        onTabModelSelected(tabModelSelector.getCurrentModel());

        getActivity(topActivityWindowAndroid)
                .findViewById(android.R.id.content)
                .getViewTreeObserver()
                .addOnGlobalLayoutListener(this);

        mWindowStateManager.update(getActivity(topActivityWindowAndroid));
    }

    /**
     * Associates the given {@link TabModel} with the {@link AndroidBrowserWindow} for its {@link
     * Profile} creating the browser window if it does not exist. *
     *
     * @param tabModel The {@link TabModel} to associate.
     */
    private void associateTabModelWithBrowserWindow(TabModel tabModel) {
        var profile = tabModel.getProfile();
        assert profile != null;
        var browserWindow = mAndroidBrowserWindows.get(profile);
        if (browserWindow == null) {
            browserWindow = new AndroidBrowserWindow(this, profile);
            mAndroidBrowserWindows.put(profile, browserWindow);
            long ptr = browserWindow.getOrCreateNativePtr();
            for (var observer : mAndroidBrowserWindowObservers) {
                observer.onBrowserWindowAdded(ptr);
            }
        }
        tabModel.associateWithBrowserWindow(browserWindow.getOrCreateNativePtr());
    }

    private void unregisterListenersForTopActivity() {
        var topActivityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
        if (topActivityScopedObjects == null) {
            return;
        }

        var topActivityWindowAndroid = topActivityScopedObjects.mActivityWindowAndroid;

        // Unregister Activity LifecycleObservers.
        getActivityLifecycleDispatcher(topActivityWindowAndroid).unregister(this);
        topActivityWindowAndroid.removeActivityStateObserver(this);

        // Unregister Task VisibilityListener.
        ApplicationStatus.unregisterTaskVisibilityListener(this);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && topActivityWindowAndroid.getInsetObserver() != null) {
            // Unregister WindowInsetsAnimationListener.
            topActivityWindowAndroid
                    .getInsetObserver()
                    .removeWindowInsetsAnimationListener(mWindowInsetsAnimationListener);
        }

        var tabModelSelector = topActivityScopedObjects.mTabModelSelector;
        if (tabModelSelector != null) {
            if (topActivityScopedObjects.mSupportedProfileType == SupportedProfileType.MIXED) {
                var incognitoTabModel =
                        (IncognitoTabModel) tabModelSelector.getModel(/* incognito= */ true);
                incognitoTabModel.removeIncognitoObserver(mIncognitoTabModelObserver);
            }
            tabModelSelector
                    .getCurrentTabModelSupplier()
                    .removeObserver(mOnTabModelSelectedCallback);

            for (var tabModel : tabModelSelector.getModels()) {
                tabModel.dissociateWithBrowserWindow();
            }
        }
        getActivity(topActivityWindowAndroid)
                .findViewById(android.R.id.content)
                .getViewTreeObserver()
                .removeOnGlobalLayoutListener(this);
    }

    /**
     * @param topActivityScopedObjects The {@link TopActivityScopedObjects} for this task.
     * @param futureBoundsInDp The future bounds the task is supposed to be when becoming alive.
     * @param futureRestoredBoundsInDp The restored bounds recorded before becoming alive.
     */
    @SuppressLint("NewApi")
    private void dispatchPendingActions(
            TopActivityScopedObjects topActivityScopedObjects,
            @Nullable Rect futureBoundsInDp,
            @Nullable Rect futureRestoredBoundsInDp) {
        // Initiate actions on a live Task.
        assertAlive();

        @PendingAction int[] pendingActions = mPendingActionManager.getAndClearPendingActions();
        for (@PendingAction int action : pendingActions) {
            if (action == PendingAction.NONE) continue;
            switch (action) {
                case PendingAction.SHOW:
                    showInternal(topActivityScopedObjects);
                    break;
                case PendingAction.SHOW_INACTIVE:
                case PendingAction.DEACTIVATE:
                    ChromeAndroidTaskTrackerImpl.getInstance().activatePenultimatelyActivatedTask();
                    break;
                case PendingAction.CLOSE:
                    closeInternal(topActivityScopedObjects);
                    break;
                case PendingAction.ACTIVATE:
                    activateInternal(topActivityScopedObjects);
                    break;
                case PendingAction.MAXIMIZE:
                    maximizeInternal(topActivityScopedObjects);
                    break;
                case PendingAction.MINIMIZE:
                    minimizeInternal(topActivityScopedObjects);
                    break;
                case PendingAction.RESTORE:
                    // RESTORE should be ignored to fall back to default startup bounds if
                    // non-empty, non-default bounds are not requested in pending state.
                    if (futureRestoredBoundsInDp != null && !futureRestoredBoundsInDp.isEmpty()) {
                        float dipScale =
                                topActivityScopedObjects
                                        .mActivityWindowAndroid
                                        .getDisplay()
                                        .getDipScale();
                        Rect restoredBoundsInPx =
                                DisplayUtil.scaleToEnclosingRect(
                                        futureRestoredBoundsInDp, dipScale);
                        restoreInternal(topActivityScopedObjects, restoredBoundsInPx);
                    }
                    break;
                case PendingAction.SET_BOUNDS:
                    assert futureBoundsInDp != null;
                    setBoundsInDpInternal(topActivityScopedObjects, futureBoundsInDp);
                    break;
                default:
                    assert false : "Unsupported pending action.";
            }
        }
    }

    private void removeActivityScopedObjectsInternal(ActivityWindowAndroid activityWindowAndroid) {
        if (mActivityScopedObjectsDeque.isEmpty()) {
            return;
        }

        ActivityScopedObjects activityScopedObjectsToRemove = null;
        for (var activityScopedObjects : mActivityScopedObjectsDeque) {
            if (activityScopedObjects.mActivityWindowAndroid == activityWindowAndroid) {
                assert activityScopedObjectsToRemove == null
                        : "the same instance of ActivityScopedObjects was added more than once";
                activityScopedObjectsToRemove = activityScopedObjects;
            }
        }

        if (activityScopedObjectsToRemove != null) {
            mActivityScopedObjectsDeque.remove(activityScopedObjectsToRemove);

            Iterator<Entry<ChromeAndroidTaskFeatureKey, ChromeAndroidTaskFeature>> iterator =
                    mFeatures.entrySet().iterator();
            while (iterator.hasNext()) {
                Entry<ChromeAndroidTaskFeatureKey, ChromeAndroidTaskFeature> entry =
                        iterator.next();
                ChromeAndroidTaskFeatureKey key = entry.getKey();
                if (activityWindowAndroid == key.mActivityWindowAndroid) {
                    entry.getValue().onFeatureRemoved();
                    iterator.remove();
                }
            }
        }
    }

    private void removeAllActivityScopedObjects() {
        unregisterListenersForTopActivity();
        mActivityScopedObjectsDeque.clear();
    }

    private void useActivity(ActivityUpdater updater) {
        var topActivityScopedObjects = TopActivityScopedObjects.obtain(this);
        if (topActivityScopedObjects != null) {
            updater.update(topActivityScopedObjects);
        }
    }

    private <T> T useActivity(ActivityReader<T> reader, T defaultValue) {
        var topActivityScopedObjects = TopActivityScopedObjects.obtain(this);
        return topActivityScopedObjects == null
                ? defaultValue
                : reader.read(topActivityScopedObjects);
    }

    private void destroyFeatures() {
        for (var feature : mFeatures.values()) {
            feature.onFeatureRemoved();
        }
        mFeatures.clear();
    }

    private Rect getCurrentBoundsInDp(TopActivityScopedObjects topActivityScopedObjects) {
        Rect boundsInPx = getCurrentBoundsInPx(topActivityScopedObjects);
        return convertBoundsInPxToDp(
                boundsInPx, topActivityScopedObjects.mActivityWindowAndroid.getDisplay());
    }

    private static Rect getCurrentBoundsInPx(TopActivityScopedObjects topActivityScopedObjects) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "getBoundsInPx() requires Android R+; returning an empty Rect()");
            return new Rect();
        }

        return topActivityScopedObjects
                .mActivity
                .getWindowManager()
                .getCurrentWindowMetrics()
                .getBounds();
    }

    private void assertAlive() {
        assert mState == State.IDLE || mState == State.PENDING_UPDATE : "This Task is not alive.";
    }

    private void assertPendingCreateOrIdle() {
        assert mState == State.IDLE || mState == State.PENDING_CREATE
                : "This Task is neither pending create nor idle.";
    }

    private static boolean isActiveInternal(TopActivityScopedObjects topActivityScopedObjects) {
        return topActivityScopedObjects.mActivityWindowAndroid.isTopResumedActivity();
    }

    @RequiresApi(api = VERSION_CODES.R)
    private boolean isMaximizedInternal() {
        return mWindowStateManager.getWindowState() == WindowState.MAXIMIZED;
    }

    private boolean isMinimizedInternal() {
        return mWindowStateManager.getWindowState() == WindowState.MINIMIZED;
    }

    @RequiresApi(api = VERSION_CODES.R)
    private boolean isFullscreenInternal() {
        return mWindowStateManager.getWindowState() == WindowState.FULLSCREEN;
    }

    private void setBoundsInPx(TopActivityScopedObjects topActivityScopedObjects, Rect boundsInPx) {
        var activity = topActivityScopedObjects.mActivity;
        int displayId = topActivityScopedObjects.mActivityWindowAndroid.getDisplay().getDisplayId();

        var aconfigFlaggedApiDelegate = AconfigFlaggedApiDelegate.getInstance();
        var appTask = AndroidTaskUtils.getAppTaskFromId(activity, activity.getTaskId());
        assert aconfigFlaggedApiDelegate != null && appTask != null
                : "use canSetBounds() to prevent null values";

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

    private void showInternal(TopActivityScopedObjects topActivityScopedObjects) {
        // No-op if already active.
        if (isActiveInternal(topActivityScopedObjects)) return;

        // Activate the Task if it's already visible.
        if (!isMinimizedInternal()) {
            var activity = topActivityScopedObjects.mActivity;
            mPendingActionManager.requestAction(PendingAction.SHOW);
            mState = State.PENDING_UPDATE;
            ApiCompatibilityUtils.moveTaskToFront(activity, activity.getTaskId(), 0);
        }
    }

    private void closeInternal(TopActivityScopedObjects topActivityScopedObjects) {
        topActivityScopedObjects.mActivity.finishAndRemoveTask();
    }

    private void activateInternal(TopActivityScopedObjects topActivityScopedObjects) {
        var activity = topActivityScopedObjects.mActivity;
        mPendingActionManager.requestAction(PendingAction.ACTIVATE);
        mState = State.PENDING_UPDATE;
        ApiCompatibilityUtils.moveTaskToFront(activity, activity.getTaskId(), 0);
    }

    @RequiresApi(api = VERSION_CODES.R)
    private void maximizeInternal(TopActivityScopedObjects topActivityScopedObjects) {
        // Precondition: the Task (window) allows bounds change.
        if (!canSetBounds(topActivityScopedObjects)) {
            return;
        }

        if (isMinimizedInternal()) {
            activateInternal(topActivityScopedObjects);
        }

        Rect maxBoundsInPx =
                ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(
                        topActivityScopedObjects.mActivity.getWindowManager());
        mPendingActionManager.requestMaximize();
        mState = State.PENDING_UPDATE;
        setBoundsInPx(topActivityScopedObjects, maxBoundsInPx);
    }

    @RequiresApi(api = VERSION_CODES.R)
    private void minimizeInternal(TopActivityScopedObjects topActivityScopedObjects) {
        if (isMinimizedInternal()) {
            return;
        }

        mPendingActionManager.requestAction(PendingAction.MINIMIZE);
        mState = State.PENDING_UPDATE;
        topActivityScopedObjects.mActivity.moveTaskToBack(/* nonRoot= */ true);
    }

    @RequiresApi(api = VERSION_CODES.R)
    private void restoreInternal(TopActivityScopedObjects topActivityScopedObjects) {
        restoreInternal(topActivityScopedObjects, mWindowStateManager.getRestoredRectInPx());
    }

    @RequiresApi(api = VERSION_CODES.R)
    private void restoreInternal(
            TopActivityScopedObjects topActivityScopedObjects, @Nullable Rect restoredBoundsInPx) {
        // Precondition 1: "restored" bounds is not null.
        if (restoredBoundsInPx == null) return;

        // Precondition 2: "restored" bounds aren't the same as "future bounds".
        // This is for the case where "restore()" is called when another bounds change is already in
        // progress.
        var display = topActivityScopedObjects.mActivityWindowAndroid.getDisplay();
        var restoredBoundsInDp = convertBoundsInPxToDp(restoredBoundsInPx, display);
        var futureBounds = mPendingActionManager.getFutureBoundsInDp();
        if (restoredBoundsInDp.equals(futureBounds)) return;

        // Precondition 3: the Task (window) allows bounds change.
        if (!canSetBounds(topActivityScopedObjects)) return;

        if (isMinimizedInternal()) {
            activateInternal(topActivityScopedObjects);
        }

        mPendingActionManager.requestRestore(convertBoundsInPxToDp(restoredBoundsInPx, display));
        mState = State.PENDING_UPDATE;
        setBoundsInPx(topActivityScopedObjects, restoredBoundsInPx);
    }

    private void setBoundsInDpInternal(
            TopActivityScopedObjects topActivityScopedObjects, Rect boundsInDp) {
        // Precondition 1: new bounds are not the same as the current bounds.
        if (getCurrentBoundsInDp(topActivityScopedObjects).equals(boundsInDp)) return;

        // Precondition 2: the Task (window) allows bounds change.
        if (!canSetBounds(topActivityScopedObjects)) return;

        mPendingActionManager.requestSetBounds(boundsInDp);
        mState = State.PENDING_UPDATE;

        var activity = topActivityScopedObjects.mActivity;
        var display = topActivityScopedObjects.mActivityWindowAndroid.getDisplay();
        Rect boundsInPx = DisplayUtil.scaleToEnclosingRect(boundsInDp, display.getDipScale());
        Rect adjustedBoundsInPx =
                ChromeAndroidTaskBoundsConstraints.apply(
                        boundsInPx, display, activity.getWindowManager());
        setBoundsInPx(topActivityScopedObjects, adjustedBoundsInPx);
    }

    private void maybeSetStateIdle(int[] actions) {
        for (int action : actions) {
            if (action != PendingAction.NONE) {
                return;
            }
        }
        mState = State.IDLE;
    }

    private void onTabModelSelected(TabModel tabModel) {
        for (var feature : mFeatures.values()) {
            feature.onTabModelSelected(tabModel);
        }
    }

    @VisibleForTesting
    static Rect convertBoundsInPxToDp(Rect boundsInPx, DisplayAndroid displayAndroid) {
        return DisplayUtil.scaleToEnclosingRect(boundsInPx, 1.0f / displayAndroid.getDipScale());
    }

    @Nullable Rect getRestoredBoundsInPxForTesting() {
        ThreadUtils.assertOnUiThread();
        return mWindowStateManager.getRestoredRectInPx();
    }

    PendingActionManager getPendingActionManagerForTesting() {
        return mPendingActionManager;
    }
}
