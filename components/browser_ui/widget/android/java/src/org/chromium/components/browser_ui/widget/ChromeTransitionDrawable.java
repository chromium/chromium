// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.animation.TimeInterpolator;
import android.animation.ValueAnimator;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.TransitionDrawable;
import android.graphics.drawable.VectorDrawable;
import android.util.IntProperty;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.ui.interpolators.Interpolators;

/**
 * Re-implementation of {@link TransitionDrawable} that works with {@link VectorDrawable} and uses
 * an {@link Animator} instead of manually implementing animation.
 */
public class ChromeTransitionDrawable extends LayerDrawable {
    private static final int MAX_PROGRESS_ALPHA = 255;
    private static final int MIN_PROGRESS_ALPHA = 0;

    /** Helper class that allows chained modification of a running transition. */
    public static class TransitionHandle {
        private final ValueAnimator mAnimator;

        private TransitionHandle(ValueAnimator animator) {
            mAnimator = animator;
        }

        public TransitionHandle setDuration(long durationMillis) {
            mAnimator.setDuration(durationMillis);
            return this;
        }

        public TransitionHandle setInterpolator(TimeInterpolator interpolator) {
            mAnimator.setInterpolator(interpolator);
            return this;
        }

        /**
         * Sets the end action to run when the animation ends. This will replace any existing end
         * actions set for this animator.
         */
        public TransitionHandle withEndAction(@NonNull Runnable endAction) {
            mAnimator.removeAllListeners();
            mAnimator.addListener(
                    new CancelAwareAnimatorListener() {
                        @Override
                        public void onEnd(Animator animator) {
                            endAction.run();
                        }
                    });
            return this;
        }
    }

    private final IntProperty<ChromeTransitionDrawable> mTransitionProgressProperty =
            new IntProperty<ChromeTransitionDrawable>("ChromeTransitionDrawableProgress") {
                @Override
                public Integer get(ChromeTransitionDrawable target) {
                    return target.mProgress;
                }

                @Override
                public void setValue(ChromeTransitionDrawable target, int value) {
                    target.setProgress(value);
                }
            };

    @NonNull private final Drawable mInitialDrawable;
    @NonNull private final Drawable mFinalDrawable;
    @NonNull private ObjectAnimator mAnimator;

    private boolean mCrossFade;
    private int mProgress;

    /**
     * Constructs a new ChromeTransitionDrawable. Initially, initialDrawable will be fully visible
     * and finalDrawable will be invisible. Call {@link #startTransition()} to
     * animate the transition from the initial drawable to the final drawable.
     * @param initialDrawable The first, initially visible drawable.
     * @param finalDrawable The second, initially hidden, drawable.
     */
    public ChromeTransitionDrawable(
            @NonNull Drawable initialDrawable, @NonNull Drawable finalDrawable) {
        super(new Drawable[] {initialDrawable.mutate(), finalDrawable.mutate()});
        mInitialDrawable = getDrawable(0);
        mFinalDrawable = getDrawable(1);
        mAnimator = ObjectAnimator.ofInt(this, mTransitionProgressProperty, MAX_PROGRESS_ALPHA);
    }

    /**
     * Enables cross-fading. When enabled, the initial drawable is faded out as the final drawable
     * fades in. When disabled, the initial drawable is always drawn at full alpha. If called
     * mid-transition, this will jump the alpha of the initial drawable to its correct value.
     */
    public void setCrossFadeEnabled(boolean crossFade) {
        mCrossFade = crossFade;
        int initialDrawableAlpha =
                crossFade ? (MAX_PROGRESS_ALPHA - mProgress) : MAX_PROGRESS_ALPHA;
        mInitialDrawable.setAlpha(initialDrawableAlpha);
    }

    /**
     * Returns a handle to a started transition between the initial drawable and final drawable.
     * If a transition is already active, this will start over from the beginning.
     */
    public TransitionHandle startTransition() {
        if (mAnimator.isRunning()) {
            mAnimator.cancel();
        }

        setProgress(MIN_PROGRESS_ALPHA);
        mAnimator = ObjectAnimator.ofInt(this, mTransitionProgressProperty, MAX_PROGRESS_ALPHA);
        mAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        mAnimator.start();
        return new TransitionHandle(mAnimator);
    }

    /**
     * Returns a handle to a started transition between the final drawable and initial drawable,
     * starting from the current progress. If a transition is already in progress, it will stop and
     * play backwards from the point at which reverse was called  This will by default inherit the
     * properties of the current Animator, but these can be modified via the returned handle.
     */
    public TransitionHandle reverseTransition() {
        mAnimator.reverse();
        return new TransitionHandle(mAnimator);
    }

    /** Reset to showing only either the initial or final drawable, cancelling any animation. */
    public void finishTransition(boolean resolveToFinalDrawable) {
        if (mAnimator.isRunning()) {
            mAnimator.cancel();
        }

        setProgress(resolveToFinalDrawable ? MAX_PROGRESS_ALPHA : MIN_PROGRESS_ALPHA);
    }

    public Drawable getInitialDrawable() {
        return mInitialDrawable;
    }

    public Drawable getFinalDrawable() {
        return mFinalDrawable;
    }

    @VisibleForTesting
    @NonNull
    public Animator getAnimatorForTesting() {
        return mAnimator;
    }

    @Override
    public void draw(@NonNull Canvas canvas) {
        if (mInitialDrawable.getAlpha() > 0) {
            mInitialDrawable.draw(canvas);
        }

        if (mFinalDrawable.getAlpha() > 0) {
            mFinalDrawable.draw(canvas);
        }
    }

    private void setProgress(int progress) {
        mProgress = progress;
        if (mCrossFade) {
            mInitialDrawable.setAlpha(MAX_PROGRESS_ALPHA - progress);
        }

        mFinalDrawable.setAlpha(progress);
    }
}
