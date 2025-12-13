// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.cc.input.BrowserControlsState;

/** A delegate to determine visibility of the browser controls. */
@NullMarked
public class BrowserControlsVisibilityDelegate
        implements NonNullObservableSupplier<@BrowserControlsState Integer> {
    protected SettableNonNullObservableSupplier<Integer> mDelegateSupplier;

    public BrowserControlsVisibilityDelegate() {
        this(BrowserControlsState.BOTH);
    }

    /**
     * Constructs a delegate that controls the visibility of the browser controls.
     *
     * @param initialValue The initial browser state visibility.
     */
    public BrowserControlsVisibilityDelegate(@BrowserControlsState int initialValue) {
        mDelegateSupplier = ObservableSuppliers.createNonNull(initialValue);
    }

    @Override
    public @BrowserControlsState Integer addObserver(
            Callback<@BrowserControlsState Integer> obs, int behavior) {
        return mDelegateSupplier.addObserver(obs, behavior);
    }

    @Override
    public void removeObserver(Callback<@BrowserControlsState Integer> obs) {
        mDelegateSupplier.removeObserver(obs);
    }

    @Override
    public int getObserverCount() {
        return mDelegateSupplier.getObserverCount();
    }

    @Override
    public @BrowserControlsState Integer get() {
        return mDelegateSupplier.get();
    }

    public void set(@BrowserControlsState int value) {
        mDelegateSupplier.set(value);
    }
}
