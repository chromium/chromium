// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Map;
import java.util.TreeMap;

/** Selection manager for RecyclerViews. */
@NullMarked
public class RecyclerViewSelectionController extends SelectionController
        implements RecyclerView.OnChildAttachStateChangeListener {
    private static final int ADVANCE_EXPOSE_VIEWS = 2;
    private final LayoutManager mLayoutManager;
    private int mLastSelectedItemLogicalIndex = RecyclerView.NO_POSITION;

    // Tracks the logical indices and callbacks of views that don't exist in the RecyclerView
    // adapter.
    // A virtual view is a view that exists outside the RecyclerView adapter.
    // A logical index can refer to either a real view in the adapter or a virtual view.
    // In the presence of virtual views, logical indices are not equal to real indices and must be
    // mapped back to real indices by calling getRealIndexInLayoutManager
    // A real index refers to a real view in the adapter.
    // Kept sorted to make it easier to calculate the offset between a logical index and an
    // index-in-layout-manager.
    private final Map<Integer, Callback<Boolean>> mVirtualViews = new TreeMap<>();

    public RecyclerViewSelectionController(
            LayoutManager layoutManager, @SelectionController.Mode int mode) {
        super(mode);
        mLayoutManager = layoutManager;
        reset();
    }

    @Override
    protected int getItemCount() {
        return mLayoutManager.getItemCount() + mVirtualViews.size();
    }

    @Override
    public void reset() {
        super.reset();
        if (isParkedAtSentinel()) {
            mLastSelectedItemLogicalIndex = RecyclerView.NO_POSITION;
        }
    }

    @Override
    boolean setPosition(int newPosition) {
        boolean retVal = super.setPosition(newPosition);
        if (isParkedAtSentinel()) {
            mLastSelectedItemLogicalIndex = RecyclerView.NO_POSITION;
        }
        return retVal;
    }

    @Override
    public void onChildViewAttachedToWindow(View view) {
        // Force update selection of the view that might come from a recycle pool.
        setItemState(mLastSelectedItemLogicalIndex, true);
    }

    @Override
    public void onChildViewDetachedFromWindow(View view) {
        // Force move selection to the item that now occupies slot at mLastSelectedItemIndex.
        // The explicit state set here is necessary, because the setSelectedItem call
        // does not iterate over all available views, so when this view is re-used,
        // we do not want it to show up as selected right away.
        view.setSelected(false);
        setItemState(mLastSelectedItemLogicalIndex, true);
    }

    /** Retrieve currently selected element. */
    public @Nullable View getSelectedView() {
        Integer selection = getPosition();
        if (selection == null) return null;
        int realIndex = getRealIndexInLayoutManager(selection);
        if (realIndex == RecyclerView.NO_POSITION) return null;

        return mLayoutManager.findViewByPosition(realIndex);
    }

    /** Returns whether item at specific position is focusable. */
    @Override
    protected boolean isSelectableItem(int logicalIndex) {
        // Virtual views are assumed to be selectable destinations.
        if (mVirtualViews.containsKey(logicalIndex)) return true;

        int realIndex = getRealIndexInLayoutManager(logicalIndex);
        if (realIndex == RecyclerView.NO_POSITION) return false;

        View view = mLayoutManager.findViewByPosition(realIndex);
        return realIndex == 0
                || realIndex == (mLayoutManager.getItemCount() - 1)
                || (view != null && view.isFocusable());
    }

    /**
     * Change the selected state of a view.
     *
     * @param logicalIndex index of the child view to be selected
     * @param isSelected the desired selected state of the view
     */
    @VisibleForTesting
    @Override
    public void setItemState(int logicalIndex, boolean isSelected) {
        int realIndex = getRealIndexInLayoutManager(logicalIndex);
        // Only attempt to mutate RecyclerView children if it's a real adapter index.
        if (realIndex != RecyclerView.NO_POSITION) {
            View view = mLayoutManager.findViewByPosition(realIndex);
            if (view != null) {
                view.setSelected(isSelected);
            }
        } else {
            // If it's a virtual view, notify the corresponding callback.
            Callback<Boolean> callback = mVirtualViews.get(logicalIndex);
            if (callback != null) {
                callback.onResult(isSelected);
            }
        }

        // Ensure additional views are exposed when tabbing through items.
        if (isSelected && mLastSelectedItemLogicalIndex != logicalIndex) {
            // Map the target to a real index for LayoutManager scrolling.
            int realIndexOfPreviousItem = logicalIndex - getVirtualViewOffset(logicalIndex) - 1;
            int scrollTargetRealIndex =
                    realIndex == RecyclerView.NO_POSITION
                            ? Math.max(0, realIndexOfPreviousItem)
                            : realIndex;

            int exposeUntilViewRealIndex =
                    (mLastSelectedItemLogicalIndex < logicalIndex)
                            ? Math.min(
                                    scrollTargetRealIndex + ADVANCE_EXPOSE_VIEWS,
                                    mLayoutManager.getItemCount() - 1)
                            : Math.max(scrollTargetRealIndex - ADVANCE_EXPOSE_VIEWS, 0);

            mLayoutManager.scrollToPosition(exposeUntilViewRealIndex);
            mLastSelectedItemLogicalIndex = logicalIndex;
        }
    }

    /** Injects a virtual view into the logical list at the specified logical index. */
    public void addVirtualView(int logicalIndex, Callback<Boolean> virtualViewSelectionCallback) {
        mVirtualViews.put(logicalIndex, virtualViewSelectionCallback);
    }

    /** Removes a virtual view from the logical list. */
    public void removeVirtualView(int logicalIndex) {
        mVirtualViews.remove(logicalIndex);
    }

    /**
     * Translates a logical list index into a real LayoutManager adapter index.
     *
     * @param logicalIndex The index including virtual views.
     * @return The real adapter index, or NO_POSITION if the logical index points to a virtual view.
     */
    private int getRealIndexInLayoutManager(int logicalIndex) {
        if (mVirtualViews.containsKey(logicalIndex)) return RecyclerView.NO_POSITION;

        return logicalIndex - getVirtualViewOffset(logicalIndex);
    }

    private int getVirtualViewOffset(int logicalIndex) {
        int offset = 0;
        for (int virtualViewIndex : mVirtualViews.keySet()) {
            if (virtualViewIndex < logicalIndex) {
                offset++;
            } else {
                break;
            }
        }
        return offset;
    }
}
