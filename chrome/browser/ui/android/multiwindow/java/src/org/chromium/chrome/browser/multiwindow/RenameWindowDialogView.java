// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.widget.ScrollView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Content view class for rename window dialog. */
@NullMarked
public class RenameWindowDialogView extends ScrollView {
    public RenameWindowDialogView(Context context) {
        super(context);
    }

    public RenameWindowDialogView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Adjust the width to appropriately fit the dialog content in the parent view.
        Resources res = getResources();
        int screenWidth = res.getDisplayMetrics().widthPixels;
        int defaultWidth = MeasureSpec.getSize(widthMeasureSpec);
        int sideMargin = res.getDimensionPixelSize(R.dimen.confirmation_dialog_side_margin) * 2;
        int width = Math.min(defaultWidth, screenWidth - sideMargin);
        super.onMeasure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), heightMeasureSpec);
    }
}
