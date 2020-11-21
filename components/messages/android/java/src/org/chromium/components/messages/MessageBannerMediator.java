// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.chromium.components.messages.MessageBannerProperties.ALPHA;
import static org.chromium.components.messages.MessageBannerProperties.TRANSLATION_Y;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.TimeInterpolator;
import android.content.res.Resources;
import android.view.MotionEvent;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelAnimatorFactory;

/**
 * Mediator responsible for the business logic in a message banner.
 */
class MessageBannerMediator implements SwipeHandler {
    private static final int SHOW_DURATION_MS = 400;
    private static final int HIDE_DURATION_MS = 300;
    private static final int ANIMATION_DELAY_MS = 100;
    private static final TimeInterpolator TRANSLATION_SHOW_INTERPOLATOR =
            Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR;
    private static final TimeInterpolator TRANSLATION_HIDE_INTERPOLATOR =
            Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR;
    private static final TimeInterpolator ALPHA_INTERPOLATOR = Interpolators.LINEAR_INTERPOLATOR;

    private PropertyModel mModel;
    private AnimatorSet mAnimatorSet;
    private Supplier<Integer> mMaxTranslationSupplier;

    private final float mHideThresholdPx;
    private final Runnable mMessageDismissed;

    private float mSwipeStartTranslationY;

    /**
     * Constructs the message banner mediator.
     */
    MessageBannerMediator(PropertyModel model, Supplier<Integer> maxTranslationSupplier,
            Resources resources, Runnable messageDismissed) {
        mModel = model;
        mMaxTranslationSupplier = maxTranslationSupplier;
        mHideThresholdPx = resources.getDimensionPixelSize(R.dimen.message_hide_threshold);
        mMessageDismissed = messageDismissed;
    }

    /**
     * Shows the message banner with an animation.
     * @param messageShown The {@link Runnable} that will run once the message banner is shown.
     */
    void show(Runnable messageShown) {
        if (mAnimatorSet == null) {
            mModel.set(TRANSLATION_Y, -mMaxTranslationSupplier.get());
        }
        cancelAnyAnimations();
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
        cancelAnyAnimations();
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

    void setOnTouchRunnable(Runnable runnable) {
        mModel.set(MessageBannerProperties.ON_TOUCH_RUNNABLE, runnable);
    }

    // region SwipeHandler implementation
    // ---------------------------------------------------------------------------------------------

    @Override
    public void onSwipeStarted(@ScrollDirection int direction, MotionEvent ev) {
        mSwipeStartTranslationY = mModel.get(TRANSLATION_Y);
    }

    @Override
    public void onSwipeUpdated(
            MotionEvent current, float tx, float ty, float distanceX, float distanceY) {
        final float currentGesturePositionY = mSwipeStartTranslationY + ty;
        final float currentTranslationY =
                MathUtils.clamp(currentGesturePositionY, -mMaxTranslationSupplier.get(), 0);
        mModel.set(TRANSLATION_Y, currentTranslationY);
    }
    // TODO(sinansahin): See if we need special handling for #onFling.

    @Override
    public void onSwipeFinished() {
        cancelAnyAnimations();

        // No need to animate if the message banner is in resting position.
        if (mModel.get(TRANSLATION_Y) == 0.f) return;

        final boolean isShow = mModel.get(TRANSLATION_Y) > -mHideThresholdPx;
        mAnimatorSet = createAnimatorSet(isShow);
        mAnimatorSet.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onEnd(Animator animator) {
                if (!isShow) mMessageDismissed.run();
                mAnimatorSet = null;
            }
        });
        mAnimatorSet.start();
    }

    @Override
    public boolean isSwipeEnabled(@ScrollDirection int direction) {
        // TODO(sinansahin): We will implement swiping left/right to dismiss.
        return (direction == ScrollDirection.UP || direction == ScrollDirection.DOWN)
                && (mAnimatorSet == null || !mAnimatorSet.isRunning());
    }

    // ---------------------------------------------------------------------------------------------
    // endregion

    private AnimatorSet createAnimatorSet(boolean isShow) {
        final long duration = isShow ? SHOW_DURATION_MS : HIDE_DURATION_MS;

        final float alphaTo = isShow ? 1.f : 0.f;
        final Animator alphaAnimation =
                PropertyModelAnimatorFactory.ofFloat(mModel, ALPHA, alphaTo);
        alphaAnimation.setInterpolator(ALPHA_INTERPOLATOR);
        alphaAnimation.setDuration(duration);

        final float translateTo = isShow ? 0.f : -mMaxTranslationSupplier.get();
        final Animator translationAnimation =
                PropertyModelAnimatorFactory.ofFloat(mModel, TRANSLATION_Y, translateTo);
        translationAnimation.setInterpolator(
                isShow ? TRANSLATION_SHOW_INTERPOLATOR : TRANSLATION_HIDE_INTERPOLATOR);
        translationAnimation.setDuration(duration);

        (isShow ? translationAnimation : alphaAnimation).setStartDelay(ANIMATION_DELAY_MS);

        final AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(alphaAnimation, translationAnimation);

        return animatorSet;
    }

    private void cancelAnyAnimations() {
        if (mAnimatorSet != null && mAnimatorSet.isStarted()) mAnimatorSet.cancel();
        mAnimatorSet = null;
    }
}
