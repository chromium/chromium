// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.widget.ScrollView;

import androidx.annotation.Nullable;

/** Content view class for close confirmation dialog. */
public class CloseConfirmationDialogView extends ScrollView {
    public CloseConfirmationDialogView(Context context) {
        super(context);
    }

    public CloseConfirmationDialogView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // This fixed-width view may be wider than the screen and clipped off. Adjust the width
        // to avoid this by taking the screen width into account.
        Resources res = getResources();
        int screenWidth = res.getDisplayMetrics().widthPixels;
        int defaultWidth = MeasureSpec.getSize(widthMeasureSpec);
        int sideMargin = res.getDimensionPixelSize(R.dimen.confirmation_dialog_side_margin) * 2;
        int width = Math.min(defaultWidth, screenWidth - sideMargin);
        super.onMeasure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), heightMeasureSpec);
    }
}
