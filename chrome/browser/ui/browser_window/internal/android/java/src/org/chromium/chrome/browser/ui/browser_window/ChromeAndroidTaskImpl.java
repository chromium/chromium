// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.role.RoleManager;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.ArrayMap;
import android.view.Display;
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
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature.InitInfo;
import org.chromium.chrome.browser.ui.browser_window.PendingActionManager.PendingAction;
import org.chromium.chrome.browser.ui.browser_window.WindowStateManager.WindowState;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowResizePrecheckResult;
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
                TopResumedActivityChangedWithNativeObserver,
                TaskVisibilityListener,
                ViewTreeObserver.OnGlobalLayoutListener {

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

    /** Includes the public {@link ActivityScopedObjects} and internal Activity-scoped objects. */
    private static final class InternalActivityScopedObjects {
        final ActivityScopedObjects mActivityScopedObjects;

        /**
         * Contains {@link AndroidBrowserWindow}s for one Activity.
         *
         * <p>{@link AndroidBrowserWindow} is the Android counterpart of the native {@code
         * BrowserWindowInterface} (BWI).
         *
         * <p>BWI assumes it will only be associated with 1 profile, and a lot of native code is
         * based on this assumption. However, on Android, a {@code ChromeActivity} can have more
         * than one Profile, e.g., the {@code ChromeActivity} on mobile Android allows the user to
         * switch between a regular tab and an incognito tab without creating a new Activity/Task.
         *
         * <p>Therefore, to keep the aforementioned assumption valid, and to avoid auditing all
         * native code, one {@code ChromeActivity} is allowed to have more than one BWI, each for a
         * different Profile.
         */
        final Map<Profile, AndroidBrowserWindow> mAndroidBrowserWindows = new ArrayMap<>();
        @Nullable IncognitoTabModelObserver mIncognitoTabModelObserver;

        InternalActivityScopedObjects(
                ActivityScopedObjects activityScopedObjects, AndroidBrowserWindow browserWindow) {
            assert activityScopedObjects.mActivityWindowAndroid
                            == browserWindow.getActivityWindowAndroid()
                    : "AndroidBrowserWindow does not match ActivityScopedObjects.";
            var currentProfile =
                    activityScopedObjects.mTabModelSelector.getCurrentModel().getProfile();
            assert currentProfile == browserWindow.getProfile()
                    : "browserWindow profile does not match activityScopedObjects. Did TabModel"
                            + " change its current Profile?";

            mActivityScopedObjects = activityScopedObjects;
            mAndroidBrowserWindows.put(currentProfile, browserWindow);
        }

        AndroidBrowserWindow getBrowserWindowForCurrentProfile() {
            var profile = mActivityScopedObjects.mTabModelSelector.getCurrentModel().getProfile();
            assert profile != null
                    : "getBrowserWindowForCurrentProfile() called with a TabModel with no Profile.";
            var browserWindow = mAndroidBrowserWindows.get(profile);
            assert browserWindow != null
                    : "getBrowserWindowForCurrentProfile() called but no AndroidBrowserWindow for"
                            + " Profile!";
            return browserWindow;
        }

        void addBrowserWindow(AndroidBrowserWindow browserWindow) {
            var profile = browserWindow.getProfile();
            assert !mAndroidBrowserWindows.containsKey(profile)
                    : "Within one Activity, a Profile can only be associated with one"
                            + " AndroidBrowserWindow";
            mAndroidBrowserWindows.put(profile, browserWindow);
        }

        void addIncognitoTabModelObserver(ChromeAndroidTaskImpl chromeAndroidTaskImpl) {
            assert mIncognitoTabModelObserver == null
                    : "mIncognitoTabModelObserver is already initialized.";

            var incognitoModel =
                    (IncognitoTabModel)
                            mActivityScopedObjects.mTabModelSelector.getModel(
                                    /* incognito= */ true);
            mIncognitoTabModelObserver =
                    new IncognitoTabModelObserverImpl(chromeAndroidTaskImpl, this);
            incognitoModel.addIncognitoObserver(mIncognitoTabModelObserver);
        }

        void removeIncognitoTabModelObserver() {
            if (mIncognitoTabModelObserver != null) {
                var incognitoModel =
                        (IncognitoTabModel)
                                mActivityScopedObjects.mTabModelSelector.getModel(
                                        /* incognito= */ true);
                incognitoModel.removeIncognitoObserver(mIncognitoTabModelObserver);
                mIncognitoTabModelObserver = null;
            }
        }
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
        final @BrowserWindowType int mBrowserWindowType;

        static @Nullable TopActivityScopedObjects obtain(ChromeAndroidTaskImpl chromeAndroidTask) {
            var internalActivityScopedObjects =
                    chromeAndroidTask.mActivityScopedObjectsDeque.peekFirst();
            var activityScopedObjects =
                    internalActivityScopedObjects == null
                            ? null
                            : internalActivityScopedObjects.mActivityScopedObjects;
            var activity =
                    activityScopedObjects == null
                            ? null
                            : activityScopedObjects.mActivityWindowAndroid.getActivity().get();
            return activityScopedObjects == null || activity == null
                    ? null
                    : new TopActivityScopedObjects(
                            activity,
                            activityScopedObjects.mActivityWindowAndroid,
                            activityScopedObjects.mDesktopWindowStateManager,
                            activityScopedObjects.mBrowserWindowType);
        }

        private TopActivityScopedObjects(
                Activity activity,
                ActivityWindowAndroid activityWindowAndroid,
                @Nullable DesktopWindowStateManager desktopWindowStateManager,
                @BrowserWindowType int browserWindowType) {
            mActivity = activity;
            mActivityWindowAndroid = activityWindowAndroid;
            mDesktopWindowStateManager = desktopWindowStateManager;
            mBrowserWindowType = browserWindowType;
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

    // TODO(crbug.com/491791515): Consider removing this field and just relying on the
    // TabModelSelector to determine the profile.
    private final Profile mInitialProfile;

    private final WindowStateManager mWindowStateManager = new WindowStateManager();

    /**
     * Contains all {@link ChromeAndroidTaskFeature}s associated with this {@link
     * ChromeAndroidTask}.
     */
    private final Map<ChromeAndroidTaskFeatureKey, ChromeAndroidTaskFeature> mFeatures =
            new ArrayMap<>();

    /**
     * When the Task is PENDING, this variable is used to store the associated {@link
     * AndroidBrowserWindow}.
     */
    @Nullable private AndroidBrowserWindow mPendingBrowserWindow;

    /**
     * All {@link InternalActivityScopedObjects} instances associated with this Task.
     *
     * <p>As a {@link ChromeAndroidTask} is meant to track an Android Task, but {@link
     * InternalActivityScopedObjects} is associated with a {@code ChromeActivity}, {@link
     * InternalActivityScopedObjects} should be added/removed per the {@code ChromeActivity}
     * lifecycle.
     *
     * @see #addActivityScopedObjects
     * @see #removeActivityScopedObjects
     */
    private final Deque<InternalActivityScopedObjects> mActivityScopedObjectsDeque =
            new ArrayDeque<>();

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
                    removeAllFeaturesForProfile(profile);

                    // TODO(crbug.com/479566813): Several objects for desktop Android related to
                    // extensions do not handle the BrowserWindow destruction happening when the
                    // profile is destroyed. This should be fixed. For now we can just defer the
                    // destruction until the activity is destroyed since there should never be more
                    // than one profile/window on desktop Android.
                    if (!BuildConfig.IS_DESKTOP_ANDROID) {
                        if (mPendingBrowserWindow != null
                                && mPendingBrowserWindow.getProfile() == profile) {
                            assert mActivityScopedObjectsDeque.isEmpty();

                            destroyBrowserWindow(
                                    mPendingBrowserWindow,
                                    null,
                                    mAndroidBrowserWindowObserverNotifier);
                            mPendingBrowserWindow = null;
                            return;
                        }

                        var iterator = mActivityScopedObjectsDeque.iterator();
                        while (iterator.hasNext()) {
                            var internalActivityScopedObjects = iterator.next();
                            var browserWindow =
                                    internalActivityScopedObjects.mAndroidBrowserWindows.get(
                                            profile);
                            if (browserWindow != null) {
                                destroyBrowserWindow(
                                        browserWindow,
                                        internalActivityScopedObjects,
                                        mAndroidBrowserWindowObserverNotifier);
                            }
                        }
                        mAndroidBrowserWindowObserverNotifier.updateActiveBrowserWindow(
                                getActiveBrowserWindow());
                    }
                }
            };

    private final Callback<TabModel> mOnTabModelSelectedCallback = this::onTabModelSelected;

    private final AndroidBrowserWindowObserverNotifier mAndroidBrowserWindowObserverNotifier =
            new AndroidBrowserWindowObserverNotifier();

    private static final class IncognitoTabModelObserverImpl implements IncognitoTabModelObserver {
        private final ChromeAndroidTaskImpl mChromeAndroidTaskImpl;
        private final InternalActivityScopedObjects mInternalActivityScopedObjects;

        IncognitoTabModelObserverImpl(
                ChromeAndroidTaskImpl chromeAndroidTaskImpl,
                InternalActivityScopedObjects internalActivityScopedObjects) {
            mChromeAndroidTaskImpl = chromeAndroidTaskImpl;
            mInternalActivityScopedObjects = internalActivityScopedObjects;
        }

        @Override
        public void onIncognitoModelCreated() {
            var incognitoModel =
                    mInternalActivityScopedObjects.mActivityScopedObjects.mTabModelSelector
                            .getModel(/* incognito= */ true);

            var incognitoProfile = incognitoModel.getProfile();
            assert incognitoProfile != null : "Incognito profile should not be null.";
            assert mInternalActivityScopedObjects.mAndroidBrowserWindows.get(incognitoProfile)
                            == null
                    : "Incognito TabModel created, but its Activity already has the"
                            + " incognito Profile";

            var browserWindow =
                    new AndroidBrowserWindow(
                            mChromeAndroidTaskImpl,
                            incognitoProfile,
                            mInternalActivityScopedObjects
                                    .mActivityScopedObjects
                                    .mBrowserWindowType,
                            mInternalActivityScopedObjects
                                    .mActivityScopedObjects
                                    .mActivityWindowAndroid);
            mInternalActivityScopedObjects.addBrowserWindow(browserWindow);
            long ptr = browserWindow.getOrCreateNativePtr();
            incognitoModel.associateWithBrowserWindow(ptr);
            mChromeAndroidTaskImpl.mAndroidBrowserWindowObserverNotifier.notifyBrowserWindowAdded(
                    browserWindow);
            mChromeAndroidTaskImpl.mAndroidBrowserWindowObserverNotifier.updateActiveBrowserWindow(
                    mChromeAndroidTaskImpl.getActiveBrowserWindow());
        }

        @Override
        public void didBecomeEmpty() {
            // It is possible that the profile will start destruction shortly after this happens. In
            // this case, the ProfileObserver will also handle the BrowserWindow destruction (in
            // addition to profile scoped feature and pending activity cleanup). However, if at
            // least one other IncognitoTabModel is non-empty, the OTR profile will remain valid.
            // Properly clean up only the BrowserWindow in this case, so that if a new IncognitoTab
            // is created in this Activity, a new BrowserWindow can be created.
            var incognitoModel =
                    mInternalActivityScopedObjects.mActivityScopedObjects.mTabModelSelector
                            .getModel(/* incognito= */ true);
            mChromeAndroidTaskImpl.removeAllFeaturesForTabModel(incognitoModel);
            var incognitoProfile = incognitoModel.getProfile();
            if (incognitoProfile != null) {
                var browserWindow =
                        mInternalActivityScopedObjects.mAndroidBrowserWindows.get(incognitoProfile);
                if (browserWindow != null) {
                    destroyBrowserWindow(
                            browserWindow,
                            mInternalActivityScopedObjects,
                            mChromeAndroidTaskImpl.mAndroidBrowserWindowObserverNotifier);
                }
            }
            mChromeAndroidTaskImpl.mAndroidBrowserWindowObserverNotifier.updateActiveBrowserWindow(
                    mChromeAndroidTaskImpl.getActiveBrowserWindow());
        }
    }

    private @Nullable Integer mId;
    private long mLastActivatedTimeMillis;
    private @Nullable PendingTaskInfo mPendingTaskInfo;
    private @State int mState;

    /**
     * Whether this Task has seen its top Activity becomes the top-resumed Activity for the first
     * time after native initialization is completed.
     *
     * <p>This is set by {@link TopResumedActivityChangedWithNativeObserver}.
     */
    private boolean mIsTopActivityResumedWithNative;

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
                                    mWindowStateManager.update(
                                            topActivityScopedObjects.mActivity,
                                            topActivityScopedObjects.mActivityWindowAndroid
                                                    .getDisplay()));
                }
            };

    // TODO(https://crbug.com/518763461): remove flag once verified
    private static int getTaskId(Activity activity) {
        if (ChromeFeatureList.sTaskGetIdAnrFix.isEnabled()) {
            return ApplicationStatus.getTaskId(activity);
        } else {
            return activity.getTaskId();
        }
    }

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
     * Returns the failure reason if the Task (window) bounds for the top {@code Activity} cannot be
     * changed.
     *
     * <p>This method checks all preconditions on changing Task bounds. It should be called before
     * {@link #mState} is set to {@link State#PENDING_UPDATE}.
     */
    private static @WindowResizePrecheckResult int canResizeInternal(
            TopActivityScopedObjects topActivityScopedObjects) {
        // The Android API to change window bounds is available on BAKLAVA+.
        if (Build.VERSION.SDK_INT < VERSION_CODES.BAKLAVA) {
            Log.w(TAG, "Unable to set bounds: unsupported API level");
            return WindowResizePrecheckResult.SDK_TOO_LOW;
        }

        // For the window bounds to be changed, the app must hold the browser role.
        var roleManager = ContextUtils.getApplicationContext().getSystemService(RoleManager.class);
        if (!roleManager.isRoleHeld(RoleManager.ROLE_BROWSER)) {
            Log.w(TAG, "Unable to set bounds: the app doesn't hold the browser role");
            return WindowResizePrecheckResult.BROWSER_ROLE_NOT_HELD;
        }

        // Only free-form windows can change bounds.
        boolean isFreeformWindow;
        if (topActivityScopedObjects.mBrowserWindowType == BrowserWindowType.NORMAL) {
            isFreeformWindow =
                    AppHeaderUtils.isAppInDesktopWindow(
                            topActivityScopedObjects.mDesktopWindowStateManager);
        } else {
            // For CCT/TWA/PWA, check if the window height matches the screen height
            var activity = topActivityScopedObjects.mActivity;
            var windowManager = activity.getWindowManager();
            int windowHeight = windowManager.getCurrentWindowMetrics().getBounds().height();
            int screenHeight =
                    topActivityScopedObjects.mActivityWindowAndroid.getDisplay().getDisplayHeight();
            isFreeformWindow = windowHeight != screenHeight;
        }

        if (!isFreeformWindow) {
            Log.w(TAG, "Unable to set bounds: the app isn't in desktop windowing mode");
            return WindowResizePrecheckResult.NOT_A_FREEFORM_WINDOW;
        }

        // The Android API to change window bounds is accessed via AppTask, so AppTask must be
        // non-null.
        // AppTask can be null when ChromeAndroidTask is for a CCT window. Please see
        // http://crbug.com/468113288 for details.
        var activity = topActivityScopedObjects.mActivity;
        var appTask = AndroidTaskUtils.getAppTaskFromId(activity, getTaskId(activity));
        if (appTask == null) {
            Log.w(TAG, "Unable to set bounds: null AppTask");
            return WindowResizePrecheckResult.NULL_APP_TASK;
        }

        // Chrome wraps the Android API in AconfigFlaggedApiDelegate, so AconfigFlaggedApiDelegate
        // must be non-null.
        var aconfigFlaggedApiDelegate = AconfigFlaggedApiDelegate.getInstance();
        if (aconfigFlaggedApiDelegate == null) {
            Log.w(TAG, "Unable to set bounds: null AconfigFlaggedApiDelegate");
            return WindowResizePrecheckResult.NULL_ACONFIG_FLAGGED_API_DELEGATE;
        }

        return WindowResizePrecheckResult.OK;
    }

    ChromeAndroidTaskImpl(ActivityScopedObjects activityScopedObjects) {
        Activity activity = getActivity(activityScopedObjects.mActivityWindowAndroid);
        mId = getTaskId(activity);

        Profile initialProfile =
                activityScopedObjects.mTabModelSelector.getCurrentModel().getProfile();
        assert initialProfile != null
                : "ChromeAndroidTask must be initialized with a non-null profile";
        mInitialProfile = initialProfile;

        ProfileManager.addObserver(mProfileObserver);

        mState = State.IDLE;
        addActivityScopedObjectsInternal(activityScopedObjects);
        mWindowStateManager.update(
                activity, activityScopedObjects.mActivityWindowAndroid.getDisplay());
    }

    ChromeAndroidTaskImpl(PendingTaskInfo pendingTaskInfo) {
        mPendingTaskInfo = pendingTaskInfo;

        mInitialProfile = pendingTaskInfo.mCreateParams.getProfile();
        assert mInitialProfile != null
                : "PendingTaskInfo must be initialized with a non-null profile";

        // ActivityWindowAndroid does not exist yet, since Task is pending. So we pass a null
        // ActivityWindowAndroid.
        mPendingBrowserWindow =
                new AndroidBrowserWindow(
                        /* chromeAndroidTask= */ this,
                        mInitialProfile,
                        pendingTaskInfo.mCreateParams.getWindowType(),
                        /* activityWindowAndroid= */ null);

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

        mWindowStateManager.update(
                topActivityScopedObjects.mActivity,
                topActivityScopedObjects.mActivityWindowAndroid.getDisplay());
        mId = getTaskId(topActivityScopedObjects.mActivity);
        @Nullable Rect futureBounds = mPendingActionManager.getFutureBoundsInDp();
        @Nullable Rect futureRestoredBounds = mPendingActionManager.getFutureRestoredBoundsInDp();
        mState = State.IDLE;
        dispatchPendingActions(topActivityScopedObjects, futureBounds, futureRestoredBounds);

        JniOnceCallback<Long> taskCreationCallbackForNative =
                mPendingTaskInfo.mTaskCreationCallbackForNative;
        if (taskCreationCallbackForNative != null) {
            assert mActivityScopedObjectsDeque.size() == 1
                    : "#completePendingCreate() called in an invalid state";
            var internalActivityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
            var browserWindow = internalActivityScopedObjects.getBrowserWindowForCurrentProfile();

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
        var internalActivityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
        if (internalActivityScopedObjects == null) {
            return;
        }
        var topActivityScopedObjects = internalActivityScopedObjects.mActivityScopedObjects;

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
            mAndroidBrowserWindowObserverNotifier.updateActiveBrowserWindow(
                    getActiveBrowserWindow());
        }
    }

    @Override
    public <T extends ChromeAndroidTaskFeature> @Nullable ChromeAndroidTaskFeature addFeature(
            ChromeAndroidTaskFeatureKey featureKey, Supplier<@Nullable T> featureSupplier) {
        ThreadUtils.assertOnUiThread();
        assertPendingCreateOrIdle();

        ChromeAndroidTaskFeature feature = mFeatures.get(featureKey);
        if (feature != null) {
            return feature;
        }

        feature = featureSupplier.get();
        if (feature == null) {
            return null;
        }

        mFeatures.put(featureKey, feature);

        feature.onAddedToTask(createInitInfo(featureKey));

        // Invoke ChromeAndroidTaskFeature#onTabModelSelected() with the current TabModel.
        var internalActivityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
        var topActivityScopedObjects =
                internalActivityScopedObjects == null
                        ? null
                        : internalActivityScopedObjects.mActivityScopedObjects;
        var tabModelSelector =
                topActivityScopedObjects == null
                        ? null
                        : topActivityScopedObjects.mTabModelSelector;
        if (tabModelSelector != null) {
            feature.onTabModelSelected(tabModelSelector.getCurrentModel());
        }

        return feature;
    }

    @Override
    public void removeAllFeaturesForActivity(ActivityWindowAndroid activityWindowAndroid) {
        ThreadUtils.assertOnUiThread();
        removeAllFeaturesForActivityInternal(activityWindowAndroid);
    }

    // TODO(crbug.com/486858979): Mark this as deprecated and add Activity as a parameter.
    @Override
    public long getOrCreateNativeBrowserWindowPtr(Profile profile) {
        ThreadUtils.assertOnUiThread();
        assert mState == State.PENDING_CREATE
                        || mState == State.IDLE
                        || mState == State.PENDING_UPDATE
                : "This Task is not pending or alive.";
        var browserWindow = getTopmostWindowWithProfile(profile);
        assert browserWindow != null : "No AndroidBrowserWindow found for given Profile.";
        return browserWindow.getOrCreateNativePtr();
    }

    @Override
    public long getNativeBrowserWindowPtr(Profile profile, Activity activity) {
        ThreadUtils.assertOnUiThread();
        assert mState == State.PENDING_CREATE
                        || mState == State.IDLE
                        || mState == State.PENDING_UPDATE
                : "This Task is not pending or alive.";
        for (var obj : mActivityScopedObjectsDeque) {
            if (obj.mActivityScopedObjects.mActivityWindowAndroid.getActivity().get() == activity) {
                var browserWindow = obj.mAndroidBrowserWindows.get(profile);
                if (browserWindow != null) {
                    return browserWindow.getNativePtr();
                }
                return 0;
            }
        }
        return 0;
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

        removeAllFeatures();
        ProfileManager.removeObserver(mProfileObserver);

        removeAllActivityScopedObjects();

        mState = State.DESTROYED;
    }

    /**
     * Destroys an {@link AndroidBrowserWindow} with guaranteed correctness of the order of method
     * calls and object destruction.
     *
     * <p>Do not destroy an {@link AndroidBrowserWindow} in any other way; always use this method.
     *
     * <p>Do not make this method non-static; being stateless helps guarantee its correctness.
     *
     * @param browserWindow The {@link AndroidBrowserWindow} to destroy.
     * @param InternalActivityScopedObjects The {@link InternalActivityScopedObjects} the given
     *     {@code browserWindow} is associated with; failure to provide the correct {@link
     *     InternalActivityScopedObjects} will result in a crash.
     * @param browserWindowObservers Observers to be notified of the {@link AndroidBrowserWindow}
     *     destruction.
     */
    private static void destroyBrowserWindow(
            AndroidBrowserWindow browserWindow,
            @Nullable InternalActivityScopedObjects internalActivityScopedObjects,
            AndroidBrowserWindowObserverNotifier browserWindowObserverNotifier) {
        // Check if the given browserWindow matches internalActivityScopedObjects.
        if (internalActivityScopedObjects == null) {
            assert browserWindow.getActivityWindowAndroid() == null;
        } else {
            assert browserWindow.getActivityWindowAndroid()
                    == internalActivityScopedObjects.mActivityScopedObjects.mActivityWindowAndroid;
        }
        long ptr = browserWindow.getNativePtr();
        assert ptr != 0 : "Native object has not been created.";

        if (internalActivityScopedObjects != null) {
            var profile = browserWindow.getProfile();
            internalActivityScopedObjects
                    .mActivityScopedObjects
                    .mTabModelSelector
                    .getModel(profile.isOffTheRecord())
                    .dissociateWithBrowserWindow();
            internalActivityScopedObjects.mAndroidBrowserWindows.remove(profile);
        }

        // Note: Notify observers immediately before browserWindow.destroy(), and after everything
        // else.
        browserWindowObserverNotifier.notifyBrowserWindowDestroyed(browserWindow);
        browserWindow.destroy();
    }

    @Override
    public void onGlobalLayout() {
        ThreadUtils.assertOnUiThread();
        useActivity(
                topActivityScopedObjects -> {
                    var display = topActivityScopedObjects.mActivityWindowAndroid.getDisplay();
                    mWindowStateManager.update(topActivityScopedObjects.mActivity, display);

                    if (!mWindowStateManager.boundsChangedInDp()) {
                        return;
                    }

                    for (var feature : mFeatures.values()) {
                        feature.onTaskBoundsChanged(
                                display.getDisplayId(),
                                mWindowStateManager.getCurrentBoundsInDp(),
                                mWindowStateManager.getCurrentBoundsInPx());
                    }
                });
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

        return mWindowStateManager.getWindowState() == WindowState.MAXIMIZED;
    }

    @Override
    public boolean isMinimized() {
        ThreadUtils.assertOnUiThread();
        @Nullable Boolean isVisibleFuture = mPendingActionManager.isVisibleFuture(mState);
        if (isVisibleFuture != null) {
            return !isVisibleFuture;
        }
        return mWindowStateManager.getWindowState() == WindowState.MINIMIZED;
    }

    @Override
    public boolean isFullscreen() {
        ThreadUtils.assertOnUiThread();
        if (mState == State.PENDING_CREATE) {
            return false;
        }

        return mWindowStateManager.getWindowState() == WindowState.FULLSCREEN;
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
                    Rect restoredBoundsInPx = mWindowStateManager.getRestoredBoundsInPx();
                    if (restoredBoundsInPx == null) {
                        restoredBoundsInPx = mWindowStateManager.getCurrentBoundsInPx();
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

        return mWindowStateManager.getCurrentBoundsInDp();
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

        return mWindowStateManager.getWindowState() != WindowState.MINIMIZED;
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
    public void close() {
        ThreadUtils.assertOnUiThread();
        if (mState == State.PENDING_CREATE) {
            mPendingActionManager.requestAction(PendingAction.CLOSE);
            return;
        }

        useActivity(
                topActivityScopedObjects ->
                        topActivityScopedObjects.mActivity.finishAndRemoveTask());
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
    public @WindowResizePrecheckResult int canResize() {
        ThreadUtils.assertOnUiThread();
        return useActivity(
                ChromeAndroidTaskImpl::canResizeInternal,
                /* defaultValue= */ WindowResizePrecheckResult.NO_ACTIVITY);
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

        useActivity(
                topActivityScopedObjects ->
                        restoreInternal(
                                topActivityScopedObjects,
                                mWindowStateManager.getRestoredBoundsInPx()));
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
    public void onTopResumedActivityChangedWithNative(boolean isTopResumedActivity) {
        ThreadUtils.assertOnUiThread();
        if (isTopResumedActivity) {
            setLastActivatedTimeMillis();
        }

        if (isTopResumedActivity && !mIsTopActivityResumedWithNative) {
            mIsTopActivityResumedWithNative = true;
        }

        if (mIsTopActivityResumedWithNative) {
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

        mAndroidBrowserWindowObserverNotifier.updateActiveBrowserWindow(getActiveBrowserWindow());
    }

    @Override
    public void onTaskVisibilityChanged(int taskId, boolean isVisible) {
        ThreadUtils.assertOnUiThread();
        useActivity(
                topActivityScopedObjects ->
                        mWindowStateManager.update(
                                topActivityScopedObjects.mActivity,
                                topActivityScopedObjects.mActivityWindowAndroid.getDisplay()));

        if (mId == null || taskId != mId) return;

        for (var feature : mFeatures.values()) {
            feature.onTaskVisibilityChanged(isVisible);
        }

        if (mState != State.PENDING_UPDATE) return;
        if (!isVisible) {
            @PendingAction
            int[] actions =
                    mPendingActionManager.getAndClearTargetPendingActions(PendingAction.MINIMIZE);
            maybeSetStateIdle(actions);
        }
    }

    List<ActivityScopedObjects> getActivityScopedObjectsListForTesting() {
        ThreadUtils.assertOnUiThread();

        List<ActivityScopedObjects> resultList =
                new ArrayList<>(mActivityScopedObjectsDeque.size());
        for (InternalActivityScopedObjects internalObj : mActivityScopedObjectsDeque) {
            resultList.add(internalObj.mActivityScopedObjects);
        }

        return resultList;
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
        var browserWindow = getTopmostWindowWithProfile(profile);
        return browserWindow == null ? null : browserWindow.getNativeSessionIdForTesting();
    }

    @Override
    public List<Long> getAllNativeBrowserWindowPtrs() {
        ThreadUtils.assertOnUiThread();
        List<Long> allNativeBrowserWindowPtrs = new ArrayList<>();
        for (var internalActivityScopedObjects : mActivityScopedObjectsDeque) {

            Iterator<Map.Entry<Profile, AndroidBrowserWindow>> iterator =
                    internalActivityScopedObjects.mAndroidBrowserWindows.entrySet().iterator();
            while (iterator.hasNext()) {
                var entry = iterator.next();
                allNativeBrowserWindowPtrs.add(entry.getValue().getOrCreateNativePtr());
            }
        }
        if (mPendingBrowserWindow != null) {
            allNativeBrowserWindowPtrs.add(mPendingBrowserWindow.getOrCreateNativePtr());
        }
        return allNativeBrowserWindowPtrs;
    }

    @Override
    public void addAndroidBrowserWindowObserver(AndroidBrowserWindowObserver observer) {
        mAndroidBrowserWindowObserverNotifier.addObserver(observer);
    }

    @Override
    public void removeAndroidBrowserWindowObserver(AndroidBrowserWindowObserver observer) {
        mAndroidBrowserWindowObserverNotifier.removeObserver(observer);
    }

    @Override
    public boolean hasAndroidBrowserWindowObserver(AndroidBrowserWindowObserver observer) {
        return mAndroidBrowserWindowObserverNotifier.hasObserver(observer);
    }

    @VisibleForTesting
    @State
    int getState() {
        return mState;
    }

    List<AndroidBrowserWindow> getBrowserWindowsForTesting(Profile profile) {
        List<AndroidBrowserWindow> windows = new ArrayList<>();
        for (var internalActivityScopedObjects : mActivityScopedObjectsDeque) {
            var browserWindow = internalActivityScopedObjects.mAndroidBrowserWindows.get(profile);
            if (browserWindow != null) {
                windows.add(browserWindow);
            }
        }
        if (mPendingBrowserWindow != null && mPendingBrowserWindow.getProfile() == profile) {
            windows.add(mPendingBrowserWindow);
        }
        return windows;
    }

    /**
     * Returns the first {@link AndroidBrowserWindow} from the top of {@link
     * mActivityScopedObjectsDeque} that matches the given {@link Profile}, or null if no such
     * {@link AndroidBrowserWindow} exists.
     */
    @Nullable
    private AndroidBrowserWindow getTopmostWindowWithProfile(Profile profile) {
        for (var internalActivityScopedObjects : mActivityScopedObjectsDeque) {
            var browserWindow = internalActivityScopedObjects.mAndroidBrowserWindows.get(profile);
            if (browserWindow != null) {
                return browserWindow;
            }
        }
        if (mPendingBrowserWindow != null && mPendingBrowserWindow.getProfile() == profile) {
            return mPendingBrowserWindow;
        }
        return null;
    }

    private void addActivityScopedObjectsInternal(ActivityScopedObjects activityScopedObjects) {
        assertPendingCreateOrIdle();

        // Get everything we need from the ActivityScopedObjects to be added.
        var activityWindowAndroid = activityScopedObjects.mActivityWindowAndroid;
        var tabModel = activityScopedObjects.mTabModelSelector.getCurrentModel();
        var profile = tabModel.getProfile();

        // Precondition checks.
        assert profile != null;
        if (mState == State.IDLE) {
            assert mId != null;
            assert mId == getTaskId(getActivity(activityWindowAndroid))
                    : "The new ActivityWindowAndroid doesn't belong to this Task.";
        } else {
            assert mId == null;
        }

        // Unregister all listeners for the current top Activity.
        // This must be done before changing mActivityScopedObjectsDeque.
        unregisterListenersForTopActivity();

        // See if the ActivityScopedObjects to be added already exists.
        var existingInternalActivityScopedObjects =
                findInternalActivityScopedObjects(activityWindowAndroid);

        if (existingInternalActivityScopedObjects != null) {
            // If the ActivityScopedObjects to be added already exists,
            // move it to the top of the deque.
            mActivityScopedObjectsDeque.remove(existingInternalActivityScopedObjects);
            mActivityScopedObjectsDeque.addFirst(existingInternalActivityScopedObjects);
        } else {
            // Create a new AndroidBrowserWindow.
            AndroidBrowserWindow newBrowserWindow;
            if (mState == State.PENDING_CREATE) {
                assert mActivityScopedObjectsDeque.isEmpty() && mPendingBrowserWindow != null
                        : "addActivityScopedObjects() called in an invalid state.";
                assert mPendingBrowserWindow.getProfile() == profile;
                newBrowserWindow = mPendingBrowserWindow;
                newBrowserWindow.setActivityWindowAndroid(activityWindowAndroid);
                mPendingBrowserWindow = null;
            } else {
                newBrowserWindow =
                        new AndroidBrowserWindow(
                                /* chromeAndroidTask= */ this,
                                profile,
                                activityScopedObjects.mBrowserWindowType,
                                activityWindowAndroid);
            }

            // Associate the new AndroidBrowserWindow with TabModel.
            tabModel.associateWithBrowserWindow(newBrowserWindow.getOrCreateNativePtr());

            // Create a new InternalActivityScopedObjects instance, and
            // add it to the top of the deque.
            var internalActivityScopedObjects =
                    new InternalActivityScopedObjects(activityScopedObjects, newBrowserWindow);
            mActivityScopedObjectsDeque.addFirst(internalActivityScopedObjects);

            if (activityScopedObjects.mSupportedProfileType == SupportedProfileType.MIXED) {
                internalActivityScopedObjects.addIncognitoTabModelObserver(this);
            }

            // Notify observers of new window creation.
            mAndroidBrowserWindowObserverNotifier.notifyBrowserWindowAdded(newBrowserWindow);
            mAndroidBrowserWindowObserverNotifier.updateActiveBrowserWindow(
                    getActiveBrowserWindow());
        }

        // By this point, mActivityScopedObjectsDeque has been correctly
        // updated. Register listeners for its current top Activity.
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
        var internalActivityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
        if (internalActivityScopedObjects == null) {
            return;
        }
        var topActivityScopedObjects = internalActivityScopedObjects.mActivityScopedObjects;
        var topActivityWindowAndroid = topActivityScopedObjects.mActivityWindowAndroid;

        // Register Activity LifecycleObservers
        var lifecycleDispatcher = getActivityLifecycleDispatcher(topActivityWindowAndroid);
        lifecycleDispatcher.register(this);
        if (lifecycleDispatcher.getCurrentActivityState() == ActivityState.RESUMED_WITH_NATIVE) {
            mIsTopActivityResumedWithNative = true;
        }
        if (mIsTopActivityResumedWithNative) {
            completePendingCreate();
        }

        // Register Task VisibilityListener
        ApplicationStatus.registerTaskVisibilityListener(this);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && topActivityWindowAndroid.getInsetObserver() != null) {
            topActivityWindowAndroid
                    .getInsetObserver()
                    .addWindowInsetsAnimationListener(mWindowInsetsAnimationListener);
        }

        TabModelSelector tabModelSelector = topActivityScopedObjects.mTabModelSelector;

        tabModelSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndPostIfNonNull(mOnTabModelSelectedCallback);
        onTabModelSelected(tabModelSelector.getCurrentModel());

        getActivity(topActivityWindowAndroid)
                .findViewById(android.R.id.content)
                .getViewTreeObserver()
                .addOnGlobalLayoutListener(this);

        mWindowStateManager.update(
                getActivity(topActivityWindowAndroid), topActivityWindowAndroid.getDisplay());
    }

    private void unregisterListenersForTopActivity() {
        var internalActivityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
        if (internalActivityScopedObjects == null) {
            return;
        }
        var topActivityScopedObjects = internalActivityScopedObjects.mActivityScopedObjects;
        var topActivityWindowAndroid = topActivityScopedObjects.mActivityWindowAndroid;

        // Unregister Activity LifecycleObservers.
        getActivityLifecycleDispatcher(topActivityWindowAndroid).unregister(this);

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
            tabModelSelector
                    .getCurrentTabModelSupplier()
                    .removeObserver(mOnTabModelSelectedCallback);
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
                    topActivityScopedObjects.mActivity.finishAndRemoveTask();
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

    @Nullable
    private InternalActivityScopedObjects findInternalActivityScopedObjects(
            ActivityWindowAndroid activityWindowAndroid) {
        InternalActivityScopedObjects result = null;
        for (var internalActivityScopedObjects : mActivityScopedObjectsDeque) {
            if (internalActivityScopedObjects.mActivityScopedObjects.mActivityWindowAndroid
                    == activityWindowAndroid) {
                assert result == null
                        : "the same instance of ActivityScopedObjects was added more than once";
                result = internalActivityScopedObjects;
            }
        }
        return result;
    }

    private void removeActivityScopedObjectsInternal(ActivityWindowAndroid activityWindowAndroid) {
        var activityScopedObjectsToRemove =
                findInternalActivityScopedObjects(activityWindowAndroid);
        if (activityScopedObjectsToRemove == null) {
            return;
        }

        // Remove from Deque.
        mActivityScopedObjectsDeque.remove(activityScopedObjectsToRemove);
        // Handle observers.
        activityScopedObjectsToRemove.removeIncognitoTabModelObserver();
        // Remove task features.
        removeAllFeaturesForActivityInternal(activityWindowAndroid);
        // Destroy associated windows.
        var windows =
                new ArrayList<>(activityScopedObjectsToRemove.mAndroidBrowserWindows.values());
        for (var window : windows) {
            destroyBrowserWindow(
                    window, activityScopedObjectsToRemove, mAndroidBrowserWindowObserverNotifier);
        }
    }

    private void removeAllFeaturesForActivityInternal(ActivityWindowAndroid activityWindowAndroid) {
        Iterator<Entry<ChromeAndroidTaskFeatureKey, ChromeAndroidTaskFeature>> iterator =
                mFeatures.entrySet().iterator();
        while (iterator.hasNext()) {
            Entry<ChromeAndroidTaskFeatureKey, ChromeAndroidTaskFeature> entry = iterator.next();
            ChromeAndroidTaskFeatureKey key = entry.getKey();
            if (activityWindowAndroid == key.mActivityWindowAndroid) {
                entry.getValue().onFeatureRemoved();
                iterator.remove();
            }
        }
    }

    private void removeAllFeaturesForTabModel(TabModel tabModel) {
        Iterator<Entry<ChromeAndroidTaskFeatureKey, ChromeAndroidTaskFeature>> iterator =
                mFeatures.entrySet().iterator();
        while (iterator.hasNext()) {
            Entry<ChromeAndroidTaskFeatureKey, ChromeAndroidTaskFeature> entry = iterator.next();
            ChromeAndroidTaskFeatureKey key = entry.getKey();
            if (tabModel == key.mTabModel) {
                entry.getValue().onFeatureRemoved();
                iterator.remove();
            }
        }
    }

    private void removeAllActivityScopedObjects() {
        unregisterListenersForTopActivity();

        while (!mActivityScopedObjectsDeque.isEmpty()) {
            var internalActivityScopedObjects = mActivityScopedObjectsDeque.pollFirst();
            internalActivityScopedObjects.removeIncognitoTabModelObserver();
            removeAllFeaturesForActivityInternal(
                    internalActivityScopedObjects.mActivityScopedObjects.mActivityWindowAndroid);
            var windows =
                    new ArrayList<>(internalActivityScopedObjects.mAndroidBrowserWindows.values());
            for (var window : windows) {
                destroyBrowserWindow(
                        window,
                        internalActivityScopedObjects,
                        mAndroidBrowserWindowObserverNotifier);
            }
        }
        if (mPendingBrowserWindow != null) {
            destroyBrowserWindow(
                    mPendingBrowserWindow, null, mAndroidBrowserWindowObserverNotifier);
            mPendingBrowserWindow = null;
        }
        mAndroidBrowserWindowObserverNotifier.updateActiveBrowserWindow(getActiveBrowserWindow());
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

    private void removeAllFeatures() {
        for (var feature : mFeatures.values()) {
            feature.onFeatureRemoved();
        }
        mFeatures.clear();
    }

    private void removeAllFeaturesForProfile(Profile profile) {
        var iterator = mFeatures.entrySet().iterator();
        while (iterator.hasNext()) {
            var entry = iterator.next();
            ChromeAndroidTaskFeatureKey key = entry.getKey();
            if (profile.equals(key.mProfile)) {
                entry.getValue().onFeatureRemoved();
                iterator.remove();
            }
        }
    }

    private void assertAlive() {
        assert mState == State.IDLE || mState == State.PENDING_UPDATE : "This Task is not alive.";
    }

    private void assertPendingCreateOrIdle() {
        assert mState == State.IDLE || mState == State.PENDING_CREATE
                : "This Task is neither pending create nor idle. Current state: " + mState;
    }

    private static boolean isActiveInternal(TopActivityScopedObjects topActivityScopedObjects) {
        return topActivityScopedObjects.mActivityWindowAndroid.isTopResumedActivity();
    }

    private void setBoundsInPx(TopActivityScopedObjects topActivityScopedObjects, Rect boundsInPx) {
        var activity = topActivityScopedObjects.mActivity;
        int displayId = topActivityScopedObjects.mActivityWindowAndroid.getDisplay().getDisplayId();

        var aconfigFlaggedApiDelegate = AconfigFlaggedApiDelegate.getInstance();
        var appTask = AndroidTaskUtils.getAppTaskFromId(activity, getTaskId(activity));
        assert aconfigFlaggedApiDelegate != null && appTask != null
                : "use canResizeInternal() to prevent null values";

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
        if (mWindowStateManager.getWindowState() != WindowState.MINIMIZED) {
            var activity = topActivityScopedObjects.mActivity;
            mPendingActionManager.requestAction(PendingAction.SHOW);
            mState = State.PENDING_UPDATE;
            ApiCompatibilityUtils.moveTaskToFront(activity, getTaskId(activity), 0);
        }
    }

    private void activateInternal(TopActivityScopedObjects topActivityScopedObjects) {
        var activity = topActivityScopedObjects.mActivity;
        mPendingActionManager.requestAction(PendingAction.ACTIVATE);
        mState = State.PENDING_UPDATE;
        ApiCompatibilityUtils.moveTaskToFront(activity, getTaskId(activity), 0);
    }

    @RequiresApi(api = VERSION_CODES.R)
    private void maximizeInternal(TopActivityScopedObjects topActivityScopedObjects) {
        // Precondition: the Task (window) allows bounds change.
        if (canResizeInternal(topActivityScopedObjects) != WindowResizePrecheckResult.OK) {
            return;
        }

        if (mWindowStateManager.getWindowState() == WindowState.MINIMIZED) {
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
        if (mWindowStateManager.getWindowState() == WindowState.MINIMIZED) {
            return;
        }

        mPendingActionManager.requestAction(PendingAction.MINIMIZE);
        mState = State.PENDING_UPDATE;
        topActivityScopedObjects.mActivity.moveTaskToBack(/* nonRoot= */ true);
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
        if (canResizeInternal(topActivityScopedObjects) != WindowResizePrecheckResult.OK) {
            return;
        }

        if (mWindowStateManager.getWindowState() == WindowState.MINIMIZED) {
            activateInternal(topActivityScopedObjects);
        }

        mPendingActionManager.requestRestore(convertBoundsInPxToDp(restoredBoundsInPx, display));
        mState = State.PENDING_UPDATE;
        setBoundsInPx(topActivityScopedObjects, restoredBoundsInPx);
    }

    private void setBoundsInDpInternal(
            TopActivityScopedObjects topActivityScopedObjects, Rect boundsInDp) {
        // Precondition 1: new bounds are not the same as the current bounds.
        if (mWindowStateManager.getCurrentBoundsInDp().equals(boundsInDp)) return;

        // Precondition 2: the Task (window) allows bounds change.
        if (canResizeInternal(topActivityScopedObjects) != WindowResizePrecheckResult.OK) {
            return;
        }

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
        mAndroidBrowserWindowObserverNotifier.updateActiveBrowserWindow(getActiveBrowserWindow());
    }

    private @Nullable AndroidBrowserWindow getActiveBrowserWindow() {
        var internalActivityScopedObjects = mActivityScopedObjectsDeque.peekFirst();
        if (internalActivityScopedObjects != null) {
            var windowAndroid =
                    internalActivityScopedObjects.mActivityScopedObjects.mActivityWindowAndroid;
            if (!windowAndroid.isTopResumedActivity()) {
                return null;
            }
            var tabModel =
                    internalActivityScopedObjects.mActivityScopedObjects.mTabModelSelector
                            .getCurrentModel();
            if (tabModel != null) {
                var profile = tabModel.getProfile();
                if (profile != null) {
                    return internalActivityScopedObjects.mAndroidBrowserWindows.get(profile);
                }
            }
        }
        return null;
    }

    private InitInfo createInitInfo(ChromeAndroidTaskFeatureKey featureKey) {
        AndroidBrowserWindow browserWindowForFeature = null;
        for (var obj : mActivityScopedObjectsDeque) {
            if (obj.mActivityScopedObjects.mActivityWindowAndroid
                    != featureKey.mActivityWindowAndroid) {
                continue;
            }

            browserWindowForFeature = obj.mAndroidBrowserWindows.get(featureKey.mProfile);
            if (browserWindowForFeature != null) {
                break;
            }
        }

        long nativeBrowserWindowPtr =
                browserWindowForFeature != null
                        ? browserWindowForFeature.getOrCreateNativePtr()
                        : 0;

        return useActivity(
                activityScopedObjects -> {
                    // WindowStateManager#getWindowState() will return a valid value that will be
                    // set only on Android R+. On pre-R devices, determine task visibility from
                    // ApplicationStatus.
                    boolean isTaskVisible;
                    if (VERSION.SDK_INT < VERSION_CODES.R) {
                        isTaskVisible =
                                ApplicationStatus.isTaskVisible(
                                        getTaskId(activityScopedObjects.mActivity));
                    } else {
                        isTaskVisible =
                                mWindowStateManager.getWindowState() != WindowState.MINIMIZED;
                    }
                    int displayId =
                            activityScopedObjects
                                    .mActivityWindowAndroid
                                    .getDisplay()
                                    .getDisplayId();
                    return new InitInfo(
                            nativeBrowserWindowPtr,
                            isTaskVisible,
                            mWindowStateManager.getCurrentBoundsInPx(),
                            mWindowStateManager.getCurrentBoundsInDp(),
                            displayId);
                },
                new InitInfo(
                        nativeBrowserWindowPtr,
                        /* isVisible= */ false,
                        new Rect(),
                        new Rect(),
                        Display.DEFAULT_DISPLAY));
    }

    @VisibleForTesting
    static Rect convertBoundsInPxToDp(Rect boundsInPx, DisplayAndroid displayAndroid) {
        return DisplayUtil.scaleToEnclosingRect(boundsInPx, 1.0f / displayAndroid.getDipScale());
    }

    @Nullable Rect getRestoredBoundsInPxForTesting() {
        ThreadUtils.assertOnUiThread();
        return mWindowStateManager.getRestoredBoundsInPx();
    }

    PendingActionManager getPendingActionManagerForTesting() {
        return mPendingActionManager;
    }
}
