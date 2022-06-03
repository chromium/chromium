// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.animation;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import java.util.ArrayList;

/** Animates children of a vertical {@link LinearLayout} expanding/collapsing when focused. */
public class FocusAnimator {
    private static final int ANIMATION_LENGTH_MS = 225;

    /** Contains all of the Views that may be focused. */
    private final LinearLayout mLayout;

    /** Child that is being focused. */
    private final View mFocusedChild;

    /** Number of children initially set when the {@link FocusAnimator} was created. */
    private final int mInitialNumberOfChildren;

    /** Values of {@link View#getTop} for each child View.  See {@link #calculateChildTops}. */
    private final ArrayList<Integer> mInitialTops;

    /**
     * Constructs the {@link FocusAnimator}.
     *
     * To get the correct values to animate between, this should be called immediately before the
     * children of the layout are remeasured.
     *
     * @param layout       Layout being animated.
     * @param focusedChild Child being focused, or null if none is being focused.
     * @param callback     Callback to run when children are in the correct places.
     */
    public FocusAnimator(
            LinearLayout layout, @Nullable View focusedChild, final Runnable callback) {
        mLayout = layout;
        mFocusedChild = focusedChild;
        mInitialNumberOfChildren = mLayout.getChildCount();
        mInitialTops = calculateChildTops();

        // Add a listener to know when Android has done another measurement pass.  The listener
        // automatically removes itself to prevent triggering the animation multiple times.
        mLayout.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                mLayout.removeOnLayoutChangeListener(this);
                startAnimator(callback);
            }
        });
    }

    private void startAnimator(final Runnable callback) {
        // Don't animate anything if the number of children changed.
        if (mInitialNumberOfChildren != mLayout.getChildCount()) {
            finishAnimation(callback);
            return;
        }

        // Don't animate if children are already all in the correct places.
        boolean isAnimationNecessary = false;
        ArrayList<Integer> finalChildTops = calculateChildTops();
        for (int i = 0; i < finalChildTops.size() && !isAnimationNecessary; i++) {
            isAnimationNecessary |= finalChildTops.get(i).compareTo(mInitialTops.get(i)) != 0;
        }
        if (!isAnimationNecessary) {
            finishAnimation(callback);
            return;
        }

        // Animate each child moving and changing size to match their final locations.
        ArrayList<Animator> animators = new ArrayList<Animator>();
        ValueAnimator childAnimator = ValueAnimator.ofFloat(0f, 1f);
        animators.add(childAnimator);
        for (int i = 0; i < mLayout.getChildCount(); i++) {
            // The child is already where it should be.
            if (mInitialTops.get(i).compareTo(finalChildTops.get(i)) == 0
                    && mInitialTops.get(i + 1).compareTo(finalChildTops.get(i + 1)) == 0) {
                continue;
            }

            final View child = mLayout.getChildAt(i);
            final int translationDifference = mInitialTops.get(i) - finalChildTops.get(i);
            final int oldHeight = mInitialTops.get(i + 1) - mInitialTops.get(i);
            final int newHeight = finalChildTops.get(i + 1) - finalChildTops.get(i);

            // Translate the child to its new place while changing where its bottom is drawn to
            // animate the child changing height without causing another layout.
            childAnimator.addUpdateListener(new AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    float progress = (Float) animation.getAnimatedValue();
                    child.setTranslationY(translationDifference * (1f - progress));

                    if (oldHeight != newHeight) {
                        float animatedHeight = oldHeight * (1f - progress) + newHeight * progress;
                        child.setBottom(child.getTop() + (int) animatedHeight);
                    }
                }
            });

            // Explicitly place the child in its final position in the end.
            childAnimator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animator) {
                    child.setTranslationY(0);
                    child.setBottom(child.getTop() + newHeight);
                }
            });
        }

        // Animate the height of the container itself changing.
        int oldContainerHeight = mInitialTops.get(mInitialTops.size() - 1);
        int newContainerHeight = finalChildTops.get(finalChildTops.size() - 1);
        ValueAnimator layoutAnimator = ValueAnimator.ofInt(oldContainerHeight, newContainerHeight);
        layoutAnimator.addUpdateListener(new AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                mLayout.setBottom(((Integer) animation.getAnimatedValue()));
                requestChildFocus();
            }
        });
        animators.add(layoutAnimator);

        // Set up and kick off the animation.
        AnimatorSet animator = new AnimatorSet();
        animator.setDuration(ANIMATION_LENGTH_MS);
        animator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        animator.playTogether(animators);
        animator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animator) {
                finishAnimation(callback);

                // Request a layout to put everything in the right final place.
                mLayout.requestLayout();
            }
        });
        animator.start();
    }

    /** Cleans up the animation and notifies the owner that it is done via the runnable. */
    private void finishAnimation(Runnable callback) {
        requestChildFocus();
        callback.run();
    }

    /** Scroll the layout so that the focused child is on screen. */
    private void requestChildFocus() {
        ViewGroup parent = (ViewGroup) mLayout.getParent();
        if (mLayout.getParent() == null) return;

        // Scroll the parent to make the focused child visible.
        if (mFocusedChild != null) parent.requestChildFocus(mLayout, mFocusedChild);

        // {@link View#requestChildFocus} fails to account for children changing their height, so
        // the scroll value may be past the actual maximum.
        int viewportHeight = parent.getBottom() - parent.getTop();
        int scrollMax = Math.max(0, mLayout.getMeasuredHeight() - viewportHeight);
        if (parent.getScrollY() > scrollMax) parent.setScrollY(scrollMax);
    }

    /**
     * Calculates where the top of each child view should be.
     *
     * @return Array containing the values of {@link View#getTop} for each child of the layout.
     *         An additional value at the end indicates the total height of the layout and points at
     *         the bottom of the last child.
     */
    private ArrayList<Integer> calculateChildTops() {
        ArrayList<Integer> tops = new ArrayList<Integer>();

        int runningTotal = 0;
        for (int i = 0; i < mLayout.getChildCount(); i++) {
            tops.add(runningTotal);
            runningTotal += mLayout.getChildAt(i).getMeasuredHeight();
        }

        tops.add(runningTotal);
        return tops;
    }
}
