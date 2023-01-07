// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.content.Context;
import android.view.KeyEvent;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.view.MarginLayoutParamsCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.SimpleHorizontalLayoutView;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.util.ColorUtils;

/**
 * Container view for the {@link ChipView}.
 * This view is for aligning the chip with contentview in BaseSuggestionView.
 * Padding in front of the chip need to be same as the icon in BaseSuggestionView, and Padding at
 * the end of the chip need to be same as the action in BaseSuggestionView.
 */
public class PedalView extends SimpleHorizontalLayoutView {
    private static final String TAG = "PedalView";

    private final @NonNull ChipView mPedal;

    /**
     * Constructs a new pedal view.
     *
     * @param context The context used to construct the chip view.
     */
    public PedalView(Context context) {
        super(context);

        setFocusable(true);

        mPedal = new ChipView(context, R.style.OmniboxPedalChipThemeOverlay);
        mPedal.setLayoutParams(LayoutParams.forDynamicView());

        final int baseColor = MaterialColors.getColor(context, R.attr.colorSurface, TAG);
        final int overlayColor = MaterialColors.getColor(context, R.attr.colorOutline, TAG);
        final float alpha =
                ResourcesCompat.getFloat(context.getResources(), R.dimen.chip_outline_alpha);
        mPedal.setBorder(context.getResources().getDimensionPixelSize(R.dimen.chip_border_width),
                ColorUtils.getColorWithOverlay(baseColor, overlayColor, alpha));

        addView(mPedal);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_TAB) {
            mPedal.setSelected(!mPedal.isSelected());
            return true;
        } else if (KeyNavigationUtil.isEnter(event) && mPedal.isSelected()) {
            return mPedal.performClick();
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    public void setSelected(boolean isSelected) {
        super.setSelected(isSelected);
        mPedal.setSelected(false);
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        MarginLayoutParams marginLayoutParams = (MarginLayoutParams) getLayoutParams();

        final int widthPx = MeasureSpec.getSize(widthSpec)
                - MarginLayoutParamsCompat.getMarginEnd(marginLayoutParams);
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

    @Override
    public boolean isFocused() {
        return super.isFocused() || (isSelected() && !isInTouchMode());
    }
}
