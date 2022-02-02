// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.content.Context;
import android.widget.TextView;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.suggestions.base.SimpleHorizontalLayoutView;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * Container view for the {@link ChipView}.
 * This view is for aligning the chip with contentview in BaseSuggestionView.
 * Padding in front of the chip need to be same as the icon in BaseSuggestionView, and Padding at
 * the end of the chip need to be same as the action in BaseSuggestionView.
 */
public class PedalView extends SimpleHorizontalLayoutView {
    private final @NonNull ChipView mPedal;

    /**
     * Constructs a new pedal view.
     *
     * @param context The context used to construct the chip view.
     */
    public PedalView(Context context) {
        super(context);

        mPedal = new ChipView(context, null);
        mPedal.setLayoutParams(LayoutParams.forDynamicView());
        addView(mPedal);
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        final int widthPx = MeasureSpec.getSize(widthSpec);
        int chipViewWidth = widthPx - getPaddingLeft() - getPaddingRight();

        // Measure height of the content view given the width constraint.
        mPedal.measure(MeasureSpec.makeMeasureSpec(chipViewWidth, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int heightPx = mPedal.getMeasuredHeight();

        setMeasuredDimension(widthPx,
                MeasureSpec.makeMeasureSpec(
                        heightPx + getPaddingTop() + getPaddingBottom(), MeasureSpec.EXACTLY));
    }

    /** @return The Primary TextView in this view. */
    TextView getPedalTextView() {
        return mPedal.getPrimaryTextView();
    }

    /** @return The {@link ChipView} in this view. */
    ChipView getChipView() {
        return mPedal;
    }
}
