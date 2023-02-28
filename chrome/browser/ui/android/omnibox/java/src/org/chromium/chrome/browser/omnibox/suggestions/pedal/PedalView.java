// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.content.Context;
import android.graphics.Rect;
import android.view.KeyEvent;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * Container view for the {@link ChipView}.
 * Chips should be initially horizontally aligned with the Content view and stretch to the end of
 * the encompassing BaseSuggestionView
 */
public class PedalView extends RecyclerView {
    private @Nullable PedalViewAdapter mAdapter;

    /**
     * Constructs a new pedal view.
     *
     * @param context The context used to construct the chip view.
     */
    public PedalView(Context context) {
        super(context);

        setItemAnimator(null);
        setLayoutManager(new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
        setMinimumHeight(getResources().getDimensionPixelSize(
                R.dimen.omnibox_action_chips_container_height));

        final @Px int actionChipSpacing =
                getResources().getDimensionPixelSize(R.dimen.omnibox_action_chip_spacing);
        addItemDecoration(new ItemDecoration() {
            @Override
            public void getItemOffsets(
                    Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
                outRect.right = actionChipSpacing / 2;
                outRect.left = actionChipSpacing / 2;
            }
        });

        setPaddingRelative(0, 0, 0,
                getResources().getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_semicompact_padding));
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        assert mAdapter != null;

        if (event.getKeyCode() == KeyEvent.KEYCODE_TAB) {
            if (event.isShiftPressed()) {
                mAdapter.selectPreviousItem();
            } else {
                mAdapter.selectNextItem();
            }
            return true;
        } else if (KeyNavigationUtil.isEnter(event)) {
            var chip = mAdapter.getSelectedView();
            if (chip != null) return chip.performClick();
        }

        return super.onKeyDown(keyCode, event);
    }

    public void setAdapter(@Nullable PedalViewAdapter adapter) {
        super.setAdapter(adapter);
        mAdapter = adapter;
    }

    @Override
    public void setSelected(boolean isSelected) {
        mAdapter.resetSelection();
    }
}
