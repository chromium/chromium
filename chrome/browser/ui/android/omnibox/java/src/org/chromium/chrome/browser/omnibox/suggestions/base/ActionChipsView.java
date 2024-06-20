// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.view.KeyEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.CheckDiscard;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.RecyclerViewSelectionController;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * Container view for the {@link ChipView}. Chips should be initially horizontally aligned with the
 * Content view and stretch to the end of the encompassing BaseSuggestionView.
 */
public class ActionChipsView extends RecyclerView {
    private @NonNull RecyclerViewSelectionController mSelectionController;

    /**
     * Constructs a new pedal view.
     *
     * @param context The context used to construct the chip view.
     */
    public ActionChipsView(Context context) {
        super(context);

        setItemAnimator(null);
        setId(R.id.omnibox_actions_carousel);
        var layoutManager = new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false);
        setLayoutManager(layoutManager);

        mSelectionController = new RecyclerViewSelectionController(layoutManager);
        mSelectionController.setCycleThroughNoSelection(true);
        addOnChildAttachStateChangeListener(mSelectionController);

        setMinimumHeight(
                getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_action_chips_container_height));
        setPaddingRelative(
                0,
                0,
                0,
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_content_padding));

        final @Px int leadInSpace =
                OmniboxResourceProvider.getSuggestionDecorationIconSizeWidth(context);
        final @Px int elementSpace =
                getResources().getDimensionPixelSize(R.dimen.omnibox_action_chip_spacing);

        addItemDecoration(new SpacingRecyclerViewItemDecoration(leadInSpace, elementSpace));
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_TAB) {
            if (event.isShiftPressed()) {
                return mSelectionController.selectPreviousItem();
            } else {
                return mSelectionController.selectNextItem();
            }
        } else if (KeyNavigationUtil.isEnter(event)) {
            var chip = mSelectionController.getSelectedView();
            if (chip != null) return chip.performClick();
        }

        return superOnKeyDown(keyCode, event);
    }

    /**
     * Proxy calls to super.onKeyDown; call exposed for testing purposes. There is no way to detect
     * calls to super using robolectric.
     */
    @CheckDiscard("Should be inlined except for testing")
    @VisibleForTesting
    public boolean superOnKeyDown(int keyCode, KeyEvent event) {
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public void setSelected(boolean isSelected) {
        mSelectionController.resetSelection();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    void setSelectionControllerForTesting(RecyclerViewSelectionController controller) {
        mSelectionController = controller;
    }

    public void setLeadInSpacing(int spacing) {
        if (getItemDecorationCount() > 0) {
            assert getItemDecorationCount() == 1 : "Expected at most 1 decoration";
            removeItemDecorationAt(0);
        }

        addItemDecoration(
                new SpacingRecyclerViewItemDecoration(
                        spacing,
                        getResources().getDimensionPixelSize(R.dimen.omnibox_action_chip_spacing)));
    }
}
