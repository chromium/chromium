// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import androidx.annotation.MainThread;

/**
 * {@link ObservableValue} subclass that allow value modification using {@link #set} method. Should
 * only be used from the main thread.
 * @param <T> The type of the stored value.
 */
@MainThread
public class MutableObservableValue<T> extends ObservableValue<T> {
    public MutableObservableValue(T initialValue) {
        super(initialValue);
    }

    /**
     * Sets the value. Calling this will synchronously notify all observers. Please note that the
     * internal state is updated before observers are notified, so {@link #get} invoked from
     * observer will return the updated value.
     */
    @Override
    public void set(T value) {
        super.set(value);
    }
}
