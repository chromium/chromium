// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.view.View;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.dragreorder.DragTouchHandler;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/** A custom {@link DragTouchHandler} implementation for toolbar actions. */
@NullMarked
public class ExtensionsToolbarDragTouchHandler extends DragTouchHandler {

    private static final int ANIMATION_DURATION_MS = 100;

    public ExtensionsToolbarDragTouchHandler(Context context, ModelList listData) {
        super(context, listData);
    }

    @Override
    public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        int dragFlags = 0;
        if ((getDraggedViewHolder() == viewHolder || getDraggedViewHolder() == null)
                && isActivelyDraggable(viewHolder)) {
            dragFlags = ItemTouchHelper.LEFT | ItemTouchHelper.RIGHT;
        }
        return makeMovementFlags(dragFlags, /* swipeFlags= */ 0);
    }

    @Override
    public void updateVisualState(boolean dragged, RecyclerView.ViewHolder viewHolder) {
        View view = viewHolder.itemView;

        float startElevation = view.getTranslationZ();
        float endElevation = dragged ? getDraggedElevation() : 0;
        ValueAnimator elevationAnimator = ValueAnimator.ofFloat(startElevation, endElevation);
        elevationAnimator.addUpdateListener(
                (anim) -> view.setTranslationZ((float) anim.getAnimatedValue()));

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.setDuration(ANIMATION_DURATION_MS);
        animatorSet.play(elevationAnimator);
        animatorSet.start();
    }

    @Override
    public void onChildDraw(
            Canvas c,
            RecyclerView recyclerView,
            RecyclerView.ViewHolder viewHolder,
            float dX,
            float dY,
            int actionState,
            boolean isCurrentlyActive) {
        View view = viewHolder.itemView;

        float newLeft = view.getLeft() + dX;
        float newRight = view.getRight() + dX;

        // Clamp dX so that the icon doesn't go outside RecyclerView's bounds.
        if (newLeft < 0) {
            dX = -view.getLeft();
        } else if (newRight > recyclerView.getWidth()) {
            dX = recyclerView.getWidth() - view.getRight();
        }

        super.onChildDraw(c, recyclerView, viewHolder, dX, dY, actionState, isCurrentlyActive);
    }
}
