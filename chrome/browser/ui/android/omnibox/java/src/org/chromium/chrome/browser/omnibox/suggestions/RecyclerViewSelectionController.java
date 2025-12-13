// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Selection manager for RecyclerViews. */
@NullMarked
public class RecyclerViewSelectionController extends SelectionController
        implements RecyclerView.OnChildAttachStateChangeListener {
    private static final int ADVANCE_EXPOSE_VIEWS = 2;
    private final LayoutManager mLayoutManager;
    private int mLastSelectedItemIndex = RecyclerView.NO_POSITION;

    public RecyclerViewSelectionController(
            LayoutManager layoutManager, @SelectionController.Mode int mode) {
        super(mode);
        mLayoutManager = layoutManager;
        reset();
    }

    @Override
    protected int getItemCount() {
        return mLayoutManager.getItemCount();
    }

    @Override
    public void reset() {
        super.reset();
        mLastSelectedItemIndex = RecyclerView.NO_POSITION;
    }

    @Override
    public void onChildViewAttachedToWindow(View view) {
        // Force update selection of the view that might come from a recycle pool.
        setItemState(mLastSelectedItemIndex, true);
    }

    @Override
    public void onChildViewDetachedFromWindow(View view) {
        // Force move selection to the item that now occupies slot at mLastSelectedItemIndex.
        // The explicit state set here is necessary, because the setSelectedItem call
        // does not iterate over all available views, so when this view is re-used,
        // we do not want it to show up as selected right away.
        view.setSelected(false);
        setItemState(mLastSelectedItemIndex, true);
    }

    /** Retrieve currently selected element. */
    public @Nullable View getSelectedView() {
        Integer selection = getPosition();
        if (selection == null) return null;
        return mLayoutManager.findViewByPosition(selection);
    }

    /** Returns whether item at specific position is focusable. */
    @Override
    protected boolean isSelectableItem(int index) {
        View view = mLayoutManager.findViewByPosition(index);
        return view != null && view.isFocusable();
    }

    /**
     * Change the selected state of a view.
     *
     * @param index index of the child view to be selected
     * @param isSelected the desired selected state of the view
     */
    @VisibleForTesting
    @Override
    public void setItemState(int index, boolean isSelected) {
        View view = mLayoutManager.findViewByPosition(index);
        if (view != null) {
            view.setSelected(isSelected);
        }

        // Ensure additional views are exposed when tabbing through items.
        // This has the additional benefit of
        // - presenting the extra views to the user so they don't have to tab through to them to see
        //   them, and
        // - instantiating these extra views, so these can be evaluated when the user tabs through
        //   the list.
        // If we don't expose additional views, the user may occasionally be unable to tab through
        // the list; the LayoutManager may report <null> when we request a view at a specific
        // position, because the view is not yet bound when we need it.
        if (isSelected) {
            int exposeUntilViewIndex =
                    (mLastSelectedItemIndex < index)
                            ? Math.min(
                                    index + ADVANCE_EXPOSE_VIEWS, mLayoutManager.getItemCount() - 1)
                            : Math.max(index - ADVANCE_EXPOSE_VIEWS, 0);
            mLayoutManager.scrollToPosition(exposeUntilViewIndex);
            mLastSelectedItemIndex = index;
        }
    }
}
