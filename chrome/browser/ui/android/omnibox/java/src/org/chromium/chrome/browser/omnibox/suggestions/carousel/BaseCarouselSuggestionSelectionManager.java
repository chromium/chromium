// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

/**
 * Selection manager for BaseCarouselSuggestion.
 */
public class BaseCarouselSuggestionSelectionManager
        implements RecyclerView.OnChildAttachStateChangeListener {
    private int mSelectedItem = RecyclerView.NO_POSITION;
    private LayoutManager mLayoutManager;

    BaseCarouselSuggestionSelectionManager(LayoutManager layoutManager) {
        mLayoutManager = layoutManager;
    }

    @Override
    public void onChildViewAttachedToWindow(View view) {
        // Force update selection of the view that might come from a recycle pool.
        setSelectedItem(mSelectedItem, true);
    }

    @Override
    public void onChildViewDetachedFromWindow(View view) {
        // Force move selection to the item that now occupies slot at mSelectedItem.
        // The explicit state set here is necessary, because the setSelectedItem call
        // does not iterate over all available views, so when this view is re-used,
        // we do not want it to show up as selected right away.
        view.setSelected(false);
        setSelectedItem(mSelectedItem, true);
    }

    /**
     * Reset the active selection.
     */
    void resetSelection() {
        setSelectedItem(RecyclerView.NO_POSITION, false);
    }

    /**
     * Move selection to the next element on the list.
     */
    void selectNextItem() {
        if (mLayoutManager == null) return;

        int newSelectedItem;
        if (mSelectedItem == RecyclerView.NO_POSITION) {
            newSelectedItem = 0;
        } else if (mSelectedItem < mLayoutManager.getItemCount()) {
            newSelectedItem = mSelectedItem + 1;
        } else {
            newSelectedItem = mLayoutManager.getItemCount() - 1;
        }

        setSelectedItem(newSelectedItem, false);
    }

    /**
     * Move selection to the previous element on the list.
     */
    void selectPreviousItem() {
        if (mLayoutManager == null) return;

        int newSelectedItem;
        if (mSelectedItem == RecyclerView.NO_POSITION) {
            newSelectedItem = mLayoutManager.getItemCount() - 1;
        } else if (mSelectedItem > 0) {
            newSelectedItem = mSelectedItem - 1;
        } else {
            newSelectedItem = 0;
        }

        setSelectedItem(newSelectedItem, false);
    }

    /**
     * Move focus to another view.
     *
     * @param index Index of the child view to be selected.
     * @param force Whether to apply the selection even if the current selected item has not
     *         changed.
     */
    void setSelectedItem(int index, boolean force) {
        if (mLayoutManager == null) return;
        if (index != RecyclerView.NO_POSITION
                && (index < 0 || index >= mLayoutManager.getItemCount())) {
            return;
        }
        if (!force && (index == mSelectedItem)) return;

        View previousSelectedView = mLayoutManager.findViewByPosition(mSelectedItem);
        if (previousSelectedView != null) {
            previousSelectedView.setSelected(false);
        }

        mSelectedItem = index;
        mLayoutManager.scrollToPosition(index);

        View currentSelectedView = mLayoutManager.findViewByPosition(index);
        if (currentSelectedView != null) {
            currentSelectedView.setSelected(true);
        }
    }

    /**
     * Returns the selected item index.
     */
    int getSelectedItemForTest() {
        return mSelectedItem;
    }
}
