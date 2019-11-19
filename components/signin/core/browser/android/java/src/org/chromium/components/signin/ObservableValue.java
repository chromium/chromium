// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.support.v4.util.ObjectsCompat;

import androidx.annotation.MainThread;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;

/**
 * Simple observable value that notifies all observers whenever the value changes. Should only be
 * used from the main thread.
 * @param <T> The type of the stored value.
 */
@MainThread
public class ObservableValue<T> {
    /**
     * Observer to get notifications when the value is changed.
     */
    public interface Observer {
        /**
         * After an observer has been added using {@link ObservableValue#addObserver}, this method
         * is invoked whenever the value is changed.
         */
        void onValueChanged();
    }

    private T mValue;
    private final ObserverList<Observer> mObserverList = new ObserverList<>();

    protected ObservableValue(T initialValue) {
        mValue = initialValue;
    }

    /**
     * Returns the current value.
     */
    public T get() {
        ThreadUtils.assertOnUiThread();
        return mValue;
    }

    /**
     * Adds observer to watch value changes. Use {@link #removeObserver} to remove.
     * @param observer The observer to receive notifications trough {@link Observer#onValueChanged}.
     */
    public void addObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        assert observer != null;
        mObserverList.addObserver(observer);
    }

    /**
     * Removes observer that was added by {@link #addObserver}. The removed observer will no longer
     * receive notifications when value changes.
     * @param observer The observer to remove.
     */
    public void removeObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        assert observer != null;
        boolean result = mObserverList.removeObserver(observer);
        assert result : "No such observer";
    }

    protected void set(T value) {
        ThreadUtils.assertOnUiThread();
        if (ObjectsCompat.equals(mValue, value)) return;
        mValue = value;
        for (Observer observer : mObserverList) {
            observer.onValueChanged();
        }
    }
}
