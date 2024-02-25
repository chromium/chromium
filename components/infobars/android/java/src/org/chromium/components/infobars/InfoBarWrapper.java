// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.infobars;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.components.browser_ui.styles.ChromeColors;

/** Layout that holds an infobar's contents and provides a background color and a top shadow. */
class InfoBarWrapper extends FrameLayout {
    private final InfoBarUiItem mItem;

    /** Constructor for inflating from Java. */
    InfoBarWrapper(Context context, InfoBarUiItem item) {
        super(context);
        mItem = item;
        Resources res = context.getResources();
        int peekingHeight = res.getDimensionPixelSize(R.dimen.infobar_peeking_height);
        int shadowHeight = res.getDimensionPixelSize(R.dimen.infobar_shadow_height);
        setMinimumHeight(peekingHeight + shadowHeight);

        // setBackgroundResource() changes the padding, so call setPadding() second.
        setBackgroundResource(R.drawable.infobar_wrapper_bg);
        setPadding(0, shadowHeight, 0, 0);

        LayerDrawable layerDrawable = (LayerDrawable) getBackground();
        ColorDrawable colorDrawable =
                (ColorDrawable) layerDrawable.findDrawableByLayerId(R.id.infobar_wrapper_bg_fill);
        colorDrawable.mutate();
        colorDrawable.setColor(
                ChromeColors.getSurfaceColor(getContext(), R.dimen.infobar_elevation));
    }

    InfoBarUiItem getItem() {
        return mItem;
    }

    @Override
    public void onViewAdded(View child) {
        child.setLayoutParams(
                new LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.TOP));
    }
}
