// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.chips;

import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.widget.ChipView;

/** The {@link ViewHolder} responsible for reflecting a {@link Chip} to a {@link View}. */
public class ChipsViewHolder extends ViewHolder {
    /** Builds a ChipsViewHolder around a specific {@link View}. */
    private ChipsViewHolder(View itemView) {
        super(itemView);
    }

    private ChipView getChipView() {
        assert itemView
                instanceof ChipView : "ChipViewHolder doesn't hold ChipView but "
                                      + itemView.getClass();
        return (ChipView) itemView;
    }

    /**
     * Used as a method reference for ViewHolderFactory.
     * @see RecyclerViewAdapter
     *         .ViewHolderFactory#createViewHolder
     */
    public static ChipsViewHolder create(ViewGroup parent, int viewType) {
        assert viewType == 0;
        return new ChipsViewHolder(
                new ChipView(parent.getContext(), R.style.SuggestionChipThemeOverlay));
    }

    /**
     * Used as a method reference for ViewBinder, to push the properties of {@code chip} to
     * {@link #itemView}.
     * @param chip The {@link Chip} to visually reflect in the stored {@link View}.
     * @see SimpleRecyclerViewMcp.ViewBinder#onBindViewHolder
     */
    public void bind(Chip chip) {
        getChipView().setEnabled(chip.enabled);
        getChipView().setSelected(chip.selected);
        getChipView().setOnClickListener(v -> chip.chipSelectedListener.run());
        // Set the text if it has been provided in one form or another.
        TextView primaryTextView = getChipView().getPrimaryTextView();
        if (chip.rawText != null) {
            primaryTextView.setText(chip.rawText);
        } else if (chip.text != Chip.INVALID_STRING_RES_ID) {
            primaryTextView.setText(chip.text);
        } else {
            primaryTextView.setText("");
        }
        // Set the icon if it's been provided - if selected we use the "check" icon.
        if (chip.icon != Chip.INVALID_ICON_ID || chip.selected) {
            getChipView().setIcon(
                    chip.selected ? R.drawable.ic_check_googblue_24dp : chip.icon, true);
        } else {
            getChipView().setIcon(Chip.INVALID_ICON_ID, false);
        }
        primaryTextView.setContentDescription(chip.contentDescription);
    }
}
