// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.app.Activity;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.WindowFocusChangedObserver;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Tracks the window focus state of the activity using {@link ActivityLifecycleDispatcher}. */
@NullMarked
public class WindowFocusSupplier
        implements NonNullObservableSupplier<Boolean>, WindowFocusChangedObserver, Destroyable {
    private final SettableNonNullObservableSupplier<Boolean> mSupplier;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    /**
     * @param activityLifecycleDispatcher Dispatcher used to observe window focus events.
     * @param windowAndroid WindowAndroid used to fetch initial focus state.
     */
    public WindowFocusSupplier(
            ActivityLifecycleDispatcher activityLifecycleDispatcher, WindowAndroid windowAndroid) {
        WeakReference<Activity> activityRef = windowAndroid.getActivity();
        Activity activity = activityRef != null ? activityRef.get() : null;
        boolean initialFocus = activity != null && activity.hasWindowFocus();
        mSupplier = ObservableSuppliers.createNonNull(initialFocus);
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        mSupplier.set(hasFocus);
    }

    @Override
    public Boolean addObserver(Callback<Boolean> observer, int behavior) {
        return mSupplier.addObserver(observer, behavior);
    }

    @Override
    public void removeObserver(Callback<Boolean> observer) {
        mSupplier.removeObserver(observer);
    }

    @Override
    public Boolean get() {
        return mSupplier.get();
    }

    @Override
    public int getObserverCount() {
        return mSupplier.getObserverCount();
    }

    @Override
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(this);
    }
}
