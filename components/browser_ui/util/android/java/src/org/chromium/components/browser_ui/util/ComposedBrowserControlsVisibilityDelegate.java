// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import org.chromium.base.Callback;
import org.chromium.cc.input.BrowserControlsState;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Delegate for the visibility of browser controls that combines the results of other delegates. */
public class ComposedBrowserControlsVisibilityDelegate extends BrowserControlsVisibilityDelegate {
    private final List<BrowserControlsVisibilityDelegate> mDelegates;
    private final Callback<Integer> mConstraintsUpdatedCallback;

    private boolean mSetDisabled;

    /**
     * Constructs a composed visibility delegate that will generate results based on the delegates
     * passed in.
     */
    public ComposedBrowserControlsVisibilityDelegate(
            BrowserControlsVisibilityDelegate... delegates) {
        super(BrowserControlsState.BOTH);
        mSetDisabled = true;
        mDelegates = new ArrayList<>(Arrays.asList(delegates));
        mConstraintsUpdatedCallback = (constraints) -> super.set(calculateVisibilityConstraints());
        // We start initially with no observers and we don't actively update the set() value here.
        // It will be calculated on-demand in get() instead.
    }

    /**
     * Adds an additional delegate to the composed visibility delegate that will determine the
     * overall visibility constraints.
     *
     * @param delegate The delegate to be added.
     */
    public void addDelegate(BrowserControlsVisibilityDelegate delegate) {
        mDelegates.add(delegate);
        if (hasObservers()) {
            delegate.addObserver(mConstraintsUpdatedCallback);
            // Update the set() value now, so get() right after this call will return the right
            // value.
            super.set(calculateVisibilityConstraints());
        }
    }

    @Override
    public Integer addObserver(Callback<Integer> obs) {
        if (!hasObservers()) {
            for (int i = 0; i < mDelegates.size(); i++) {
                mDelegates.get(i).addObserver(mConstraintsUpdatedCallback);
            }
            // Since the observer is not added yet, we need to trigger an update manually.
            super.set(calculateVisibilityConstraints());
        }
        return super.addObserver(obs);
    }

    @Override
    public void removeObserver(Callback<Integer> obs) {
        super.removeObserver(obs);
        if (!hasObservers()) {
            // One of the delegates can be activity-scoped and live longer than e.g. a tab-scoped
            // observer. Unsubscribe when the last observer goes away, to keep the behavior
            // consistent with using the wrapped delegate directly, and to prevent memory leaks.
            for (int i = 0; i < mDelegates.size(); i++) {
                mDelegates.get(i).removeObserver(mConstraintsUpdatedCallback);
            }
        }
    }

    @Override
    public Integer get() {
        // When there are no observers, we don't actively update the set() value and calculate a
        // fresh value on demand.
        if (!hasObservers()) {
            return calculateVisibilityConstraints();
        }
        return super.get();
    }

    @Override
    public void set(Integer value) {
        // Allow set(...) to only be called via the super constructor.  After initial construction,
        // no client should be allowed to update the value through anything other than the
        // attached delegates.
        if (mSetDisabled) {
            throw new IllegalStateException("Calling set on the composed delegate is not allowed.");
        }
        super.set(value);
    }

    private @BrowserControlsState int calculateVisibilityConstraints() {
        boolean shouldBeShown = false;
        for (int i = 0; i < mDelegates.size(); i++) {
            @BrowserControlsState int delegateConstraints = mDelegates.get(i).get();
            if (delegateConstraints == BrowserControlsState.HIDDEN) {
                return BrowserControlsState.HIDDEN;
            }
            shouldBeShown |= delegateConstraints == BrowserControlsState.SHOWN;
        }
        return shouldBeShown ? BrowserControlsState.SHOWN : BrowserControlsState.BOTH;
    }
}
