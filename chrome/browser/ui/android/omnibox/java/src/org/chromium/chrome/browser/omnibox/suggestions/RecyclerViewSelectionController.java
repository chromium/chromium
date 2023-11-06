// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

/** Selection manager for RecyclerViews. */
public class RecyclerViewSelectionController
        implements RecyclerView.OnChildAttachStateChangeListener {
    private int mSelectedItem = RecyclerView.NO_POSITION;
    private LayoutManager mLayoutManager;

    /** When true, cycling to next/previous item will go through null selection. */
    private boolean mCycleThroughNoSelection;

    public RecyclerViewSelectionController(LayoutManager layoutManager) {
        mLayoutManager = layoutManager;
    }

    /**
     * Specifies whether advancing to previous/next element should go through no selection.
     *
     * <p>Note that advancing from no selection always proceeds to the next element:
     *
     * <p>- If the flag is set to `false`, once the last possible element is selected, the user
     * cannot advance any further.
     *
     * <pre>[A] -> [B] -> [C] -> [C] -> [C] -> [C] ...</pre>
     *
     * <p>- if the flag is set to `true`, once the last possible element is selected and the user
     * advances to the next item, selection will re-start from no selection, and advance to the
     * first selectable item afterwards.
     *
     * <pre>[A] -> [B] -> [C] -> [∅] -> [A] -> [B] ...</pre>
     */
    public void setCycleThroughNoSelection(boolean cycleThroughNoSelection) {
        mCycleThroughNoSelection = cycleThroughNoSelection;
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

    /** Reset the active selection. */
    public void resetSelection() {
        setSelectedItem(RecyclerView.NO_POSITION, false);
    }

    /** Move selection to the next element on the list. */
    public void selectNextItem() {
        if (mLayoutManager == null) return;

        int newSelectedItem;
        if (mSelectedItem == RecyclerView.NO_POSITION) {
            newSelectedItem = 0;
        } else if (mSelectedItem < mLayoutManager.getItemCount() - 1) {
            newSelectedItem = mSelectedItem + 1;
        } else if (mCycleThroughNoSelection) {
            newSelectedItem = RecyclerView.NO_POSITION;
        } else {
            newSelectedItem = mLayoutManager.getItemCount() - 1;
        }

        setSelectedItem(newSelectedItem, false);
    }

    /** Move selection to the previous element on the list. */
    public void selectPreviousItem() {
        if (mLayoutManager == null) return;

        int newSelectedItem;
        if (mSelectedItem == RecyclerView.NO_POSITION) {
            newSelectedItem = mLayoutManager.getItemCount() - 1;
        } else if (mSelectedItem > 0) {
            newSelectedItem = mSelectedItem - 1;
        } else if (mCycleThroughNoSelection) {
            newSelectedItem = RecyclerView.NO_POSITION;
        } else {
            newSelectedItem = 0;
        }

        setSelectedItem(newSelectedItem, false);
    }

    /** Retrieve currently selected element. */
    @Nullable
    public View getSelectedView() {
        return mLayoutManager.findViewByPosition(mSelectedItem);
    }

    /**
     * Move focus to another view.
     *
     * @param index Index of the child view to be selected.
     * @param force Whether to apply the selection even if the current selected item has not
     *     changed.
     */
    @VisibleForTesting
    public void setSelectedItem(int index, boolean force) {
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

    /** Returns the selected item index. */
    @VisibleForTesting
    int getSelectedItemForTest() {
        return mSelectedItem;
    }
}
