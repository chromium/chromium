// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.content.Context;
import android.view.KeyEvent;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.SimpleVerticalLayoutView;
import org.chromium.components.browser_ui.widget.chips.ChipView;

import java.util.ArrayList;
import java.util.List;

/**
 * Base layout for pedals suggestion types. This is a {@link BaseSuggestionView} with a pedal under
 * it.
 *
 * @param <T> The type of View being wrapped by BaseSuggestionView.
 */
public class PedalSuggestionView<T extends View> extends SimpleVerticalLayoutView {
    private final @NonNull BaseSuggestionView<T> mBaseSuggestionView;
    private final @NonNull List<PedalView> mPedalList;

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

        mPedalList = new ArrayList<>();
        PedalView pedal = new PedalView(getContext());
        final @Px int pedalSuggestionSizePx = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_pedal_suggestion_pedal_height);
        final @Px int pedalStartPaddingPx =
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_icon_area_size);
        pedal.setPaddingRelative(pedalStartPaddingPx, 0, 0, 0);
        pedal.getChipView().setMinimumHeight(pedalSuggestionSizePx);
        addView(pedal);
        mPedalList.add(pedal);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // TODO(crbug/1411871): implement proper keyboard navigation for actions.
        if (mPedalList.size() > 0) {
            return mPedalList.get(0).onKeyDown(keyCode, event) || super.onKeyDown(keyCode, event);
        }
        return false;
    }

    /** @return base suggestion view. */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public BaseSuggestionView<T> getBaseSuggestionView() {
        return mBaseSuggestionView;
    }

    /** @return The Primary TextView in the pedal view. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public TextView getPedalTextView() {
        return mPedalList.get(0).getPedalTextView();
    }

    /** @return The {@link ChipView} in this view. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public ChipView getPedalChipView() {
        return mPedalList.get(0).getChipView();
    }

    /** @return The {@link PedalView} in this view. */
    PedalView getPedalView() {
        return mPedalList.get(0);
    }
}
