// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.app.Activity;

import androidx.annotation.GuardedBy;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Implements {@link ChromeAndroidTask}. */
@NullMarked
final class ChromeAndroidTaskImpl implements ChromeAndroidTask {

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

    private static int getTaskId(ActivityWindowAndroid activityWindowAndroid) {
        Activity activity = activityWindowAndroid.getActivity().get();
        assert activity != null : "ActivityWindowAndroid should have an Activity.";

        return activity.getTaskId();
    }

    ChromeAndroidTaskImpl(ActivityWindowAndroid activityWindowAndroid) {
        mId = getTaskId(activityWindowAndroid);
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
        return getActivityWindowAndroidInternal(/* assertAlive= */ true);
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

    /**
     * Same as {@link #getActivityWindowAndroid()}, but skips asserting that the {@link
     * ChromeAndroidTask} is alive.
     *
     * <p>This method should only be used in tests.
     */
    @Nullable ActivityWindowAndroid getActivityWindowAndroidForTesting() {
        return getActivityWindowAndroidInternal(/* assertAlive= */ false);
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
            assert mId == getTaskId(activityWindowAndroid)
                    : "The new ActivityWindowAndroid doesn't belong to this Task.";

            mActivityWindowAndroid = new WeakReference<>(activityWindowAndroid);
        }
    }

    private @Nullable ActivityWindowAndroid getActivityWindowAndroidInternal(boolean assertAlive) {
        synchronized (mActivityWindowAndroidLock) {
            if (assertAlive) {
                assertAlive();
            }

            return mActivityWindowAndroid.get();
        }
    }

    private void clearActivityWindowAndroidInternal() {
        synchronized (mActivityWindowAndroidLock) {
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

    private void assertAlive() {
        assert mState.get() == State.ALIVE : "This Task is not alive.";
    }
}
