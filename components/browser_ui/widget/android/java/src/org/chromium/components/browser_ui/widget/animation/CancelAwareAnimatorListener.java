// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.animation;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;

/**
 * An {@link AnimatorListenerAdapter} that distinguishes cancel and end signal. Subclasses should
 * override {@link #onStart(Animator)}, {@link #onEnd(Animator)} and {@link #onCancel(Animator)}
 * instead of the standard callback functions.
 */
public class CancelAwareAnimatorListener extends AnimatorListenerAdapter {
    // Only allows one of the following to be called for any one start(): onEnd(), onCancel(). Also
    // serves as a guard against an infinite loop that's present in ValueAnimator triggered
    // when calling ValueAnimator#end() in an AnimatorListenerAdapter#onAnimationEnd() callback.
    private boolean mHasResultCallbackBeenInvoked;

    @Override
    public final void onAnimationStart(Animator animation) {
        mHasResultCallbackBeenInvoked = false;
        onStart(animation);
    }

    @Override
    public final void onAnimationCancel(Animator animation) {
        if (mHasResultCallbackBeenInvoked) return;
        mHasResultCallbackBeenInvoked = true;
        onCancel(animation);
    }

    @Override
    public final void onAnimationEnd(Animator animation) {
        if (mHasResultCallbackBeenInvoked) return;
        mHasResultCallbackBeenInvoked = true;
        onEnd(animation);
    }

    /**
     * Notifies the start of the animator.
     */
    public void onStart(Animator animator) {}

    /**
     * Notifies that the animator was cancelled.
     */
    public void onCancel(Animator animator) {}

    /**
     * Notifies that the animator has finished running. This method will not be called if the
     * animator is canclled.
     */
    public void onEnd(Animator animator) {}
}
