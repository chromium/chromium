// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.transition.ChangeBounds;
import android.transition.Transition;
import android.transition.TransitionManager;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AccelerateDecelerateInterpolator;

import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A {@link RecyclerView} for extension action buttons. This container will automatically hide any
 * buttons that don't fit into the allowed width.
 */
@NullMarked
public class ExtensionActionListRecyclerView extends RecyclerView {

    private static final int RECYCLER_VIEW_PADDING = 300;

    private @Nullable ViewGroup mTransitionRoot;

    /** Custom animator that triggers our layout loop when it accepts an animation task. */
    private final DefaultItemAnimator mItemAnimator = new ActionListItemAnimator();

    private View.@Nullable OnLayoutChangeListener mLayoutChangeListener;
    private final AccelerateDecelerateInterpolator mAccelerateDecelerateInterpolator =
            new AccelerateDecelerateInterpolator();

    public ExtensionActionListRecyclerView(Context context) {
        super(context);
        setupRecyclerView(context);
    }

    public ExtensionActionListRecyclerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setupRecyclerView(context);
    }

    public ExtensionActionListRecyclerView(
            Context context, @Nullable AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        setupRecyclerView(context);
    }

    private void setupRecyclerView(Context context) {
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(
                        context, LinearLayoutManager.HORIZONTAL, /* reverseLayout= */ false) {
                    @Override
                    public boolean canScrollHorizontally() {
                        return false;
                    }

                    @Override
                    public void calculateExtraLayoutSpace(
                            RecyclerView.State state, int[] extraLayoutSpace) {
                        // We put enough padding to make sure the View instances at the edge do not
                        // disappear.
                        extraLayoutSpace[0] = RECYCLER_VIEW_PADDING;
                        extraLayoutSpace[1] = RECYCLER_VIEW_PADDING;
                    }
                };
        layoutManager.setStackFromEnd(true);
        setLayoutManager(layoutManager);
        setItemAnimator(mItemAnimator);

        // We monitor {@code this} for layout changes so that we can update the width of {@code
        // this} even when items are updated without animation, most notably during initialization.
        mLayoutChangeListener =
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    post(
                            () -> {
                                if (!mItemAnimator.isRunning()) {
                                    updateRecyclerViewWidth();
                                }
                            });
                };
        addOnLayoutChangeListener(mLayoutChangeListener);
    }

    public void setTransitionRoot(ViewGroup transitionRoot) {
        mTransitionRoot = transitionRoot;
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mLayoutChangeListener != null) {
            removeOnLayoutChangeListener(mLayoutChangeListener);
        }
    }

    private void updateRecyclerViewWidth() {
        int newWidth = calculateContentWidth();
        if (getLayoutParams().width != newWidth) {
            final var lp = getLayoutParams();
            lp.width = calculateContentWidth();
            setLayoutParams(lp);
        }
    }

    private int calculateContentWidth() {
        if (getAdapter() == null || getAdapter().getItemCount() == 0) return 0;

        RecyclerView.ViewHolder startViewHolder = findViewHolderForAdapterPosition(0);
        int lastIndex = getAdapter().getItemCount() - 1;
        RecyclerView.ViewHolder endViewHolder = findViewHolderForAdapterPosition(lastIndex);

        if (startViewHolder == null || endViewHolder == null) {
            return 0;
        }

        View startView = startViewHolder.itemView;
        View endView = endViewHolder.itemView;

        float startPosition;
        float endPosition;

        boolean rtl = getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
        if (rtl) {
            startPosition = startView.getRight();
            endPosition = endView.getLeft();
        } else {
            startPosition = startView.getLeft();
            endPosition = endView.getRight();
        }

        return Math.round(Math.abs(endPosition - startPosition));
    }

    /**
     * We need the width of {@code this} to follow the width of the contents so that the omnibox can
     * animate and 'hug' the actions. We do this by monitoring {@code this}'s {@link ItemAnimator}
     * and applying the same animation to the width.
     */
    private class ActionListItemAnimator extends DefaultItemAnimator {
        private boolean mHasPendingRemovals;

        @Override
        public boolean animateRemove(ViewHolder holder) {
            mHasPendingRemovals = true;
            return super.animateRemove(holder);
        }

        @Override
        public void runPendingAnimations() {
            long startDelay = mHasPendingRemovals ? getRemoveDuration() : 0;
            long duration = getMoveDuration();

            if (mTransitionRoot != null) {
                Transition matchRecyclerView = new ChangeBounds();
                matchRecyclerView.setDuration(duration);
                matchRecyclerView.setStartDelay(startDelay);
                matchRecyclerView.setInterpolator(mAccelerateDecelerateInterpolator);

                TransitionManager.beginDelayedTransition(mTransitionRoot, matchRecyclerView);
                updateRecyclerViewWidth();
            }

            super.runPendingAnimations();

            mHasPendingRemovals = false;
        }
    }
}
