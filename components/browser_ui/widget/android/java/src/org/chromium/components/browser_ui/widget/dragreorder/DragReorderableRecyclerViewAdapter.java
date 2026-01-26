// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.dragreorder;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.SparseArray;
import android.view.View;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.dragreorder.DragTouchHandler.DraggabilityProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * MVC-compatible adapter for a {@link RecyclerView} that manages drag-reorderable lists. Used in
 * same way as {@link SimpleRecyclerViewAdapter} with a few exceptions. In addition to registerType,
 * there are two additional ways to register types:
 *
 * <pre>
 * 1. registerDraggableType: This method registers a view that can be dragged. It takes one
 *    additional argument - a single-function interface, DragBinder, which connects some UI event
 *    (e.g. a touch on a drag handle) to the adapter.
 * 2. registerPassivelyDraggableType: This method registers a view that can be dragged over, but
 *    isn't itself draggable. The call is identical to SimpleRecyclerViewAdapter#registerType
 *    but it keeps track of the view to make it available to drag over later.
 * </pre>
 */
@NullMarked
public class DragReorderableRecyclerViewAdapter extends SimpleRecyclerViewAdapter {
    /**
     * Responsible for binding draggable views to the items adapter. The viewHolder should add a
     * listener to the correct view (e.g. a drag handle) which informs the ItemTouchHandler that
     * dragging has begun. Refer to the android docs for an example:
     * https://developer.android.com/reference/androidx/recyclerview/widget/ItemTouchHelper#startDrag(androidx.recyclerview.widget.RecyclerView.ViewHolder)
     */
    public interface DragBinder {
        void bind(ViewHolder viewHolder, ItemTouchHelper itemTouchHelper);
    }

    /** Keep a reference to the underlying RecyclerView to attach the drag/drop helpers. */
    private @Nullable RecyclerView mRecyclerView;

    /** Classes to handle drag/drop functionality. */
    private final DragTouchHandler mDragTouchHandler;

    private final ItemTouchHelper mItemTouchHelper;

    /** A map of view types to view binders. */
    private final SparseArray<DragBinder> mDragBinderMap = new SparseArray<>();

    /**
     * @param context The context for that this DragReorderableRecyclerViewAdapter occupies.
     * @param modelList The {@link ModelList} which determines what's shown in the list.
     * @param handler The {@link DragTouchHandler} that handles touch events.
     */
    public DragReorderableRecyclerViewAdapter(
            Context context, ModelList modelList, DragTouchHandler handler) {
        super(modelList);

        mDragTouchHandler = handler;
        mItemTouchHelper = new ItemTouchHelper(mDragTouchHandler);
    }

    /**
     * Registers a view that can be dragged. It takes one additional argument - a single-function
     * interface, DragBinder, which connects some UI event (e.g. a touch on a drag handle) to the
     * adapter.
     *
     * @param typeId The ID of the view type. This should not match any other view type registered
     *     in this adapter.
     * @param builder A mechanism for building new views of the specified type.
     * @param binder A means of binding a model to the provided view.
     * @param dragBinder A means of binding the view to Android's drag system.
     * @param draggabilityProvider A way of resolving if a given row is draggable.
     */
    public <T extends View> void registerDraggableType(
            int typeId,
            ViewBuilder<T> builder,
            ViewBinder<PropertyModel, T, PropertyKey> binder,
            DragBinder dragBinder,
            DraggabilityProvider draggabilityProvider) {
        super.registerType(typeId, builder, binder);

        assert mDragBinderMap.get(typeId) == null;
        mDragBinderMap.put(typeId, dragBinder);

        mDragTouchHandler.registerDraggableType(typeId, draggabilityProvider);
    }

    // Drag/drop helper functions.

    /** Enables drag & drop interaction on the RecyclerView that this adapter is attached to. */
    public void enableDrag() {
        mDragTouchHandler.setDragEnabled(true);
        mItemTouchHelper.attachToRecyclerView(mRecyclerView);
    }

    /** Disables drag & drop interaction. */
    public void disableDrag() {
        mDragTouchHandler.setDragEnabled(false);
        mItemTouchHelper.attachToRecyclerView(null);
    }

    // RecyclerView implementation.

    @Override
    public void onBindViewHolder(ViewHolder viewHolder, int position) {
        super.onBindViewHolder(viewHolder, position);
        int typeId = mListData.get(position).type;

        // If this view was previously used during a drag (and drifted off-screen), it might still
        // have the "closed hand" cursor set, so we reset it to a default cursor.
        if (ChromeFeatureList.sAndroidBookmarkBarFastFollow.isEnabled()) {
            viewHolder.itemView.setPointerIcon(null);
        }

        // Overridden to given the draggable items a chance to bind correctly since a ViewHolder
        // is required.
        DragBinder dragBinder = mDragBinderMap.get(typeId);
        if (dragBinder != null) {
            dragBinder.bind(viewHolder, mItemTouchHelper);
        }
    }

    @Override
    public void onAttachedToRecyclerView(RecyclerView recyclerView) {
        mRecyclerView = recyclerView;
        mDragTouchHandler.setRecyclerView(recyclerView);
    }

    @Override
    public void onDetachedFromRecyclerView(RecyclerView recyclerView) {
        mRecyclerView = null;
        mDragTouchHandler.setRecyclerView(null);
    }

    public DragTouchHandler getDragTouchHandlerForTest() {
        return mDragTouchHandler;
    }

    /**
     * Simulate a drag. All items that are involved in the drag must be visible (no scrolling).
     *
     * @param start The index of the ViewHolder that you want to drag.
     * @param end The index this ViewHolder should be dragged to and dropped at.
     */
    public void simulateDragForTests(int start, int end) {
        assumeNonNull(mRecyclerView);
        RecyclerView.ViewHolder viewHolder = mRecyclerView.findViewHolderForAdapterPosition(start);
        assert viewHolder != null;
        mItemTouchHelper.startDrag(viewHolder);
        int increment = start < end ? 1 : -1;
        int i = start;
        while (i != end) {
            i += increment;
            RecyclerView.ViewHolder nextViewHolder =
                    mRecyclerView.findViewHolderForAdapterPosition(i);
            assert nextViewHolder != null;
            if (!mDragTouchHandler.canDropOver(mRecyclerView, viewHolder, nextViewHolder)) {
                break;
            }
            mDragTouchHandler.onMove(mRecyclerView, viewHolder, nextViewHolder);
        }
        mDragTouchHandler.onSelectedChanged(viewHolder, ItemTouchHelper.ACTION_STATE_IDLE);
        mDragTouchHandler.clearView(mRecyclerView, viewHolder);
    }
}
