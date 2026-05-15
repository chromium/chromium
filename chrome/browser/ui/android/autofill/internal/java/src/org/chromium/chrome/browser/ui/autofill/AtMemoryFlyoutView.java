// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.constraintlayout.helper.widget.Flow;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** View wrapper for the AtMemory Flyout sheet. */
@NullMarked
class AtMemoryFlyoutView {
    private final View mContentView;
    private final TextView mTitleView;
    private final TextView mSourceView;
    private final ConstraintLayout mChipsContainer;
    private final Flow mChipsFlow;
    private final List<ChipView> mActiveChips = new ArrayList<>();

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
        mTitleView = mContentView.findViewById(R.id.flyout_title);
        mSourceView = mContentView.findViewById(R.id.flyout_source_row);
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
        mSourceView.setText(sourceText);
    }

    void setChipsData(List<Pair<String, String>> chipsData) {
        // Remove old chips.
        for (ChipView chip : mActiveChips) {
            mChipsContainer.removeView(chip);
        }
        mActiveChips.clear();

        int[] ids = new int[chipsData.size()];
        Context context = mContentView.getContext();
        int i = 0;
        for (Pair<String, String> suggestion : chipsData) {
            ChipView chip = createChipView(context, mChipsContainer, suggestion);
            ids[i] = chip.getId();
            mChipsContainer.addView(chip);
            mActiveChips.add(chip);
            i++;
        }
        mChipsFlow.setReferencedIds(ids);
    }

    private ChipView createChipView(Context context, ViewGroup parent, Pair<String, String> data) {
        ChipView chip =
                (ChipView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.at_memory_flyout_chip, parent, false);
        int id = View.generateViewId();
        chip.setId(id);

        chip.getPrimaryTextView().setText(data.first);

        if (data.second != null && !data.second.isEmpty()) {
            TextView secondaryTextView = chip.getSecondaryTextView();
            secondaryTextView.setText(data.second);
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
                data.second == null || data.second.isEmpty() ? paddingVerticalPx : 0);

        if (data.second != null && !data.second.isEmpty()) {
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
