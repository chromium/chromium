// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.view.View;

import androidx.interpolator.view.animation.FastOutSlowInInterpolator;

import org.chromium.chrome.browser.ui.native_page.NativePage.SmoothTransitionDelegate;

/**
 * A basic implementation of a smooth transition delegate. This will trigger a smooth transition
 * when page is navigated back from webpage to native page by gesture.
 */
public class BasicSmoothTransitionDelegate implements SmoothTransitionDelegate {
    private static final int SMOOTH_TRANSITION_DURATION_MS = 100;

    private View mView;
    private Animator mAnimator;

    public BasicSmoothTransitionDelegate(View view) {
        mView = view;
        mAnimator = buildSmoothTransition(view);
    }

    @Override
    public void prepare() {
        assert !mAnimator.isRunning() : "Previous animation should not be running";
        cancel();
        mView.setAlpha(0f);
    }

    @Override
    public void start(Runnable onEnd) {
        assert !mAnimator.isRunning() : "Previous animation have been done or cancelled";
        mAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        onEnd.run();
                    }
                });
        mAnimator.start();
    }

    @Override
    public void cancel() {
        mAnimator.cancel();
        mView.setAlpha(1f);
    }

    private static Animator buildSmoothTransition(View view) {
        var animator = ObjectAnimator.ofFloat(view, View.ALPHA, 0f, 1f);
        animator.setInterpolator(new FastOutSlowInInterpolator());
        animator.setDuration(SMOOTH_TRANSITION_DURATION_MS);
        return animator;
    }

    public Animator getAnimatorForTesting() {
        return mAnimator;
    }
}
