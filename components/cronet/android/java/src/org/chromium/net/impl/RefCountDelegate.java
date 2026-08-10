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
        int updatedCount = mCount.incrementAndGet();
        assert updatedCount > 1 : "increment() called on a RefCountDelegate with count < 1";
    }

    public void decrement() {
        int updatedCount = mCount.decrementAndGet();
        assert updatedCount >= 0 : "decrement() called on a RefCountDelegate with count < 1";
        if (updatedCount == 0) {
            mDelegate.run();
        }
    }
}
