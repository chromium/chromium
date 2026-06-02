// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.constraintlayout.helper.widget.Flow;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.widget.chips.ChipView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** View wrapper for the AtMemory Flyout sheet. */
@NullMarked
class AtMemoryFlyoutView {
    private final View mContentView;
    private final View mBackButton;
    private final TextView mTitleView;
    private final TextView mSourceButton;
    private final TextView mManageButton;
    private final ConstraintLayout mChipsContainer;
    private final Flow mChipsFlow;
    private final List<ChipView> mActiveChips = new ArrayList<>();
    private @MonotonicNonNull Callback<AutofillSuggestion> mSuggestionClickedCallback;

    private final View.OnLayoutChangeListener mChipsLayoutListener =
            new View.OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(
                        View v,
                        int left,
                        int top,
                        int right,
                        int bottom,
                        int oldLeft,
                        int oldTop,
                        int oldRight,
                        int oldBottom) {
                    v.removeOnLayoutChangeListener(this);
                    alignChipHeights();
                    v.addOnLayoutChangeListener(this);
                }
            };

    AtMemoryFlyoutView(Context context) {
        mContentView =
                LayoutInflater.from(context).inflate(R.layout.at_memory_flyout_bottom_sheet, null);
        mBackButton = mContentView.findViewById(R.id.flyout_back_button);
        mTitleView = mContentView.findViewById(R.id.flyout_title);
        mSourceButton = mContentView.findViewById(R.id.flyout_source_button);
        mManageButton = mContentView.findViewById(R.id.flyout_manage_button);
        mChipsContainer = mContentView.findViewById(R.id.flyout_chips_container);
        mChipsFlow = mContentView.findViewById(R.id.chips_flow);

        mChipsContainer.addOnLayoutChangeListener(mChipsLayoutListener);
    }

    View getContentView() {
        return mContentView;
    }

    void setTitle(String title) {
        mTitleView.setText(title);
    }

    void setSourceText(String sourceText) {
        mSourceButton.setText(sourceText);
    }

    void setBackButtonCallback(Runnable callback) {
        mBackButton.setOnClickListener(v -> callback.run());
    }

    void setSourceClickCallback(Runnable callback) {
        mSourceButton.setOnClickListener(v -> callback.run());
    }

    void setManageClickCallback(Runnable callback) {
        mManageButton.setOnClickListener(v -> callback.run());
    }

    void setSuggestionClickCallback(Callback<AutofillSuggestion> callback) {
        mSuggestionClickedCallback = callback;
    }

    void setSuggestions(List<AutofillSuggestion> suggestions) {
        // Remove old chips.
        for (ChipView chip : mActiveChips) {
            mChipsContainer.removeView(chip);
        }
        mActiveChips.clear();

        int[] ids = new int[suggestions.size()];
        Context context = mContentView.getContext();
        for (int i = 0; i < suggestions.size(); i++) {
            AutofillSuggestion suggestion = suggestions.get(i);
            ChipView chip = createChipView(context, mChipsContainer, suggestion);
            ids[i] = chip.getId();
            mChipsContainer.addView(chip);
            mActiveChips.add(chip);
        }
        mChipsFlow.setReferencedIds(ids);
    }

    private ChipView createChipView(
            Context context, ViewGroup parent, AutofillSuggestion suggestion) {
        ChipView chip =
                (ChipView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.at_memory_flyout_chip,
                                        parent,
                                        /* attachToRoot= */ false);
        int id = View.generateViewId();
        chip.setId(id);

        chip.getPrimaryTextView().setText(suggestion.getLabel());

        chip.setOnClickListener(
                v -> assumeNonNull(mSuggestionClickedCallback).onResult(suggestion));

        if (!suggestion.getSublabel().isEmpty()) {
            TextView secondaryTextView = chip.getSecondaryTextView();
            secondaryTextView.setText(suggestion.getSublabel());
            secondaryTextView.setVisibility(View.VISIBLE);
        }

        int paddingVerticalPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.at_memory_flyout_chip_padding_vertical);
        int paddingHorizontalPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.at_memory_flyout_default_margin);

        chip.setPaddingRelative(
                paddingHorizontalPx,
                chip.getPaddingTop(),
                paddingHorizontalPx,
                chip.getPaddingBottom());

        TextView primaryText = chip.getPrimaryTextView();
        primaryText.setPaddingRelative(
                primaryText.getPaddingStart(),
                paddingVerticalPx,
                primaryText.getPaddingEnd(),
                suggestion.getSublabel().isEmpty() ? paddingVerticalPx : 0);

        if (!suggestion.getSublabel().isEmpty()) {
            TextView secondaryText = chip.getSecondaryTextView();
            secondaryText.setPaddingRelative(
                    secondaryText.getPaddingStart(),
                    0,
                    secondaryText.getPaddingEnd(),
                    paddingVerticalPx);
        }

        return chip;
    }

    private void alignChipHeights() {
        Map<Integer, List<ChipView>> rows = new HashMap<>();
        for (ChipView chip : mActiveChips) {
            int topCoord = chip.getTop();
            List<ChipView> row = rows.get(topCoord);
            if (row == null) {
                row = new ArrayList<>();
                rows.put(topCoord, row);
            }
            row.add(chip);
        }

        for (List<ChipView> row : rows.values()) {
            int maxHeight = 0;
            for (ChipView chip : row) {
                maxHeight = Math.max(maxHeight, chip.getHeight());
            }
            for (ChipView chip : row) {
                chip.setMinimumHeight(maxHeight);
            }
        }
    }
}
