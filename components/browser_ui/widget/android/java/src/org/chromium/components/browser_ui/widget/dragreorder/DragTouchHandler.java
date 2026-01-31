// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.dragreorder;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.util.SparseArray;
import android.view.PointerIcon;
import android.view.View;

import androidx.core.graphics.ColorUtils;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.BiFunction;

/**
 * A callback for touch actions on drag-reorderable lists to be used by {@link
 * DragReorderableRecyclerViewAdapter}.
 */
@NullMarked
public class DragTouchHandler extends ItemTouchHelper.Callback {

    /** Listens to drag actions in a drag-reorderable list. */
    public interface DragListener {
        /**
         * Called when drag starts or ends.
         *
         * @param drag True iff drag is currently on.
         */
        default void onDragStateChange(boolean drag) {}

        /** Called when a drag ends and it ends up in a swap. */
        default void onSwap(int targetIndex) {}
    }

    /** Controls draggability state on a per item basis. */
    public interface DraggabilityProvider {
        boolean isActivelyDraggable(PropertyModel propertyModel);

        boolean isPassivelyDraggable(PropertyModel propertyModel);
    }

    /** Responsible for deciding when long-press drag is enabled. */
    public interface LongPressDragDelegate {
        boolean isLongPressDragEnabled();
    }

    private @Nullable RecyclerView mRecyclerView;

    private final ModelList mListData;
    private final ObserverList<DragListener> mListeners = new ObserverList<>();

    private boolean mDragEnabled;

    /** A map of view types to active/passive draggability. */
    private final SparseArray<DraggabilityProvider> mDraggabilityProviderMap = new SparseArray<>();

    private @Nullable LongPressDragDelegate mLongPressDragDelegate;
    private boolean mDefaultLongPressDragEnabled = true;

    // The view that is being dragged now; null means no view is being dragged now;
    private RecyclerView.@Nullable ViewHolder mDraggedViewHolder;

    /** Styles to use while dragging */
    private final int mDraggedBackgroundColor;

    private final float mDraggedElevation;

    /** The starting position of the moved item. */
    private int mStartPosition;

    /**
     * @param adapter The {@link DragReorderableRecyclerViewAdapter} that uses this class.
     */
    public DragTouchHandler(Context context, ModelList listData) {
        mListData = listData;

        Resources resources = context.getResources();
        if (ChromeFeatureList.sAndroidBookmarkBarFastFollow.isEnabled()) {
            mDraggedBackgroundColor = Color.TRANSPARENT;
        } else {
            // Set the alpha to 90% when dragging which is 230/255
            mDraggedBackgroundColor =
                    ColorUtils.setAlphaComponent(
                            SemanticColorUtils.getColorSurfaceContainerHigh(context),
                            resources.getInteger(R.integer.list_item_dragged_alpha));
        }

        mDraggedElevation = resources.getDimension(R.dimen.list_item_dragged_elevation);
    }

    /** Sets the {@link RecyclerView} instance. */
    public void setRecyclerView(@Nullable RecyclerView recyclerView) {
        mRecyclerView = recyclerView;
    }

    /** Stores the mapping between item type and draggability. */
    public void registerDraggableType(int typeId, DraggabilityProvider draggabilityProvider) {
        assert mDraggabilityProviderMap.get(typeId) == null;
        mDraggabilityProviderMap.put(typeId, draggabilityProvider);
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

    /** Sets whether the flag {@link mDragEnabled} is enabled. */
    public void setDragEnabled(boolean enabled) {
        mDragEnabled = enabled;
    }

    public boolean isActivelyDraggable(RecyclerView.ViewHolder viewHolder) {
        return isDraggableHelper(viewHolder, (dp, pm) -> dp.isActivelyDraggable(pm));
    }

    public boolean isPassivelyDraggable(RecyclerView.ViewHolder viewHolder) {
        return isDraggableHelper(viewHolder, (dp, pm) -> dp.isPassivelyDraggable(pm));
    }

    /** Returns whether a view is draggable. */
    boolean isDraggableHelper(
            RecyclerView.ViewHolder viewHolder,
            BiFunction<DraggabilityProvider, PropertyModel, Boolean> isDraggable) {
        if (!mDragEnabled) return false;
        DraggabilityProvider draggabilityProvider =
                mDraggabilityProviderMap.get(viewHolder.getItemViewType());
        if (draggabilityProvider == null) return false;
        int position = viewHolder.getBindingAdapterPosition();
        PropertyModel propertyModel = mListData.get(position).model;
        return isDraggable.apply(draggabilityProvider, propertyModel);
    }

    /** Gets the value for {@link mDraggedViewHolder}. */
    public RecyclerView.@Nullable ViewHolder getDraggedViewHolder() {
        return mDraggedViewHolder;
    }

    /** Gets the value for {@link mDraggedElevation}. */
    public float getDraggedElevation() {
        return mDraggedElevation;
    }

    /**
     * Sets whether the default system long-press drag gesture is enabled. If false, dragging can
     * only be initiated manually via {@link ItemTouchHelper#startDrag}.
     *
     * @param enabled True to enable automatic long-press detection, false to disable it.
     */
    public void setDefaultLongPressDragEnabled(boolean enabled) {
        mDefaultLongPressDragEnabled = enabled;
    }

    /**
     * @param longPressDragDelegate The delegate which decides when long press dragging is enabled.
     */
    public void setLongPressDragDelegate(LongPressDragDelegate longPressDragDelegate) {
        mLongPressDragDelegate = longPressDragDelegate;
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

    private void onSwap(int targetIndex) {
        for (DragListener dragListener : mListeners) {
            dragListener.onSwap(targetIndex);
        }
    }

    @Override
    public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        int dragFlags = 0;
        // This method may be called multiple times until the view is dropped
        // ensure there is only one bookmark being dragged.
        if ((mDraggedViewHolder == viewHolder || mDraggedViewHolder == null)
                && isActivelyDraggable(viewHolder)) {
            dragFlags = ItemTouchHelper.UP | ItemTouchHelper.DOWN;
        }
        return makeMovementFlags(dragFlags, /* swipeFlags= */ 0);
    }

    @Override
    public boolean onMove(
            RecyclerView recyclerView,
            RecyclerView.ViewHolder current,
            RecyclerView.ViewHolder target) {
        int from = current.getBindingAdapterPosition();
        int to = target.getBindingAdapterPosition();
        if (from == to) return false;
        mListData.move(from, to);
        return true;
    }

    @Override
    public void onSelectedChanged(RecyclerView.@Nullable ViewHolder viewHolder, int actionState) {
        super.onSelectedChanged(viewHolder, actionState);
        assumeNonNull(viewHolder);
        // similar to getMovementFlags, this method may be called multiple times
        if (actionState == ItemTouchHelper.ACTION_STATE_DRAG && mDraggedViewHolder != viewHolder) {
            mDraggedViewHolder = viewHolder;
            mStartPosition = viewHolder.getAdapterPosition();
            onDragStateChange(true);
            updateVisualState(true, viewHolder);
            if (ChromeFeatureList.sAndroidBookmarkBarFastFollow.isEnabled()) {
                if (mRecyclerView != null) {

                    PointerIcon icon =
                            PointerIcon.getSystemIcon(
                                    mRecyclerView.getContext(), PointerIcon.TYPE_GRABBING);

                    // This ensures that even when the user moves the cursor outside of the row
                    // while dragging, the cursor will still be TYPE_GRABBING instead of
                    // default.
                    mRecyclerView.setPointerIcon(icon);

                    // Iterate through all of the children (ImprovedBookmarkRows) and set the
                    // cursor explicitly to TYPE_GRABBING. This is to ensure that when we drag
                    // row A over row B, row B's grab handle's onHoverListener (open hand
                    // cursor) does not get activated.
                    for (int i = 0; i < mRecyclerView.getChildCount(); i++) {
                        View child = mRecyclerView.getChildAt(i);
                        child.setPointerIcon(icon);
                    }
                }
            }
        }
    }

    @Override
    public void clearView(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        super.clearView(recyclerView, viewHolder);

        if (ChromeFeatureList.sAndroidBookmarkBarFastFollow.isEnabled()) {
            assert recyclerView != null;

            // Reset to default cursor.
            recyclerView.setPointerIcon(null);

            // Reset children to default cursor.
            for (int i = 0; i < recyclerView.getChildCount(); i++) {
                View child = recyclerView.getChildAt(i);
                child.setPointerIcon(null);
            }
        }
        // No need to commit change if recycler view is not attached to window, such as dragging
        // is terminated by destroying activity.
        int currentPosition = viewHolder.getAdapterPosition();
        if (currentPosition != mStartPosition && recyclerView.isAttachedToWindow()) {
            // Commit the position change for the dragged item when it's dropped and
            // RecyclerView has finished layout computing.
            recyclerView.post(() -> onSwap(currentPosition));
        }
        mDraggedViewHolder = null;
        onDragStateChange(false);
        updateVisualState(false, viewHolder);
    }

    @Override
    public boolean isLongPressDragEnabled() {
        return mDefaultLongPressDragEnabled
                && mLongPressDragDelegate != null
                && mLongPressDragDelegate.isLongPressDragEnabled()
                && mDragEnabled;
    }

    @Override
    public boolean isItemViewSwipeEnabled() {
        return false;
    }

    @Override
    public void onSwiped(RecyclerView.ViewHolder viewHolder, int direction) {}

    @Override
    public boolean canDropOver(
            RecyclerView recyclerView,
            RecyclerView.ViewHolder current,
            RecyclerView.ViewHolder target) {
        // The fact that current is being dragged is proof enough since draggable views are
        // also passively draggable.
        return isPassivelyDraggable(current) && isPassivelyDraggable(target);
    }

    /**
     * Update the visual state of this row.
     *
     * @param dragged Whether this row is currently being dragged.
     * @param viewHolder The DraggableRowViewHolder that is holding this row's content.
     */
    public void updateVisualState(boolean dragged, RecyclerView.ViewHolder viewHolder) {
        DragUtils.createViewDragAnimation(
                        dragged, viewHolder.itemView, mDraggedBackgroundColor, mDraggedElevation)
                .start();
    }
}
