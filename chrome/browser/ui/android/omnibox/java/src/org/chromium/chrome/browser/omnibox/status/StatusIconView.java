// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.Space;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.base.ViewUtils;

/** StatusIconView is a custom view displaying the status icon in the location bar. */
public class StatusIconView extends LinearLayout {
    private View mIconViewFrame;
    private Space mStatusIconHoldingSpace;

    public StatusIconView(Context context, AttributeSet attributes) {
        super(context, attributes);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIconViewFrame = findViewById(R.id.location_bar_status_icon_frame);
        mStatusIconHoldingSpace = findViewById(R.id.location_bar_status_icon_holding_space);
    }

    @Override
    public void setVisibility(int visibility) {
        int iconViewFrameVisibility = getIconVisibility();
        if (iconViewFrameVisibility != visibility) {
            boolean wasLayoutPreviouslyRequested = isLayoutRequested();
            mIconViewFrame.setVisibility(visibility);
            ViewUtils.requestLayout(this, "StatusIconView setVisibility");
            // If the icon's visibility changes while layout is pending, we can end up in a bad
            // state due to a stale measurement cache. Post a task to request layout to force this
            // visibility change (https://crbug.com/1345552, https://crbug.com/1399457).
            if (wasLayoutPreviouslyRequested && getHandler() != null) {
                getHandler()
                        .post(
                                () ->
                                        ViewUtils.requestLayout(
                                                this, "StatusIconView.setVisibility Runnable"));
            }
        }
        // The holding space should be only be VISIBLE if the icon is GONE. The size should be (the
        // background size - the icon size).
        mStatusIconHoldingSpace.setVisibility(visibility == View.GONE ? View.VISIBLE : View.GONE);
    }

    /** return the status icon's visibility. */
    int getIconVisibility() {
        return mIconViewFrame.getVisibility();
    }
}
