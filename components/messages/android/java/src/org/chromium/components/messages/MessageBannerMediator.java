// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.chromium.components.messages.MessageBannerProperties.ALPHA;
import static org.chromium.components.messages.MessageBannerProperties.TRANSLATION_Y;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.content.Context;

import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelAnimatorFactory;

/**
 * Mediator responsible for the business logic in a message banner.
 */
class MessageBannerMediator {
    private static final int ANIMATION_DURATION_MS = 100;
    private static final float ANIMATION_OFFSET_DP = 50.f;

    private PropertyModel mModel;
    private AnimatorSet mAnimatorSet;
    private Context mContext;

    /**
     * Constructs the message banner mediator.
     */
    MessageBannerMediator(PropertyModel model, Context context) {
        mModel = model;
        mContext = context;
    }

    /**
     * Shows the message banner with an animation.
     * @param messageShown The {@link Runnable} that will run once the message banner is shown.
     */
    void show(Runnable messageShown) {
        if (mAnimatorSet != null && mAnimatorSet.isStarted()) {
            mAnimatorSet.cancel();
        }
        mAnimatorSet = createAnimatorSet(true);
        mAnimatorSet.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onEnd(Animator animator) {
                messageShown.run();
                mAnimatorSet = null;
            }
        });
        mAnimatorSet.start();
    }

    /**
     * Hides the message banner with an animation.
     * @param messageHidden The {@link Runnable} that will run once the message banner is hidden.
     */
    void hide(Runnable messageHidden) {
        if (mAnimatorSet != null && mAnimatorSet.isStarted()) {
            mAnimatorSet.cancel();
        }
        mAnimatorSet = createAnimatorSet(false);
        mAnimatorSet.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onEnd(Animator animator) {
                messageHidden.run();
                mAnimatorSet = null;
            }
        });
        mAnimatorSet.start();
    }

    private AnimatorSet createAnimatorSet(boolean isShow) {
        final float alphaTo = isShow ? 1.f : 0.f;
        final Animator alphaAnimation =
                PropertyModelAnimatorFactory.ofFloat(mModel, ALPHA, alphaTo);

        final float animationOffsetPx = ViewUtils.dpToPx(mContext, ANIMATION_OFFSET_DP);
        final float translateTo = isShow ? 0.f : -animationOffsetPx;
        final Animator translationAnimation =
                PropertyModelAnimatorFactory.ofFloat(mModel, TRANSLATION_Y, translateTo);

        final AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.play(alphaAnimation).with(translationAnimation);
        animatorSet.setDuration(ANIMATION_DURATION_MS);
        animatorSet.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        return animatorSet;
    }
}
