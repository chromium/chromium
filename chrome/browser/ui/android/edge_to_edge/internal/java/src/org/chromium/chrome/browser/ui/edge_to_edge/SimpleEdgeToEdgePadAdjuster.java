// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.view.View;

/**
 * A simple implementation of {@link EdgeToEdgePadAdjuster} which can add a padding when e2e is on
 * and then remove when it is off.
 */
public class SimpleEdgeToEdgePadAdjuster implements EdgeToEdgePadAdjuster {

    private final View mViewToPad;
    private final int mDefaultBottomPadding;
    private final boolean mAccountForBrowserControls;

    public SimpleEdgeToEdgePadAdjuster(View view, boolean accountForBrowserControls) {
        mViewToPad = view;
        mDefaultBottomPadding = mViewToPad.getPaddingBottom();
        mAccountForBrowserControls = accountForBrowserControls;
    }

    @Override
    public void overrideBottomInset(int defaultInset, int insetWithBrowserControls) {
        int inset = mAccountForBrowserControls ? insetWithBrowserControls : defaultInset;
        mViewToPad.setPadding(
                mViewToPad.getPaddingLeft(),
                mViewToPad.getPaddingTop(),
                mViewToPad.getPaddingRight(),
                mDefaultBottomPadding + inset);
    }
}
