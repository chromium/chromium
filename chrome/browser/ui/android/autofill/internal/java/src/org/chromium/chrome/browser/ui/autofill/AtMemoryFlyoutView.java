// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.constraintlayout.helper.widget.Flow;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.browser_ui.widget.chips.ChipView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** View wrapper for the flyout screen of the @memory bottom sheet. */
@NullMarked
public class AtMemoryFlyoutView extends LinearLayout {
    private ConstraintLayout mChipsContainer;
    private Flow mChipsFlow;
    private View mBackButton;
    private TextView mTitleView;
    private TextView mSourceTextView;
    private TextView mManageButton;

    private final List<ChipView> mActiveChips = new ArrayList<>();

    private @Nullable Callback<Integer> mSuggestionClickListener;
    private final View.OnLayoutChangeListener mChipsLayoutListener =
            (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                alignFlyoutChipHeights(v, right - left, oldRight - oldLeft);
            };

    public AtMemoryFlyoutView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mChipsContainer = findViewById(R.id.flyout_chips_container);
        mChipsFlow = findViewById(R.id.chips_flow);
        mBackButton = findViewById(R.id.flyout_back_button);
        mTitleView = findViewById(R.id.flyout_title);
        mManageButton = findViewById(R.id.flyout_manage_button);
        mSourceTextView = findViewById(R.id.flyout_source_text);
        mChipsContainer.addOnLayoutChangeListener(mChipsLayoutListener);
    }

    public void setTitle(@Nullable String title) {
        mTitleView.setText(title);
    }

    public void setSuggestions(List<AutofillSuggestion> suggestions) {
        resetViews();

        List<Integer> chipViewIds = new ArrayList<>();

        for (int index = 0; index < suggestions.size(); index++) {
            AutofillSuggestion suggestion = suggestions.get(index);
            switch (suggestion.getSuggestionType()) {
                case SuggestionType.MANAGE_ADDRESS:
                case SuggestionType.MANAGE_AUTOFILL_AI:
                case SuggestionType.MANAGE_AUTOFILL_AI_IDENTITY_DOCS:
                case SuggestionType.MANAGE_AUTOFILL_AI_TRAVEL:
                case SuggestionType.MANAGE_AUTOFILL_AI_SHOPPING:
                case SuggestionType.MANAGE_CREDIT_CARD:
                case SuggestionType.MANAGE_IBAN:
                case SuggestionType.MANAGE_LOYALTY_CARD:
                case SuggestionType.MANAGE_ENHANCED_AUTOFILL:
                    setUpManageButton(suggestion, index);
                    break;
                case SuggestionType.AT_MEMORY_SOURCE_ATTRIBUTION:
                    setUpSourceTextView(suggestion);
                    break;
                case SuggestionType.AT_MEMORY_SEARCH_RESULT:
                    setUpChip(suggestion, index, chipViewIds);
                    break;
                default:
                    break;
            }
        }

        mChipsFlow.setReferencedIds(toIntArray(chipViewIds));
        mChipsContainer.setVisibility(chipViewIds.isEmpty() ? View.GONE : View.VISIBLE);
    }

    private void resetViews() {
        for (ChipView chip : mActiveChips) {
            mChipsContainer.removeView(chip);
        }
        mActiveChips.clear();
        mSourceTextView.setVisibility(View.GONE);
        mManageButton.setVisibility(View.GONE);
        mManageButton.setOnClickListener(null);
    }

    private void setUpSourceTextView(AutofillSuggestion suggestion) {
        mSourceTextView.setText(suggestion.getLabel());
        mSourceTextView.setVisibility(View.VISIBLE);
    }

    private void setUpManageButton(AutofillSuggestion suggestion, int index) {
        mManageButton.setText(suggestion.getLabel());
        mManageButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                suggestion.getIconId(), 0, 0, 0);
        mManageButton.setVisibility(View.VISIBLE);
        mManageButton.setOnClickListener(
                v -> {
                    if (mSuggestionClickListener != null) {
                        mSuggestionClickListener.onResult(index);
                    }
                });
    }

    private void setUpChip(AutofillSuggestion suggestion, int index, List<Integer> chipViewIds) {
        ChipView chip = createFlyoutChipView(mChipsContainer, suggestion, index);
        chipViewIds.add(chip.getId());
        mChipsContainer.addView(chip);
        mActiveChips.add(chip);
    }

    public void setBackClickListener(Runnable onClickListener) {
        mBackButton.setOnClickListener(v -> onClickListener.run());
    }

    public void setSuggestionClickListener(Callback<Integer> onClickListener) {
        mSuggestionClickListener = onClickListener;
    }

    private ChipView createFlyoutChipView(
            ViewGroup parent, AutofillSuggestion suggestion, int position) {
        Context context = parent.getContext();
        ChipView chip =
                (ChipView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.at_memory_flyout_chip,
                                        parent,
                                        /* attachToRoot= */ false);
        chip.setId(View.generateViewId());

        TextView primaryTextView = chip.getPrimaryTextView();
        primaryTextView.setText(suggestion.getLabel());

        TextView secondaryTextView = chip.getSecondaryTextView();
        if (!TextUtils.isEmpty(suggestion.getSublabel())) {
            secondaryTextView.setText(suggestion.getSublabel());
            secondaryTextView.setVisibility(View.VISIBLE);
        } else {
            secondaryTextView.setVisibility(View.GONE);
        }

        chip.setOnClickListener(
                v -> {
                    if (mSuggestionClickListener != null) {
                        mSuggestionClickListener.onResult(position);
                    }
                });

        return chip;
    }

    private void alignFlyoutChipHeights(View container, int newWidth, int oldWidth) {
        container.removeOnLayoutChangeListener(mChipsLayoutListener);
        // If the Activity resizes horizontally (e.g., screen rotation or multi-window mode),
        // reset all chip minimum heights to 0 so Flow can re-pack them naturally into new rows.
        if (newWidth != oldWidth && oldWidth != 0) {
            for (ChipView chip : mActiveChips) {
                chip.setMinimumHeight(0);
            }
        } else {
            // Once horizontal container dimensions stabilize, equalize chip heights per row.
            Map<Integer, List<ChipView>> rows = new HashMap<>();
            for (ChipView chip : mActiveChips) {
                rows.computeIfAbsent(chip.getTop(), k -> new ArrayList<>()).add(chip);
            }
            for (List<ChipView> row : rows.values()) {
                int maxHeight = 0;
                for (ChipView chip : row) {
                    maxHeight = Math.max(maxHeight, chip.getHeight());
                }
                for (ChipView chip : row) {
                    if (chip.getMinimumHeight() != maxHeight) {
                        chip.setMinimumHeight(maxHeight);
                    }
                }
            }
        }
        container.addOnLayoutChangeListener(mChipsLayoutListener);
    }

    private int[] toIntArray(List<Integer> list) {
        int[] array = new int[list.size()];
        for (int i = 0; i < list.size(); i++) {
            array[i] = list.get(i);
        }
        return array;
    }
}
