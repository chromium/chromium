// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection.DOWN;
import static org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection.UP;
import static org.chromium.components.messages.MessageBannerProperties.ALPHA;
import static org.chromium.components.messages.MessageBannerProperties.TRANSLATION_X;
import static org.chromium.components.messages.MessageBannerProperties.TRANSLATION_Y;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.TimeInterpolator;
import android.content.res.Resources;
import android.view.MotionEvent;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModelAnimatorFactory;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Mediator responsible for the business logic in a message banner.
 */
class MessageBannerMediator implements SwipeHandler {
    // Message banner state
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({State.HIDDEN, State.ANIMATING, State.IDLE, State.GESTURE})
    private @interface State {
        // Hidden or never shown
        int HIDDEN = 0;
        // In motion without user interaction
        int ANIMATING = 1;
        // Resting state / fully shown
        int IDLE = 2;
        // User gesture
        int GESTURE = 3;

        int NUM_ENTRIES = 4;
    }

    private static final int ENTER_DURATION_MS = 600;
    private static final int EXIT_DURATION_MS = 300;
    private static final int ANIMATION_DELAY_MS = 100;
    private static final TimeInterpolator TRANSLATION_ENTER_INTERPOLATOR =
            Interpolators.OVERSHOOT_INTERPOLATOR;
    private static final TimeInterpolator ALPHA_ENTER_INTERPOLATOR =
            Interpolators.LINEAR_INTERPOLATOR;
    private static final TimeInterpolator EXIT_INTERPOLATOR =
            Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR;

    private final PropertyModel mModel;
    private final Supplier<Integer> mMaxTranslationYSupplier;

    private final float mVerticalHideThresholdPx;
    private final float mHorizontalHideThresholdPx;
    private final Supplier<Float> mMaxHorizontalTranslationPx;
    private final Runnable mMessageDismissed;
    private final Callback<Animator> mAnimatorStartCallback;

    private Animator mAnimation;
    @State
    private int mCurrentState = State.HIDDEN;
    @ScrollDirection
    private int mSwipeDirection;
    private float mSwipeStartTranslation;
    private boolean mDidFling;

    /**
     * Constructs the message banner mediator.
     */
    MessageBannerMediator(PropertyModel model, Supplier<Integer> maxTranslationSupplier,
            Resources resources, Runnable messageDismissed,
            Callback<Animator> animatorStartCallback) {
        mModel = model;
        mMaxTranslationYSupplier = maxTranslationSupplier;
        mVerticalHideThresholdPx =
                resources.getDimensionPixelSize(R.dimen.message_vertical_hide_threshold);
        mHorizontalHideThresholdPx =
                resources.getDimensionPixelSize(R.dimen.message_horizontal_hide_threshold);
        mMaxHorizontalTranslationPx = () -> {
            final float screenWidth = resources.getDisplayMetrics().widthPixels;
            return Math.min(
                    resources.getDimensionPixelSize(R.dimen.message_max_horizontal_translation),
                    screenWidth / 2);
        };
        mMessageDismissed = messageDismissed;
        mAnimatorStartCallback = animatorStartCallback;
    }

    /**
     * Shows the message banner with an animation.
     * @param messageShown The {@link Runnable} that will run once the message banner is shown.
     */
    void show(Runnable messageShown) {
        if (mCurrentState == State.HIDDEN) {
            mModel.set(TRANSLATION_Y, -mMaxTranslationYSupplier.get());
        }
        cancelAnyAnimations();
        startAnimation(true, 0, false, messageShown);
    }

    /**
     * Hides the message banner with an animation.
     * @param animate Whether to hide with an animation.
     * @param messageHidden The {@link Runnable} that will run once the message banner is hidden.
     */
    void hide(boolean animate, Runnable messageHidden) {
        cancelAnyAnimations();

        if (!animate) {
            mModel.set(ALPHA, 0.f);
            mModel.set(TRANSLATION_Y, -mMaxTranslationYSupplier.get());
            mCurrentState = State.HIDDEN;
        }

        if (mCurrentState == State.HIDDEN) {
            messageHidden.run();
            return;
        }

        startAnimation(true, -mMaxTranslationYSupplier.get(), false, messageHidden);
    }

    void setOnTouchRunnable(Runnable runnable) {
        mModel.set(MessageBannerProperties.ON_TOUCH_RUNNABLE, runnable);
    }

    // region SwipeHandler implementation
    // ---------------------------------------------------------------------------------------------

    @Override
    public void onSwipeStarted(@ScrollDirection int direction, MotionEvent ev) {
        mCurrentState = State.GESTURE;
        mSwipeDirection = direction;
        mSwipeStartTranslation =
                isVertical(mSwipeDirection) ? mModel.get(TRANSLATION_Y) : mModel.get(TRANSLATION_X);
        mDidFling = false;
    }

    @Override
    public void onSwipeUpdated(
            MotionEvent current, float tx, float ty, float distanceX, float distanceY) {
        if (isVertical(mSwipeDirection)) {
            final float currentGesturePositionY = mSwipeStartTranslation + ty;
            final float currentTranslationY =
                    MathUtils.clamp(currentGesturePositionY, -mMaxTranslationYSupplier.get(), 0);
            mModel.set(TRANSLATION_Y, currentTranslationY);
        } else {
            final float currentGesturePositionX = mSwipeStartTranslation + tx;
            final float currentTranslationX = MathUtils.clamp(currentGesturePositionX,
                    -mMaxHorizontalTranslationPx.get(), mMaxHorizontalTranslationPx.get());
            mModel.set(TRANSLATION_X, currentTranslationX);
        }
        mModel.set(ALPHA, calculateAlphaForTranslation(isVertical(mSwipeDirection)));
    }

    @Override
    public void onSwipeFinished() {
        // A fling gesture will already be handled in #onFling.
        if (mDidFling) return;

        cancelAnyAnimations();

        // No need to animate if the message banner is in resting position.
        if (isResting()) {
            mCurrentState = State.IDLE;
            return;
        }

        // If the current translation is within the hide threshold, i.e. message shouldn't be
        // dismissed, we will run an animation returning the message to the idle position.
        // Otherwise, the message will be dismissed with an animation.
        final boolean isVertical = isVertical(mSwipeDirection);
        float translateTo;
        if (isVertical) {
            translateTo = mModel.get(TRANSLATION_Y) > -mVerticalHideThresholdPx
                    ? 0
                    : -mMaxTranslationYSupplier.get();
        } else {
            final float translationX = mModel.get(TRANSLATION_X);
            final boolean withinHideThreshold = Math.abs(translationX) < mHorizontalHideThresholdPx;

            translateTo = withinHideThreshold
                    ? 0
                    : MathUtils.flipSignIf(mMaxHorizontalTranslationPx.get(), translationX < 0);
        }
        startAnimation(
                isVertical, translateTo, false, translateTo != 0 ? mMessageDismissed : () -> {});
    }

    @Override
    public void onFling(@ScrollDirection int direction, MotionEvent current, float tx, float ty,
            float velocityX, float velocityY) {
        mDidFling = true;

        // Flinging toward the idle position from outside the hiding threshold should animate the
        // message to the idle position. Otherwise, the message will be dismissed with animation.
        final boolean isVertical = isVertical(mSwipeDirection);
        final float velocity = isVertical ? velocityY : velocityX;
        float translateTo;
        if (isVertical) {
            final float translationY = mModel.get(TRANSLATION_Y);
            translateTo = translationY < 0 ? -mMaxTranslationYSupplier.get() : 0;
        } else {
            final float translationX = mModel.get(TRANSLATION_X);
            if (Math.abs(translationX) < mHorizontalHideThresholdPx) {
                translateTo = 0;
            } else {
                translateTo =
                        MathUtils.flipSignIf(mMaxHorizontalTranslationPx.get(), translationX < 0);
            }
        }

        // TODO(crbug.com/1157213): See if we can use velocity to change the animation
        // speed/duration.
        startAnimation(isVertical(mSwipeDirection), translateTo, velocity != 0,
                translateTo != 0 ? mMessageDismissed : () -> {});
    }

    @Override
    public boolean isSwipeEnabled(@ScrollDirection int direction) {
        return direction != ScrollDirection.UNKNOWN && mCurrentState == State.IDLE;
    }

    // ---------------------------------------------------------------------------------------------
    // endregion

    /**
     * Create and start an animation.
     * @param vertical Whether the message is being animated vertically.
     * @param translateTo Target translation value for the animation.
     * @param didFling Whether the animation is the result of a fling gesture.
     * @param onEndCallback Callback that will be called after the animation.
     */
    private void startAnimation(
            boolean vertical, float translateTo, boolean didFling, Runnable onEndCallback) {
        final long duration = translateTo == 0 ? ENTER_DURATION_MS : EXIT_DURATION_MS;

        final boolean isShow = translateTo == 0;
        final float alphaTo = isShow ? 1.f : 0.f;
        final Animator alphaAnimation =
                PropertyModelAnimatorFactory.ofFloat(mModel, ALPHA, alphaTo);
        alphaAnimation.setInterpolator(isShow ? ALPHA_ENTER_INTERPOLATOR : EXIT_INTERPOLATOR);
        alphaAnimation.setDuration(duration);

        final WritableFloatPropertyKey translationProperty =
                vertical ? TRANSLATION_Y : TRANSLATION_X;
        final Animator translationAnimation =
                PropertyModelAnimatorFactory.ofFloat(mModel, translationProperty, translateTo);
        translationAnimation.setInterpolator(
                isShow ? TRANSLATION_ENTER_INTERPOLATOR : EXIT_INTERPOLATOR);
        translationAnimation.setDuration(duration);

        // Alpha and translation animations will be played simultaneously if they're the result of a
        // fling gesture. Otherwise, we start one with a delay depending on the direction of the
        // animation.
        if (!didFling) {
            (isShow ? translationAnimation : alphaAnimation).setStartDelay(ANIMATION_DELAY_MS);
        }

        final AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(alphaAnimation, translationAnimation);

        animatorSet.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onStart(Animator animator) {
                mCurrentState = State.ANIMATING;
            }

            @Override
            public void onEnd(Animator animator) {
                mCurrentState = isShow ? State.IDLE : State.HIDDEN;
                onEndCallback.run();
                mAnimation = null;
            }
        });

        mAnimation = animatorSet;
        mAnimatorStartCallback.onResult(mAnimation);
    }

    private void cancelAnyAnimations() {
        if (mAnimation != null) mAnimation.cancel();
        mAnimation = null;
    }

    private float calculateAlphaForTranslation(boolean vertical) {
        final float displacementRatio = vertical
                ? Math.abs(mModel.get(TRANSLATION_Y)) / mMaxTranslationYSupplier.get()
                : Math.abs(mModel.get(TRANSLATION_X)) / mMaxHorizontalTranslationPx.get();
        return 1 - displacementRatio;
    }

    private boolean isVertical(@ScrollDirection int direction) {
        return direction == UP || direction == DOWN;
    }

    private boolean isResting() {
        return mModel.get(TRANSLATION_Y) == 0.f && mModel.get(TRANSLATION_X) == 0.f;
    }

    @VisibleForTesting
    Supplier<Float> getMaxHorizontalTranslationSupplierForTesting() {
        return mMaxHorizontalTranslationPx;
    }
}
