// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.content.Context;
import android.view.KeyEvent;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.content.res.ResourcesCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.util.ColorUtils;

/**
 * Container view for the {@link ChipView}.
 * Chips should be initially horizontally aligned with the Content view and stretch to the end of
 * the encompassing BaseSuggestionView
 */
public class PedalView extends FrameLayout {
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

        final int baseColor = MaterialColors.getColor(context, R.attr.colorSurface, TAG);
        final int overlayColor = MaterialColors.getColor(context, R.attr.colorOutline, TAG);
        final float alpha =
                ResourcesCompat.getFloat(context.getResources(), R.dimen.chip_outline_alpha);
        mPedal.setBorder(context.getResources().getDimensionPixelSize(R.dimen.chip_border_width),
                ColorUtils.getColorWithOverlay(baseColor, overlayColor, alpha));

        var layoutParams =
                new FrameLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        layoutParams.setMargins(0, 0, 0,
                getResources().getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_semicompact_padding));

        addView(mPedal, layoutParams);
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
