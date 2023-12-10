// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import org.chromium.base.Callback;
import org.chromium.cc.input.BrowserControlsState;

import java.util.ArrayList;
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
        mDelegates = new ArrayList<>();
        mConstraintsUpdatedCallback = (constraints) -> super.set(calculateVisibilityConstraints());
        for (int i = 0; i < delegates.length; i++) addDelegate(delegates[i]);
        super.set(calculateVisibilityConstraints());
    }

    /**
     * Adds an additional delegate to the composed visibility delegate that will determine the
     * overall visibility constraints.
     * @param delegate The delegate to be added.
     */
    public void addDelegate(BrowserControlsVisibilityDelegate delegate) {
        mDelegates.add(delegate);
        delegate.addObserver(mConstraintsUpdatedCallback);
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
