// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.dragreorder;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.util.SparseArray;
import android.util.SparseBooleanArray;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.ColorUtils;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ObserverList;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * MVC-compatible adapter for a {@link RecyclerView} that manages drag-reorderable lists. Used in
 * same way as {@link SimpleRecyclerViewAdapter} with a few exceptions. In addition to
 * registerType, there are two additional ways to register types:
 * 1. registerDraggableType: This method registers a view that can be dragged. It takes one
 *    additional argument - a single-function interface, DragBinder, which connects some UI event
 *    (e.g. a touch on a drag handle) to the adapter.
 * 2. registerPassivelyDraggableType: This method registers a view that can be dragged over, but
 *    isn't itself draggable. The call is identical to SimpleRecyclerViewAdapter#registerType
 *    but it keeps track of the view to make it available to drag over later.
 */
public class DragReorderableRecyclerViewAdapter extends SimpleRecyclerViewAdapter {
    private static final int ANIMATION_DELAY_MS = 100;

    /**
     * Responsible for binding draggable views to the items adapter. The viewHolder should add a
     * listener to the correct view (e.g. a drag handle) which informs the ItemTouchHandler that
     * dragging has begun. Refer to the android docs for an example:
     * https://developer.android.com/reference/androidx/recyclerview/widget/ItemTouchHelper#startDrag(androidx.recyclerview.widget.RecyclerView.ViewHolder)
     */
    public interface DragBinder {
        void bind(ViewHolder viewHolder, ItemTouchHelper itemTouchHelper);
    }

    /** Responsible for deciding when long-press drag is enabled. */
    public interface LongPressDragDelegate {
        boolean isLongPressDragEnabled();
    }

    /** Keep a reference to the underlying RecyclerView to attach the drag/drop helpers. */
    private RecyclerView mRecyclerView;
    private boolean mDragEnabled;
    private int mStart;

    /** Classes to handle drag/drop functionality. */
    private final ItemTouchHelper.Callback mTouchHelperCallback = new DragTouchCallback();
    private final ItemTouchHelper mItemTouchHelper = new ItemTouchHelper(mTouchHelperCallback);
    private final ObserverList<DragListener> mListeners = new ObserverList<>();
    /** A map of view types to view binders. */
    private final SparseArray<DragBinder> mDragBinderMap = new SparseArray<>();
    /** Tracks which views are passively draggable. */
    private final SparseBooleanArray mPassiveDragMap = new SparseBooleanArray();
    /** Styles to use while dragging */
    private final int mDraggedBackgroundColor;
    private final float mDraggedElevation;
    private final LongPressDragDelegate mLongPressDragDelegate;

    /**
     * A callback for touch actions on drag-reorderable lists.
     */
    private class DragTouchCallback extends ItemTouchHelper.Callback {
        // The view that is being dragged now; null means no view is being dragged now;
        private @Nullable RecyclerView.ViewHolder mBeingDragged;

        @Override
        public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
            int pos = viewHolder.getAdapterPosition();
            int dragFlags = 0;

            // This method may be called multiple times until the view is dropped
            // ensure there is only one bookmark being dragged.
            if ((mBeingDragged == viewHolder || mBeingDragged == null)
                    && isActivelyDraggable(mListData.get(pos).type)) {
                dragFlags = ItemTouchHelper.UP | ItemTouchHelper.DOWN;
            }
            return makeMovementFlags(dragFlags, /*swipeFlags=*/0);
        }

        @Override
        public boolean onMove(RecyclerView recyclerView, RecyclerView.ViewHolder current,
                RecyclerView.ViewHolder target) {
            int from = current.getBindingAdapterPosition();
            int to = target.getBindingAdapterPosition();
            if (from == to) return false;
            mListData.swap(from, to);
            notifyItemMoved(from, to);
            return true;
        }

        @Override
        public void onSelectedChanged(RecyclerView.ViewHolder viewHolder, int actionState) {
            super.onSelectedChanged(viewHolder, actionState);
            // similar to getMovementFlags, this method may be called multiple times
            if (actionState == ItemTouchHelper.ACTION_STATE_DRAG && mBeingDragged != viewHolder) {
                mBeingDragged = viewHolder;
                mStart = viewHolder.getAdapterPosition();
                onDragStateChange(true);
                updateVisualState(true, viewHolder);
            }
        }

        @Override
        public void clearView(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
            super.clearView(recyclerView, viewHolder);
            // We might need to use an explicit call here.
            // the row has been dropped, even though it is possible at same row
            mBeingDragged = null;
            onDragStateChange(false);
            updateVisualState(false, viewHolder);
        }

        @Override
        public boolean isLongPressDragEnabled() {
            return mLongPressDragDelegate.isLongPressDragEnabled();
        }

        @Override
        public boolean isItemViewSwipeEnabled() {
            return false;
        }

        @Override
        public void onSwiped(RecyclerView.ViewHolder viewHolder, int direction) {}

        @Override
        public boolean canDropOver(RecyclerView recyclerView, RecyclerView.ViewHolder current,
                RecyclerView.ViewHolder target) {
            int currentPos = current.getBindingAdapterPosition();
            int targetPos = target.getBindingAdapterPosition();
            // The fact that current is being dragged is proof enough since draggable views are
            // also passively draggable.
            return isPassivelyDraggable(mListData.get(currentPos).type)
                    && isPassivelyDraggable(mListData.get(targetPos).type);
        }

        /**
         * Update the visual state of this row.
         *
         * @param dragged    Whether this row is currently being dragged.
         * @param viewHolder The DraggableRowViewHolder that is holding this row's content.
         */
        private void updateVisualState(boolean dragged, RecyclerView.ViewHolder viewHolder) {
            // Animate background colors and elevations
            ViewCompat.animate(viewHolder.itemView)
                    .translationZ(dragged ? mDraggedElevation : 0)
                    .withEndAction(
                            ()
                                    -> viewHolder.itemView.setBackgroundColor(
                                            dragged ? mDraggedBackgroundColor : Color.TRANSPARENT))
                    .setDuration(ANIMATION_DELAY_MS)
                    .start();
        }
    }

    /**
     * Listens to drag actions in a drag-reorderable list.
     */
    public interface DragListener {
        /**
         * Called when drag starts or ends.
         *
         * @param drag True iff drag is currently on.
         */
        void onDragStateChange(boolean drag);
    }

    /**
     * @param context The context for that this DragReorderableRecyclerViewAdapter occupies.
     * @param modelList The {@link ModelList} which determines what's shown in the list.
     * @param longPressDragDelegate The delegate which decides when long press dragging is enabled.
     */
    public DragReorderableRecyclerViewAdapter(
            Context context, ModelList modelList, LongPressDragDelegate longPressDragDelegate) {
        super(modelList);

        Resources resource = context.getResources();
        // Set the alpha to 90% when dragging which is 230/255
        mDraggedBackgroundColor = ColorUtils.setAlphaComponent(
                ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_1),
                resource.getInteger(R.integer.list_item_dragged_alpha));
        mDraggedElevation = resource.getDimension(R.dimen.list_item_dragged_elevation);
        mLongPressDragDelegate = longPressDragDelegate;
    }

    /**
     * Registers a view that can be dragged. It takes one additional argument - a single-function
     * interface, DragBinder, which connects some UI event (e.g. a touch on a drag handle) to the
     * adapter.
     * @param typeId The ID of the view type. This should not match any other view type registered
     *               in this adapter.
     * @param builder A mechanism for building new views of the specified type.
     * @param binder A means of binding a model to the provided view.
     * @param dragBinder A means of binding the view to Android's drag system.
     */
    public <T extends View> void registerDraggableType(int typeId, ViewBuilder<T> builder,
            ViewBinder<PropertyModel, T, PropertyKey> binder, DragBinder dragBinder) {
        super.registerType(typeId, builder, binder);
        assert mDragBinderMap.get(typeId) == null;
        mDragBinderMap.put(typeId, dragBinder);
    }

    /**
     * Registers a view that can be dragged over, but isn't itself draggable. The call is identical
     * to SimpleRecyclerViewAdapter#registerType but it keeps track of the view to make it
     * available to drag over later.
     */
    public <T extends View> void registerPassivelyDraggableType(
            int typeId, ViewBuilder<T> builder, ViewBinder<PropertyModel, T, PropertyKey> binder) {
        super.registerType(typeId, builder, binder);
        assert !mPassiveDragMap.get(typeId);
        mPassiveDragMap.put(typeId, true);
    }

    // Drag/drop helper functions.

    /**
     * Enables drag & drop interaction on the RecyclerView that this adapter is attached to.
     */
    public void enableDrag() {
        mDragEnabled = true;
        mItemTouchHelper.attachToRecyclerView(mRecyclerView);
    }

    /**
     * Disables drag & drop interaction.
     */
    public void disableDrag() {
        mDragEnabled = false;
        mItemTouchHelper.attachToRecyclerView(null);
    }

    /**
     * @param l The drag listener to be added.
     */
    public void addDragListener(DragListener l) {
        mListeners.addObserver(l);
    }

    /**
     * @param l The drag listener to be removed.
     */
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

    private boolean isActivelyDraggable(int type) {
        return mDragBinderMap.get(type) != null;
    }

    private boolean isPassivelyDraggable(int type) {
        return isActivelyDraggable(type) || mPassiveDragMap.get(type);
    }

    // RecyclerView implementation.

    @Override
    public void onBindViewHolder(ViewHolder viewHolder, int position) {
        super.onBindViewHolder(viewHolder, position);
        int typeId = mListData.get(position).type;
        // Overridden to given the draggable items a chance to bind correctly since a ViewHolder
        // is required.
        if (mDragEnabled && isActivelyDraggable(typeId)) {
            mDragBinderMap.get(typeId).bind(viewHolder, mItemTouchHelper);
        }
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
     * Simulate a drag. All items that are involved in the drag must be visible (no scrolling).
     *
     * @param start The index of the ViewHolder that you want to drag.
     * @param end The index this ViewHolder should be dragged to and dropped at.
     */
    @VisibleForTesting
    public void simulateDragForTests(int start, int end) {
        RecyclerView.ViewHolder viewHolder = mRecyclerView.findViewHolderForAdapterPosition(start);
        mItemTouchHelper.startDrag(viewHolder);
        int increment = start < end ? 1 : -1;
        int i = start;
        while (i != end) {
            i += increment;
            if (!mTouchHelperCallback.canDropOver(mRecyclerView, viewHolder,
                        mRecyclerView.findViewHolderForAdapterPosition(i))) {
                break;
            }
            mTouchHelperCallback.onMove(
                    mRecyclerView, viewHolder, mRecyclerView.findViewHolderForAdapterPosition(i));
        }
        mTouchHelperCallback.onSelectedChanged(viewHolder, ItemTouchHelper.ACTION_STATE_IDLE);
        mTouchHelperCallback.clearView(mRecyclerView, viewHolder);
    }
}
