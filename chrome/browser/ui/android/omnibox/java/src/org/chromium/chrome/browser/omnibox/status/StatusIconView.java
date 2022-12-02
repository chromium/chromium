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

/**
 * StatusIconView is a custom view displaying the status icon in the location bar.
 */
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
        mIconViewFrame.setVisibility(visibility);
        // The holding space should be only be VISIBLE if the icon is GONE. The size should be (the
        // background size - the icon size).
        mStatusIconHoldingSpace.setVisibility(visibility == View.GONE ? View.VISIBLE : View.GONE);
    }

    /**
     * return the status icon's visibility.
     */
    int getIconVisibility() {
        return mIconViewFrame.getVisibility();
    }
}
