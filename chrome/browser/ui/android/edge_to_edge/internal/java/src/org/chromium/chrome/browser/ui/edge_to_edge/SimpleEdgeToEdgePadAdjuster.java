// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.view.View;
import android.widget.ScrollView;

/**
 * A simple implementation of {@link EdgeToEdgePadAdjuster} which can add a padding when e2e is on
 * and then remove when it is off.
 */
public class SimpleEdgeToEdgePadAdjuster implements EdgeToEdgePadAdjuster {

    private final View mViewToPad;
    private final int mDefaultBottomPadding;
    private final boolean mEnableClipToPadding;

    /**
     * @param view The view that needs padding at the bottom.
     * @param enableClipToPadding Whether enable #setClipToPadding for compatible views (e.g
     *     ScrollView).
     */
    public SimpleEdgeToEdgePadAdjuster(View view, boolean enableClipToPadding) {
        mViewToPad = view;
        mEnableClipToPadding = enableClipToPadding;
        mDefaultBottomPadding = mViewToPad.getPaddingBottom();
    }

    @Override
    public void overrideBottomInset(int inset) {
        if (mEnableClipToPadding && (mViewToPad instanceof ScrollView)) {
            ((ScrollView) mViewToPad).setClipToPadding(inset == 0);
        }

        mViewToPad.setPadding(
                mViewToPad.getPaddingLeft(),
                mViewToPad.getPaddingTop(),
                mViewToPad.getPaddingRight(),
                mDefaultBottomPadding + inset);
    }
}
