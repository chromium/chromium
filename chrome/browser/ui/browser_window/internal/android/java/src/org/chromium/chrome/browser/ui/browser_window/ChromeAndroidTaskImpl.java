// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.WindowInsets;
import android.view.WindowManager;

import androidx.annotation.GuardedBy;
import androidx.annotation.RequiresApi;

import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.ui.base.ActivityWindowAndroid;

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
                TopResumedActivityChangedWithNativeObserver {

    private static final String TAG = "ChromeAndroidTask";

    /** States of this {@link ChromeAndroidTask}. */
    private enum State {
        /* The Task is alive. */
        ALIVE,

        /** The Task is being destroyed, but the destruction hasn't been completed. */
        DESTROYING,

        /** The Task has been destroyed. */
        DESTROYED,
    }

    private final AtomicReference<State> mState = new AtomicReference<>(State.ALIVE);

    private final int mId;

    private final AndroidBrowserWindow mAndroidBrowserWindow;

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
    private @Nullable Rect mLastBoundsOnConfigChanged;

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

    ChromeAndroidTaskImpl(ActivityWindowAndroid activityWindowAndroid) {
        mId = getActivity(activityWindowAndroid).getTaskId();
        mAndroidBrowserWindow = new AndroidBrowserWindow(/* chromeAndroidTask= */ this);
        setActivityWindowAndroidInternal(activityWindowAndroid);
    }

    @Override
    public int getId() {
        return mId;
    }

    @Override
    public void setActivityWindowAndroid(ActivityWindowAndroid activityWindowAndroid) {
        setActivityWindowAndroidInternal(activityWindowAndroid);
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
        assertAlive();
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
        synchronized (mActivityWindowAndroidLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null) return false;
            return activityWindowAndroid.isTopResumedActivity();
        }
    }

    @Override
    public boolean isMaximized() {
        // TODO(crbug.com/438268202): Change the if statement to an assert.
        // We don't expect ChromeAndroidTask to work for R and below.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "isMaximized() requires Android R+; returning a false");
            return false;
        }
        synchronized (mActivityWindowAndroidLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null) return false;
            var activity = activityWindowAndroid.getActivity().get();
            if (activity == null) return false;
            var windowManager = activity.getWindowManager();
            if (isInDesktopWindowing(windowManager)) {
                return getBoundsInternalLocked().equals(getMaximizedBounds(windowManager));
            } else {
                return !activity.isInMultiWindowMode();
            }
        }
    }

    @Override
    public long getLastActivatedTimeMillis() {
        long lastActivatedTimeMillis = mLastActivatedTimeMillis.get();
        assert lastActivatedTimeMillis > 0;
        return lastActivatedTimeMillis;
    }

    @Override
    public Rect getBounds() {
        synchronized (mActivityWindowAndroidLock) {
            return getBoundsInternalLocked();
        }
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
                var newBounds = getBoundsInternalLocked();
                if (newBounds.equals(mLastBoundsOnConfigChanged)) {
                    return;
                }

                mLastBoundsOnConfigChanged = newBounds;
                for (var feature : mFeatures) {
                    feature.onTaskBoundsChanged(newBounds);
                }
            }
        }
    }

    @Override
    public void close() {
        synchronized (mActivityWindowAndroidLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null) return;
            Activity activity = activityWindowAndroid.getActivity().get();
            if (activity == null) return;
            activity.finishAndRemoveTask();
        }
    }

    @Override
    public void activate() {
        synchronized (mActivityWindowAndroidLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null) return;
            Activity activity = activityWindowAndroid.getActivity().get();
            if (activity == null) return;
            ActivityManager activityManager =
                    (ActivityManager) activity.getSystemService(Context.ACTIVITY_SERVICE);
            for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
                if (activity.getTaskId() == task.getTaskInfo().id) {
                    task.moveToFront();
                    return;
                }
            }
            throw new IllegalStateException("Target task not found");
        }
    }

    @Override
    public void maximize() {
        // TODO(crbug.com/438268202): Change the if statement to an assert.
        // We don't expect ChromeAndroidTask to work for R and below.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "maximize() requires Android R+; does nothing");
            return;
        }
        synchronized (mActivityWindowAndroidLock) {
            var activityWindowAndroid =
                    getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
            if (activityWindowAndroid == null) return;
            Activity activity = activityWindowAndroid.getActivity().get();
            if (activity == null) return;
            // No maximize action in non desktop window mode.
            if (!isInDesktopWindowing(activity.getWindowManager())) return;
            Rect maximizedBounds = getMaximizedBounds(activity.getWindowManager());
            ActivityOptions options = ActivityOptions.makeBasic();
            options.setLaunchBounds(maximizedBounds);
            Intent intent = new Intent(activity, activity.getClass());
            // TODO(crbug.com/437982549): Replace with new Task API if available.
            // When maximize() is called while another activity is on top, Chrome is brought to the
            // foreground. Subsequently, if a back gesture is triggered, FLAG_ACTIVITY_CLEAR_TOP
            // prevents the other activity from being brought back to the top of the stack.
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
            activity.startActivity(intent, options.toBundle());
        }
    }

    @Override
    public void onTopResumedActivityChangedWithNative(boolean isTopResumedActivity) {
        if (isTopResumedActivity) {
            mLastActivatedTimeMillis.set(TimeUtils.elapsedRealtimeMillis());
        }

        synchronized (mFeaturesLock) {
            for (var feature : mFeatures) {
                feature.onTaskFocusChanged(isTopResumedActivity);
            }
        }
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

    private void setActivityWindowAndroidInternal(ActivityWindowAndroid activityWindowAndroid) {
        synchronized (mActivityWindowAndroidLock) {
            assertAlive();
            assert mActivityWindowAndroid.get() == null
                    : "This Task already has an ActivityWindowAndroid.";
            assert mId == getActivity(activityWindowAndroid).getTaskId()
                    : "The new ActivityWindowAndroid doesn't belong to this Task.";

            mActivityWindowAndroid = new WeakReference<>(activityWindowAndroid);

            // Register Activity LifecycleObservers
            getActivityLifecycleDispatcher(activityWindowAndroid).register(this);
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
    private Rect getBoundsInternalLocked() {
        // TODO(crbug.com/438268202): Change the if statement to an assert.
        // We don't expect ChromeAndroidTask to work for R and below.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "getBoundsInternal() requires Android R+; returning an empty Rect()");
            return new Rect();
        }

        var activityWindowAndroid = getActivityWindowAndroidInternalLocked(/* assertAlive= */ true);
        if (activityWindowAndroid == null) return new Rect();
        Activity activity = activityWindowAndroid.getActivity().get();
        if (activity == null) return new Rect();
        return activity.getWindowManager().getCurrentWindowMetrics().getBounds();
    }

    private void assertAlive() {
        assert mState.get() == State.ALIVE : "This Task is not alive.";
    }

    @RequiresApi(api = VERSION_CODES.R)
    private static Rect getMaximizedBounds(WindowManager windowManager) {
        var insets =
                windowManager
                        .getMaximumWindowMetrics()
                        .getWindowInsets()
                        .getInsets(WindowInsets.Type.tappableElement());
        var fullscreenBounds = windowManager.getMaximumWindowMetrics().getBounds();
        return new Rect(
                0, insets.top, fullscreenBounds.right, fullscreenBounds.bottom - insets.bottom);
    }

    @RequiresApi(api = VERSION_CODES.R)
    // TODO(crbug.com/437982549): Replace with a more versatile API to improve OEM compatibility.
    private static boolean isInDesktopWindowing(WindowManager windowManager) {
        return windowManager
                .getCurrentWindowMetrics()
                .getWindowInsets()
                .isVisible(WindowInsets.Type.captionBar());
    }
}
