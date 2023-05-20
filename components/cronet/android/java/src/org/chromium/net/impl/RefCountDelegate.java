// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * A thread-safe counter that starts at 1 and executes a callback once it
 * reaches its final value of zero.
 */
public final class RefCountDelegate {
    private final AtomicInteger mCount = new AtomicInteger(1);
    private final Runnable mDelegate;

    public RefCountDelegate(Runnable delegate) {
        mDelegate = delegate;
    }

    public void increment() {
        int updated_count = mCount.incrementAndGet();
        assert updated_count > 1 : "increment() called on a RefCountDelegate with count < 1";
    }

    public void decrement() {
        int updated_count = mCount.decrementAndGet();
        assert updated_count >= 0 : "decrement() called on a RefCountDelegate with count < 1";
        if (updated_count == 0) {
            mDelegate.run();
        }
    }
}
