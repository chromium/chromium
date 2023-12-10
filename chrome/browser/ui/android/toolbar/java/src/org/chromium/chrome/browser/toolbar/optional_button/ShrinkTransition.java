// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.graphics.Path;
import android.transition.TransitionValues;
import android.transition.Visibility;
import android.view.View;
import android.view.ViewGroup;

/**
 * A transition that changes a view's scale when it's appearing or disappearing, it animates both
 * scaleX and scaleY towards 0 when disappearing and towards 1 when appearing.
 */
public class ShrinkTransition extends Visibility {
    @Override
    public Animator onAppear(
            ViewGroup sceneRoot,
            View view,
            TransitionValues startValues,
            TransitionValues endValues) {
        return createAnimation(view, view.getScaleX(), 1);
    }

    @Override
    public Animator onDisappear(
            ViewGroup sceneRoot,
            View view,
            TransitionValues startValues,
            TransitionValues endValues) {
        return createAnimation(view, view.getScaleX(), 0);
    }

    private Animator createAnimation(final View view, float startScale, float endScale) {
        Path animationPath = new Path();
        animationPath.moveTo(startScale, startScale);
        animationPath.lineTo(endScale, endScale);

        final ObjectAnimator animator =
                ObjectAnimator.ofFloat(view, "ScaleX", "ScaleY", animationPath);

        if (endScale == 0) {
            animator.addListener(
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            // We reset the scale to 1 when disappearing because the next time this
                            // view is visible this transition may not be involved, so it'll be
                            // visible but with a scale of 0.
                            view.setScaleX(1);
                            view.setScaleY(1);

                            super.onAnimationEnd(animation);
                        }
                    });
        }

        return animator;
    }
}
