// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection.DOWN;
import static org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection.UP;
import static org.chromium.components.messages.MessageBannerProperties.CONTENT_ALPHA;
import static org.chromium.components.messages.MessageBannerProperties.MARGIN_TOP;
import static org.chromium.components.messages.MessageBannerProperties.TRANSLATION_X;
import static org.chromium.components.messages.MessageBannerProperties.TRANSLATION_Y;
import static org.chromium.components.messages.MessageBannerProperties.VISUAL_HEIGHT;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.TimeInterpolator;
import android.content.res.Resources;
import android.view.MotionEvent;

import androidx.annotation.IntDef;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.components.messages.MessageStateHandler.Position;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModelAnimatorFactory;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/** Mediator responsible for the business logic in a message banner. */
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
    }

    private static final int ENTER_DURATION_MS = 550;
    private static final int EXIT_DURATION_MS = 350;
    private static final TimeInterpolator TRANSLATION_ENTER_INTERPOLATOR =
            Interpolators.EMPHASIZED_DECELERATE;
    private static final TimeInterpolator ALPHA_ENTER_INTERPOLATOR =
            Interpolators.EMPHASIZED_DECELERATE;
    private static final TimeInterpolator EXIT_INTERPOLATOR = Interpolators.EMPHASIZED_DECELERATE;

    private final PropertyModel mModel;
    private final Supplier<Integer> mMaxTranslationYSupplier;
    private final Supplier<Integer> mTopOffsetSupplier;

    private final float mVerticalHideThresholdPx;
    private final float mHorizontalHideThresholdPx;
    private final float mMaxHorizontalTranslationPx;
    private final Runnable mMessageDismissed;
    private final SwipeAnimationHandler mSwipeAnimationHandler;
    private final int mPeekingMarginTop;
    private final int mDefaultMarginTop;

    private Animator mAnimation;
    @State private int mCurrentState = State.HIDDEN;
    @ScrollDirection private int mSwipeDirection;
    private float mSwipeStartTranslation;
    private boolean mDidFling;

    /** Constructs the message banner mediator. */
    MessageBannerMediator(
            PropertyModel model,
            Supplier<Integer> topOffsetSupplier,
            Supplier<Integer> maxTranslationSupplier,
            Resources resources,
            Runnable messageDismissed,
            SwipeAnimationHandler swipeAnimationHandler) {
        mModel = model;
        mTopOffsetSupplier = topOffsetSupplier;
        mMaxTranslationYSupplier = maxTranslationSupplier;
        mVerticalHideThresholdPx =
                resources.getDimensionPixelSize(R.dimen.message_vertical_hide_threshold);
        mHorizontalHideThresholdPx =
                resources.getDimensionPixelSize(R.dimen.message_horizontal_hide_threshold);
        mMaxHorizontalTranslationPx = resources.getDisplayMetrics().widthPixels;
        mMessageDismissed = messageDismissed;
        mSwipeAnimationHandler = swipeAnimationHandler;
        mDefaultMarginTop = resources.getDimensionPixelSize(R.dimen.message_shadow_top_margin);
        mPeekingMarginTop =
                resources.getDimensionPixelSize(R.dimen.message_peeking_layer_height)
                        + mDefaultMarginTop;
    }

    /**
     * Shows the message banner with an animation.
     *
     * @param fromIndex The initial position.
     * @param toIndex The target position the message is moving to.
     * @param verticalOffset Extra offset the message is supposed to move down besides the default
     *                       marginTop value.
     * @param messageShown The {@link Runnable} that will run once the message banner is shown.
     * @return The animator to show the message.
     */
    Animator show(
            @Position int fromIndex,
            @Position int toIndex,
            int verticalOffset,
            Runnable messageShown) {
        if (mCurrentState == State.HIDDEN) {
            mModel.set(TRANSLATION_Y, fromIndex == Position.FRONT ? 0 : -mTopOffsetSupplier.get());
            mModel.set(MARGIN_TOP, mDefaultMarginTop);
            mModel.set(CONTENT_ALPHA, 0.f);
            mModel.set(VISUAL_HEIGHT, 0.f);
        } else if (mCurrentState == State.IDLE && toIndex == Position.FRONT) {
            // Animating marginTop is expensive. Use translationY to simulate the effect of
            // marginTop.
            assert fromIndex == Position.BACK;
            mModel.set(TRANSLATION_Y, mModel.get(MARGIN_TOP) - mDefaultMarginTop);
            mModel.set(MARGIN_TOP, mDefaultMarginTop);
        }
        cancelAnyAnimations();
        return startAnimation(
                true,
                true,
                0,
                toIndex == Position.BACK ? mPeekingMarginTop + verticalOffset : mDefaultMarginTop,
                messageShown);
    }

    /**
     * Hides the message banner with an animation.
     * @param fromIndex The initial position.
     * @param toIndex The target position the message is moving to.
     * @param animate Whether to hide with an animation.
     * @param messageHidden The {@link Runnable} that will run once the message banner is hidden.
     * @return The animator to hide the message.
     */
    Animator hide(
            @Position int fromIndex,
            @Position int toIndex,
            boolean animate,
            Runnable messageHidden) {
        cancelAnyAnimations();
        float translateTo = toIndex == Position.FRONT ? 0 : -mTopOffsetSupplier.get();
        if (!animate) {
            mModel.set(CONTENT_ALPHA, 0.f);
            mModel.set(VISUAL_HEIGHT, 0.f);
            mModel.set(TRANSLATION_Y, translateTo);
            mCurrentState = State.HIDDEN;
        }

        if (mCurrentState == State.HIDDEN) {
            messageHidden.run();
            return null;
        }
        return startAnimation(true, false, translateTo, mDefaultMarginTop, messageHidden);
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
        mSwipeAnimationHandler.onSwipeStart();
    }

    @Override
    public void onSwipeUpdated(
            MotionEvent current, float tx, float ty, float distanceX, float distanceY) {
        if (isVertical(mSwipeDirection)) {
            final float currentGesturePositionY = mSwipeStartTranslation + ty;
            final float currentTranslationY =
                    MathUtils.clamp(currentGesturePositionY, -mMaxTranslationYSupplier.get(), 0);
            mModel.set(TRANSLATION_Y, currentTranslationY);
            // At most 50% opacity while swiping.
            mModel.set(
                    CONTENT_ALPHA,
                    Math.max(.5f, calculateAlphaForTranslation(isVertical(mSwipeDirection))));
        } else {
            final float currentGesturePositionX = mSwipeStartTranslation + tx;
            final float currentTranslationX =
                    MathUtils.clamp(
                            currentGesturePositionX,
                            -mMaxHorizontalTranslationPx,
                            mMaxHorizontalTranslationPx);
            mModel.set(TRANSLATION_X, currentTranslationX);
            mModel.set(CONTENT_ALPHA, calculateAlphaForTranslation(isVertical(mSwipeDirection)));
        }
    }

    @Override
    public void onSwipeFinished() {
        // A fling gesture will already be handled in #onFling.
        if (mDidFling) return;

        cancelAnyAnimations();

        // No need to animate if the message banner is in resting position.
        if (isResting()) {
            mCurrentState = State.IDLE;
            mSwipeAnimationHandler.onSwipeEnd(null);
            return;
        }

        // If the current translation is within the hide threshold, i.e. message shouldn't be
        // dismissed, we will run an animation returning the message to the idle position.
        // Otherwise, the message will be dismissed with an animation.
        final boolean isVertical = isVertical(mSwipeDirection);
        float translateTo;
        if (isVertical) {
            translateTo =
                    mModel.get(TRANSLATION_Y) > -mVerticalHideThresholdPx
                            ? 0
                            : -mTopOffsetSupplier.get();
        } else {
            final float translationX = mModel.get(TRANSLATION_X);
            final boolean withinHideThreshold = Math.abs(translationX) < mHorizontalHideThresholdPx;

            translateTo =
                    withinHideThreshold
                            ? 0
                            : MathUtils.flipSignIf(mMaxHorizontalTranslationPx, translationX < 0);
        }
        boolean isShow = translateTo == 0;
        mSwipeAnimationHandler.onSwipeEnd(
                startAnimation(
                        isVertical,
                        isShow,
                        translateTo,
                        mDefaultMarginTop,
                        isShow ? () -> {} : mMessageDismissed));
    }

    @Override
    public void onFling(
            @ScrollDirection int direction,
            MotionEvent current,
            float tx,
            float ty,
            float velocityX,
            float velocityY) {
        mDidFling = true;

        // Flinging toward the idle position from outside the hiding threshold should animate the
        // message to the idle position. Otherwise, the message will be dismissed with animation.
        final boolean isVertical = isVertical(mSwipeDirection);
        final float velocity = isVertical ? velocityY : velocityX;
        float translateTo;
        if (isVertical) {
            final float translationY = mModel.get(TRANSLATION_Y);
            translateTo = translationY < 0 ? -mTopOffsetSupplier.get() : 0;
        } else {
            final float translationX = mModel.get(TRANSLATION_X);
            if (Math.abs(translationX) < mHorizontalHideThresholdPx) {
                translateTo = 0;
            } else {
                translateTo = MathUtils.flipSignIf(mMaxHorizontalTranslationPx, translationX < 0);
            }
        }

        // TODO(crbug.com/40736315): See if we can use velocity to change the animation
        // speed/duration.
        boolean isShow = translateTo == 0;
        mSwipeAnimationHandler.onSwipeEnd(
                startAnimation(
                        isVertical(mSwipeDirection),
                        isShow,
                        translateTo,
                        mDefaultMarginTop,
                        isShow ? () -> {} : mMessageDismissed));
    }

    @Override
    public boolean isSwipeEnabled(@ScrollDirection int direction) {
        return direction != ScrollDirection.UNKNOWN
                && mCurrentState == State.IDLE
                && mSwipeAnimationHandler.isSwipeEnabled();
    }

    // ---------------------------------------------------------------------------------------------
    // endregion

    /**
     * Create and start an animation.
     *
     * @param vertical Whether the message is being animated vertically.
     * @param isShow Whether the message is going to be shown.
     * @param translateTo Target translation value for the animation.
     * @param marginTo The marginTop value the view should move to.
     * @param onEndCallback Callback that will be called after the animation.
     * @return The animator which can trigger the animation.
     */
    private AnimatorSet startAnimation(
            boolean vertical,
            boolean isShow,
            float translateTo,
            int marginTo,
            Runnable onEndCallback) {
        final long duration = isShow ? ENTER_DURATION_MS : EXIT_DURATION_MS;
        List<Animator> animators = new ArrayList<>();

        if (vertical) {
            final Animator expand =
                    PropertyModelAnimatorFactory.ofFloat(mModel, VISUAL_HEIGHT, isShow ? 1 : 0);
            expand.setInterpolator(ALPHA_ENTER_INTERPOLATOR);
            expand.setDuration(duration);
            animators.add(expand);
        }

        final float alphaTo = isShow ? 1.f : 0.f;
        Animator alphaAnimation =
                PropertyModelAnimatorFactory.ofFloat(mModel, CONTENT_ALPHA, alphaTo);
        alphaAnimation.setInterpolator(EXIT_INTERPOLATOR);
        alphaAnimation.setDuration(duration);
        animators.add(alphaAnimation);

        final WritableFloatPropertyKey translationProperty =
                vertical ? TRANSLATION_Y : TRANSLATION_X;
        // Animating marginTop is expensive. Animating translateY here and then set real marginTop
        // value and reset translateY in the end of animation.
        final Animator translationAnimation =
                PropertyModelAnimatorFactory.ofFloat(
                        mModel, translationProperty, translateTo + marginTo - mDefaultMarginTop);
        translationAnimation.setInterpolator(
                isShow ? TRANSLATION_ENTER_INTERPOLATOR : EXIT_INTERPOLATOR);
        translationAnimation.setDuration(duration);
        animators.add(translationAnimation);

        final AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(animators);

        animatorSet.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onStart(Animator animator) {
                        mCurrentState = State.ANIMATING;
                    }

                    @Override
                    public void onEnd(Animator animator) {
                        if (isShow) {
                            mModel.set(MARGIN_TOP, marginTo);
                            mModel.set(TRANSLATION_Y, 0);
                        }
                        mCurrentState = isShow ? State.IDLE : State.HIDDEN;
                        onEndCallback.run();
                        mAnimation = null;
                    }
                });

        mAnimation = animatorSet;
        return animatorSet;
    }

    private void cancelAnyAnimations() {
        if (mAnimation != null) mAnimation.cancel();
        mAnimation = null;
    }

    private float calculateAlphaForTranslation(boolean vertical) {
        final float displacementRatio =
                vertical
                        ? Math.abs(mModel.get(TRANSLATION_Y)) / mTopOffsetSupplier.get()
                        : Math.abs(mModel.get(TRANSLATION_X)) / mMaxHorizontalTranslationPx;
        return 1 - displacementRatio;
    }

    private boolean isVertical(@ScrollDirection int direction) {
        return direction == UP || direction == DOWN;
    }

    private boolean isResting() {
        return mModel.get(TRANSLATION_Y) == 0.f && mModel.get(TRANSLATION_X) == 0.f;
    }
}
