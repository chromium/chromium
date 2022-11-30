// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.app.ActionBar;

/**
 * An {@link ActionModeController.ActionBarDelegate} that shifts a view as the action bar appears.
 */
public class ViewShiftingActionBarDelegate implements ActionModeController.ActionBarDelegate {
    /** The action bar which this delegate works for. */
    private final ActionBar mActionBar;

    /** The view that will be shifted as the action bar appears. */
    private final View mShiftingView;

    /** Action bar background view. */
    private final View mBackgroundView;

    /**
     * @param actionBar The {@link ActionBar} in use.
     * @param shiftingView The view that will shift when the action bar appears.
     * @param backgroundView Background view for shadow effect.
     */
    public ViewShiftingActionBarDelegate(
            ActionBar actionBar, View shiftingView, View backgroundView) {
        mActionBar = actionBar;
        mShiftingView = shiftingView;
        mBackgroundView = backgroundView;
    }

    @Override
    public void setControlTopMargin(int margin) {
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) mShiftingView.getLayoutParams();
        lp.topMargin = margin;
        mShiftingView.setLayoutParams(lp);
    }

    @Override
    public int getControlTopMargin() {
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) mShiftingView.getLayoutParams();
        return lp.topMargin;
    }

    @Override
    public ActionBar getSupportActionBar() {
        return mActionBar;
    }

    @Override
    public void setActionBarBackgroundVisibility(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        mBackgroundView.setVisibility(visibility);
        // TODO(tedchoc): Add support for changing the color based on the brand color.
    }
}
