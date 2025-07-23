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
import java.util.concurrent.atomic.AtomicBoolean;

/** Implements {@link ChromeAndroidTask}. */
@NullMarked
final class ChromeAndroidTaskImpl implements ChromeAndroidTask {

    private final int mId;

    @SuppressWarnings("UnusedVariable")
    private final AndroidBrowserWindow mAndroidBrowserWindow;

    /**
     * Contains all {@link ChromeAndroidTaskFeature}s associated with this {@link
     * ChromeAndroidTask}.
     */
    @SuppressWarnings("UnusedVariable")
    private final List<ChromeAndroidTaskFeature> mFeatures = new ArrayList<>();

    private final Object mActivityWindowAndroidLock = new Object();

    /** Whether {@link #destroy()} has been called. */
    private final AtomicBoolean mDestroyed = new AtomicBoolean(false);

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
        return getActivityWindowAndroidInternal(/* assertNotDestroyed= */ true);
    }

    @Override
    public void clearActivityWindowAndroid() {
        clearActivityWindowAndroidInternal();
    }

    @Override
    public void destroy() {
        clearActivityWindowAndroidInternal();

        // TODO(crbug.com/427214087): Destroy mAndroidBrowserWindow.

        mDestroyed.set(true);
    }

    @Override
    public boolean isDestroyed() {
        return mDestroyed.get();
    }

    /**
     * Same as {@link #getActivityWindowAndroid()}, but skips asserting that the {@link
     * ChromeAndroidTask} isn't destroyed.
     *
     * <p>This method should only be used in tests.
     */
    @Nullable ActivityWindowAndroid getActivityWindowAndroidForTesting() {
        return getActivityWindowAndroidInternal(/* assertNotDestroyed= */ false);
    }

    private void setActivityWindowAndroidInternal(ActivityWindowAndroid activityWindowAndroid) {
        synchronized (mActivityWindowAndroidLock) {
            assertNotDestroyed();
            assert mActivityWindowAndroid.get() == null
                    : "This Task already has an ActivityWindowAndroid.";
            assert mId == getTaskId(activityWindowAndroid)
                    : "The new ActivityWindowAndroid doesn't belong to this Task.";

            mActivityWindowAndroid = new WeakReference<>(activityWindowAndroid);
        }
    }

    private @Nullable ActivityWindowAndroid getActivityWindowAndroidInternal(
            boolean assertNotDestroyed) {
        synchronized (mActivityWindowAndroidLock) {
            if (assertNotDestroyed) {
                assertNotDestroyed();
            }

            return mActivityWindowAndroid.get();
        }
    }

    private void clearActivityWindowAndroidInternal() {
        synchronized (mActivityWindowAndroidLock) {
            mActivityWindowAndroid.clear();
        }
    }

    private void assertNotDestroyed() {
        assert !mDestroyed.get(): "This Task is already destroyed.";
    }
}
