// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.dragreorder;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;

import androidx.core.graphics.ColorUtils;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.R;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;

/**
 * Adapter for a {@link RecyclerView} that manages drag-reorderable lists.
 *
 * @param <T> The type of item that inhabits this adapter's list
 */
@NullMarked
public abstract class DragReorderableListAdapter<T> extends RecyclerView.Adapter<ViewHolder> {
    // keep track of the list and list managers
    protected @MonotonicNonNull ItemTouchHelper mItemTouchHelper;

    private ItemTouchHelper.@Nullable Callback mTouchHelperCallback;

    protected List<T> mItems = Collections.emptyList();

    protected @Nullable RecyclerView mRecyclerView;

    // keep track of how this item looks
    private final int mDraggedBackgroundColor;
    private final float mDraggedElevation;

    protected DragStateDelegate mDragStateDelegate;

    private int mStart;
    private final ObserverList<DragListener> mListeners = new ObserverList<>();

    /** A callback for touch actions on drag-reorderable lists. */
    private class DragTouchCallback extends ItemTouchHelper.Callback {
        // The view that is being dragged now; null means no view is being dragged now;
        private @Nullable ViewHolder mBeingDragged;

        @Override
        public int getMovementFlags(RecyclerView recyclerView, ViewHolder viewHolder) {
            // this method may be called multiple times until the view is dropped
            // ensure there is only one bookmark being dragged
            if ((mBeingDragged == viewHolder || mBeingDragged == null)
                    && isActivelyDraggable(viewHolder)) {
                return makeMovementFlags(
                        ItemTouchHelper.UP | ItemTouchHelper.DOWN, 0 /* swipe flags */);
            }
            return makeMovementFlags(0, 0);
        }

        @Override
        public boolean onMove(RecyclerView recyclerView, ViewHolder current, ViewHolder target) {
            int from = current.getAdapterPosition();
            int to = target.getAdapterPosition();
            if (from == to) return false;
            Collections.swap(mItems, from, to);
            notifyItemMoved(from, to);
            return true;
        }

        @Override
        public void onSelectedChanged(@Nullable ViewHolder viewHolder, int actionState) {
            super.onSelectedChanged(viewHolder, actionState);

            // similar to getMovementFlags, this method may be called multiple times
            if (viewHolder != null
                    && actionState == ItemTouchHelper.ACTION_STATE_DRAG
                    && mBeingDragged != viewHolder) {
                mBeingDragged = viewHolder;
                mStart = viewHolder.getAdapterPosition();
                onDragStateChange(true);
                updateVisualState(true, viewHolder);
            }
        }

        @Override
        public void clearView(RecyclerView recyclerView, ViewHolder viewHolder) {
            super.clearView(recyclerView, viewHolder);
            // no need to commit change if recycler view is not attached to window, e.g.:
            // dragging is terminated by destroying activity
            if (viewHolder.getAdapterPosition() != mStart && recyclerView.isAttachedToWindow()) {
                // Commit the position change for the dragged item when it's dropped and
                // recyclerView has finished layout computing
                recyclerView.post(() -> setOrder(mItems));
            }
            // the row has been dropped, even though it is possible at same row
            mBeingDragged = null;
            onDragStateChange(false);
            updateVisualState(false, viewHolder);
        }

        @Override
        public boolean isLongPressDragEnabled() {
            return mDragStateDelegate.getDragActive();
        }

        @Override
        public boolean isItemViewSwipeEnabled() {
            return false;
        }

        @Override
        public void onSwiped(ViewHolder viewHolder, int direction) {
            // no-op
        }

        @Override
        public boolean canDropOver(
                RecyclerView recyclerView, ViewHolder current, ViewHolder target) {
            boolean currentDraggable = isPassivelyDraggable(current);
            boolean targetDraggable = isPassivelyDraggable(target);
            return currentDraggable && targetDraggable;
        }

        /**
         * Update the visual state of this row.
         *
         * @param dragged    Whether this row is currently being dragged.
         * @param viewHolder The DraggableRowViewHolder that is holding this row's content.
         */
        private void updateVisualState(boolean dragged, ViewHolder viewHolder) {
            DragUtils.createViewDragAnimation(
                            dragged,
                            viewHolder.itemView,
                            mDraggedBackgroundColor,
                            mDraggedElevation)
                    .start();
        }
    }

    /** Listens to drag actions in a drag-reorderable list. */
    public interface DragListener {
        /**
         * Called when drag starts or ends.
         *
         * @param drag True iff drag is currently on.
         */
        void onDragStateChange(boolean drag);
    }

    /**
     * Construct a DragReorderableListAdapter.
     *
     * @param context The context for that this DragReorderableListAdapter occupies.
     */
    public DragReorderableListAdapter(Context context) {
        Resources resources = context.getResources();
        // Set the alpha to 90% when dragging which is 230/255
        mDraggedBackgroundColor =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getColorSurfaceContainerLow(context),
                        resources.getInteger(R.integer.list_item_dragged_alpha));
        mDraggedElevation = resources.getDimension(R.dimen.list_item_dragged_elevation);
    }

    /**
     * Set the drag state delegate for this adapter.
     *
     * @param delegate The drag state delegate for this adapter.
     */
    @Initializer
    protected void initDragStateDelegate(DragStateDelegate delegate) {
        mDragStateDelegate = delegate;
    }

    @Override
    public int getItemCount() {
        return mItems.size();
    }

    public T getItemByPosition(int position) {
        return mItems.get(position);
    }

    @SuppressWarnings("NotifyDataSetChanged")
    public void setItems(Collection<T> newItems) {
        mItems = new ArrayList<>(newItems);
        notifyDataSetChanged();
    }

    /** Enables drag & drop interaction on the RecyclerView that this adapter is attached to. */
    public void enableDrag() {
        assert mRecyclerView != null;
        if (mItemTouchHelper == null) {
            mTouchHelperCallback = new DragTouchCallback();
            mItemTouchHelper = new ItemTouchHelper(mTouchHelperCallback);
        }
        mItemTouchHelper.attachToRecyclerView(mRecyclerView);
    }

    /** Disables drag & drop interaction. */
    public void disableDrag() {
        if (mItemTouchHelper != null) mItemTouchHelper.attachToRecyclerView(null);
    }

    /**
     * Sets the order of the items in the drag-reorderable list.
     *
     * @param order The new order for the items.
     */
    protected abstract void setOrder(List<T> order);

    /**
     * Returns true iff a drag can start on viewHolder.
     *
     * @param viewHolder The view holder of interest.
     * @return True iff a drag can start on viewHolder.
     */
    protected abstract boolean isActivelyDraggable(ViewHolder viewHolder);

    /**
     * Returns true iff another item can be dragged over viewHolder.
     *
     * @param viewHolder The view holder of interest.
     * @return True iff other items can be dragged over viewHolder.
     */
    protected abstract boolean isPassivelyDraggable(ViewHolder viewHolder);

    /**
     * Get the item inside of a view holder.
     *
     * @param holder The view holder of interest.
     * @return The item contained by holder.
     */
    protected T getItemByHolder(ViewHolder holder) {
        assumeNonNull(mRecyclerView);
        return getItemByPosition(mRecyclerView.getChildLayoutPosition(holder.itemView));
    }

    @Override
    public void onAttachedToRecyclerView(RecyclerView recyclerView) {
        mRecyclerView = recyclerView;
    }

    @Override
    public void onDetachedFromRecyclerView(RecyclerView recyclerView) {
        mRecyclerView = null;
    }

    /**
     * @param l The drag listener to be added.
     */
    public void addDragListener(DragListener l) {
        mListeners.addObserver(l);
    }

    /** @param l The drag listener to be added. */
    public void removeDragListener(DragListener l) {
        mListeners.removeObserver(l);
    }

    /**
     * Called when drag state changes (drag starts / ends), and notifies all listeners.
     *
     * @param drag True iff drag is currently on.
     */
    private void onDragStateChange(boolean drag) {
        for (DragListener l : mListeners) {
            l.onDragStateChange(drag);
        }
    }

    /**
     * Simulate a drag. All items that are involved in the drag must be visible (no scrolling).
     *
     * @param start The index of the ViewHolder that you want to drag.
     * @param end The index this ViewHolder should be dragged to and dropped at.
     */
    @SuppressWarnings("NullAway") // Test-only method with lots of assumptions.
    public void simulateDragForTests(int start, int end) {
        ViewHolder viewHolder = mRecyclerView.findViewHolderForAdapterPosition(start);
        mItemTouchHelper.startDrag(viewHolder);
        int increment = start < end ? 1 : -1;
        int i = start;
        while (i != end) {
            i += increment;
            mTouchHelperCallback.onMove(
                    mRecyclerView, viewHolder, mRecyclerView.findViewHolderForAdapterPosition(i));
        }
        mTouchHelperCallback.onSelectedChanged(viewHolder, ItemTouchHelper.ACTION_STATE_IDLE);
        mTouchHelperCallback.clearView(mRecyclerView, viewHolder);
    }
}
