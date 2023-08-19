// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** ModelListAdapter for Omnibox Suggestion Action Chips. */
public class ActionChipsAdapter extends SimpleRecyclerViewAdapter {
    @VisibleForTesting
    private int mSelectedItem = RecyclerView.NO_POSITION;
    private LayoutManager mLayoutManager;

    public ActionChipsAdapter(ModelList data) {
        super(data);
    }

    @Override
    public void onAttachedToRecyclerView(@NonNull RecyclerView view) {
        super.onAttachedToRecyclerView(view);
        mLayoutManager = view.getLayoutManager();
        mSelectedItem = RecyclerView.NO_POSITION;
    }

    /**
     * Ensures selection is reset, with no views being highlighted.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void resetSelection() {
        setSelectedItem(RecyclerView.NO_POSITION);
    }

    /**
     * Retrieve currently selected element.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public @Nullable View getSelectedView() {
        return mLayoutManager.findViewByPosition(mSelectedItem);
    }

    /**
     * Move focus to the next view, if possible, otherwise clear all selections.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void selectNextItem() {
        // Rotate: Nothing -> First chip -> ... -> Last chip -> Nothing.
        var nextItem = 0;
        if (mSelectedItem != RecyclerView.NO_POSITION) nextItem = mSelectedItem + 1;
        if (nextItem >= getItemCount()) nextItem = RecyclerView.NO_POSITION;
        setSelectedItem(nextItem);
    }

    /**
     * Move focus to the previous view, if possible, otherwise clear all selections.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void selectPreviousItem() {
        // Rotate: Nothing -> Last chip -> ... -> First chip -> Nothing.
        var prevItem = RecyclerView.NO_POSITION;
        if (mSelectedItem > 0) prevItem = mSelectedItem - 1;
        if (mSelectedItem == RecyclerView.NO_POSITION) prevItem = getItemCount() - 1;
        setSelectedItem(prevItem);
    }

    /**
     * Move focus to another view, ensuring that the view is fully visible.
     *
     * @param index The index of a view to select, or RecyclerView.NO_POSITION to clear selection.
     */
    @VisibleForTesting
    public void setSelectedItem(int index) {
        assert index >= RecyclerView.NO_POSITION && index < getItemCount();
        assert mLayoutManager != null;

        View previousSelectedView = getSelectedView();
        if (previousSelectedView != null) previousSelectedView.setSelected(false);

        mSelectedItem = index;
        mLayoutManager.scrollToPosition(index);

        View currentSelectedView = getSelectedView();
        if (currentSelectedView != null) currentSelectedView.setSelected(true);
    }

    public int getSelectedItemForTesting() {
        return mSelectedItem;
    }
}
