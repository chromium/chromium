// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.content.Context;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.SimpleVerticalLayoutView;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * Base layout for pedals suggestion types. This is a {@link BaseSuggestionView} with a pedal under
 * it.
 *
 * @param <T> The type of View being wrapped by BaseSuggestionView.
 */
public class PedalSuggestionView<T extends View> extends SimpleVerticalLayoutView {
    private final @NonNull BaseSuggestionView<T> mBaseSuggestionView;
    private final @NonNull PedalView mPedal;

    /**
     * Constructs a new suggestion view and inflates supplied layout as the contents view.
     *
     * @param context The context used to construct the suggestion view.
     * @param layoutId Layout ID to be inflated as the BaseSuggestionView.
     */
    public PedalSuggestionView(Context context, @LayoutRes int layoutId) {
        super(context);
        mBaseSuggestionView = new BaseSuggestionView<T>(context, layoutId);
        addView(mBaseSuggestionView);

        mPedal = new PedalView(getContext());
        int pedalStartPaddingPx =
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_icon_area_size);
        int pedalEndPaddingPx =
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_action_icon_width);
        mPedal.setPaddingRelative(pedalStartPaddingPx, 0, pedalEndPaddingPx, 0);
        addView(mPedal);
    }

    /** @return base suggestion view. */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public BaseSuggestionView<T> getBaseSuggestionView() {
        return mBaseSuggestionView;
    }

    /** @return The Primary TextView in the pedal view. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public TextView getPedalTextView() {
        return mPedal.getPedalTextView();
    }

    /** @return The {@link ChipView} in this view. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public ChipView getPedalChipView() {
        return mPedal.getChipView();
    }
}
