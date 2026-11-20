// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.transition.ChangeBounds;
import android.transition.Transition;
import android.transition.TransitionListenerAdapter;
import android.transition.TransitionManager;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AccelerateDecelerateInterpolator;

import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemAnimator.ItemAnimatorFinishedListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ViewUtils;

import java.util.ArrayList;
import java.util.List;

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

    /** Runnables to run after all animations end. */
    private final List<Runnable> mOnAnimationsFinishedRunnables = new ArrayList<>();

    private final Runnable mFinishLayoutAndAnimationsRunnable =
            this::maybeFinishLayoutAndAnimations;
    private final ItemAnimatorFinishedListener mOnItemAnimatorFinishedListener =
            this::scheduleFinishLayoutAndAnimations;

    private @Nullable Transition mLastTransition;

    private View.@Nullable OnLayoutChangeListener mLayoutChangeListener;
    private final AccelerateDecelerateInterpolator mAccelerateDecelerateInterpolator =
            new AccelerateDecelerateInterpolator();

    public ExtensionActionListRecyclerView(Context context) {
        super(context);
        setupRecyclerView();
    }

    public ExtensionActionListRecyclerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setupRecyclerView();
    }

    public ExtensionActionListRecyclerView(
            Context context, @Nullable AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        setupRecyclerView();
    }

    private void setupRecyclerView() {
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(
                        getContext(), LinearLayoutManager.HORIZONTAL, /* reverseLayout= */ false) {
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
                    scheduleFinishLayoutAndAnimations();
                };
        addOnLayoutChangeListener(mLayoutChangeListener);
    }

    private void scheduleFinishLayoutAndAnimations() {
        removeCallbacks(mFinishLayoutAndAnimationsRunnable);
        post(mFinishLayoutAndAnimationsRunnable);
    }

    private void maybeFinishLayoutAndAnimations() {
        if (!isAttachedToWindow() || getAdapter() == null) {
            return;
        }
        if (isLayoutRequested()) {
            return;
        }
        if (hasPendingAdapterUpdates()) {
            ViewUtils.requestLayout(
                    this, "ExtensionActionListRecyclerView.maybeFinishLayoutAndAnimations");
            return;
        }
        // Check isRunning() before registering the listener because isRunning(listener) invokes
        // the listener synchronously when nothing is running, which would repost in a loop.
        if (mItemAnimator.isRunning()) {
            mItemAnimator.isRunning(mOnItemAnimatorFinishedListener);
            return;
        }
        if (mLastTransition != null) {
            return;
        }
        if (updateRecyclerViewWidth()) {
            return;
        }
        runAllOnAnimationsFinishedRunnables();
    }

    public void setTransitionRoot(ViewGroup transitionRoot) {
        mTransitionRoot = transitionRoot;
    }

    /**
     * Adds a callback to run after the UI becomes stable. Specifically, either after the animation
     * ends, or during the next layout if there is no pending animation.
     */
    public void addOnAnimationsFinishedRunnable(Runnable runnable) {
        mOnAnimationsFinishedRunnables.add(runnable);
    }

    /** Clears all of the registered callbacks. */
    public void clearOnAnimationsFinishedRunnables() {
        mOnAnimationsFinishedRunnables.clear();
    }

    private void runAllOnAnimationsFinishedRunnables() {
        if (mOnAnimationsFinishedRunnables.isEmpty()) {
            return;
        }

        List<Runnable> runnables = new ArrayList<>(mOnAnimationsFinishedRunnables);
        mOnAnimationsFinishedRunnables.clear();

        for (Runnable runnable : runnables) {
            runnable.run();
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mLayoutChangeListener != null) {
            removeOnLayoutChangeListener(mLayoutChangeListener);
            addOnLayoutChangeListener(mLayoutChangeListener);
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mLayoutChangeListener != null) {
            removeOnLayoutChangeListener(mLayoutChangeListener);
        }
        removeCallbacks(mFinishLayoutAndAnimationsRunnable);
        mLastTransition = null;
    }

    private int getDesiredWidth() {
        int itemWidth =
                getContext().getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
        Adapter<?> adapter = getAdapter();
        assert adapter != null;
        return itemWidth * adapter.getItemCount();
    }

    private boolean updateRecyclerViewWidth() {
        ViewGroup.LayoutParams lp = getLayoutParams();
        if (lp == null) {
            return false;
        }
        int newWidth = getDesiredWidth();
        if (lp.width != newWidth) {
            lp.width = newWidth;
            setLayoutParams(lp);
            return true;
        }
        return false;
    }

    /**
     * We need the width of {@code this} to follow the width of the contents so that the omnibox can
     * animate and 'hug' the actions. We do this by monitoring {@code this}'s {@link ItemAnimator}
     * and applying the same animation to the width.
     */
    private class ActionListItemAnimator extends DefaultItemAnimator {
        private boolean mHasPendingRemovals;
        private boolean mHasPendingAdditions;

        @Override
        public boolean animateRemove(ViewHolder holder) {
            mHasPendingRemovals = true;
            return super.animateRemove(holder);
        }

        @Override
        public boolean animateAdd(ViewHolder holder) {
            mHasPendingAdditions = true;
            return super.animateAdd(holder);
        }

        @Override
        public void runPendingAnimations() {
            assert mTransitionRoot != null;
            if ((mHasPendingRemovals || mHasPendingAdditions)
                    && getAdapter() != null
                    && getLayoutParams() != null
                    && getLayoutParams().width != getDesiredWidth()) {
                TransitionManager.endTransitions(mTransitionRoot);

                long startDelay = mHasPendingRemovals ? getRemoveDuration() : 0;
                long duration = getMoveDuration();

                Transition matchRecyclerView = new ChangeBounds();
                matchRecyclerView.setDuration(duration);
                matchRecyclerView.setStartDelay(startDelay);
                matchRecyclerView.setInterpolator(mAccelerateDecelerateInterpolator);

                matchRecyclerView.addListener(
                        new TransitionListenerAdapter() {
                            @Override
                            public void onTransitionEnd(Transition transition) {
                                onTransitionFinished(transition);
                            }

                            @Override
                            public void onTransitionCancel(Transition transition) {
                                onTransitionFinished(transition);
                            }

                            @Override
                            public void onTransitionStart(Transition transition) {
                                // Track the last transition, because we only want to run {@code
                                // mOnAnimationsFinishedRunnables} after all transitions end.
                                mLastTransition = transition;
                            }

                            private void onTransitionFinished(Transition transition) {
                                transition.removeListener(this);
                                if (transition == mLastTransition) {
                                    mLastTransition = null;
                                    scheduleFinishLayoutAndAnimations();
                                }
                            }
                        });

                TransitionManager.beginDelayedTransition(mTransitionRoot, matchRecyclerView);
                updateRecyclerViewWidth();
            }

            super.runPendingAnimations();
            mHasPendingRemovals = false;
            mHasPendingAdditions = false;
        }

        @Override
        public void endAnimations() {
            super.endAnimations();
            mHasPendingRemovals = false;
            mHasPendingAdditions = false;
        }
    }
}
