// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_button;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.listmenu.ListMenuButton;

/** The home button. */
@NullMarked
public class HomeButton extends ListMenuButton {
    private boolean mIsInitialized;
    private int mVisibility;
    private boolean mHasSpaceToShow;

    public HomeButton(Context context, AttributeSet attrs) {
        super(context, attrs);
        mHasSpaceToShow = true;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent e = TraceEvent.scoped("HomeButton.onMeasure")) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        try (TraceEvent e = TraceEvent.scoped("HomeButton.onLayout")) {
            super.onLayout(changed, left, top, right, bottom);
        }
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIsInitialized = true;
        mVisibility = getVisibility();
        // Call with cached value in case it was set before the view was inflated.
        setHasSpaceToShow(mHasSpaceToShow);
    }

    @Override
    public void setVisibility(int visibility) {
        mVisibility = visibility;
        super.setVisibility(mHasSpaceToShow ? mVisibility : GONE);
    }

    /**
     * Sets whether there is enough space for the button to be shown.
     *
     * @param hasSpaceToShow indicates whether the button view has space to show.
     */
    public void setHasSpaceToShow(boolean hasSpaceToShow) {
        mHasSpaceToShow = hasSpaceToShow;
        // This may be called before the view is initialized. If so, hold off until the view is
        // inflated.
        if (mIsInitialized) {
            setVisibility(mVisibility);
        }
    }
}
