// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.view.MotionEvent;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

/** This class holds the animation and related business logic for the scrim. */
class ScrimMediator implements ScrimCoordinator.TouchEventDelegate {
    /** A callback that is run when the scrim has completely hidden. */
    private final @NonNull Runnable mScrimHiddenRunnable;

    /** A means of changing the system UI color. */
    private final @Nullable ScrimCoordinator.SystemUiScrimDelegate mSystemUiScrimDelegate;

    private final PropertyObserver<PropertyKey> mOnModelChange = this::onModelChange;

    /** The animator for fading the view in. */
    private ValueAnimator mOverlayFadeInAnimator;

    /** The animator for fading the view out. */
    private ValueAnimator mOverlayFadeOutAnimator;

    /** The active animator (if any). */
    private Animator mOverlayAnimator;

    /** The model for the scrim component. */
    private @Nullable PropertyModel mModel;

    /** Whether the scrim is currently visible. */
    private boolean mCurrentVisibility;

    /** If true, {@code mActiveParams.eventFilter} is set, but never had an event passed to it. */
    private boolean mIsNewEventFilter;

    /** Whether the scrim is in the process of hiding or is currently hidden. */
    private boolean mIsHidingOrHidden;

    private boolean mDisableAnimationForTesting;

    /**
     * @param scrimHiddenRunnable A mechanism for hiding the scrim.
     * @param systemUiScrimDelegate A means of changing the scrim over the system UI.
     */
    ScrimMediator(
            @NonNull Runnable scrimHiddenRunnable,
            @Nullable ScrimCoordinator.SystemUiScrimDelegate systemUiScrimDelegate) {
        mScrimHiddenRunnable = scrimHiddenRunnable;
        mSystemUiScrimDelegate = systemUiScrimDelegate;
    }

    /** Triggers a fade in of the scrim creating a new animation if necessary. */
    void showScrim(@NonNull PropertyModel model, int animDurationMs) {
        // ALPHA is a protected property for this component that will only get added to the model
        // if ScrimProperties is used to build it.
        assert model.getAllProperties().contains(ScrimProperties.ALPHA)
                : "Use ScrimProperties to build the model used to show the scrim.";

        // Check the anchor here rather than in the model since clearing the scrim params
        // internally allows the anchor to be null.
        assert model.get(ScrimProperties.ANCHOR_VIEW) != null
                : "The anchor for the scrim cannot be null.";

        if (mModel != null && mSystemUiScrimDelegate != null) {
            mModel.removeObserver(mOnModelChange);
        }
        mModel = model;
        mIsHidingOrHidden = false;
        int fadeDurationMs = getAnimationDuration(animDurationMs);

        // Pass the current scrim color to the SystemUiScrimDelegate.
        if (mSystemUiScrimDelegate != null
                && model.getAllSetProperties().contains(ScrimProperties.BACKGROUND_COLOR)) {
            @ColorInt int color = model.get(ScrimProperties.BACKGROUND_COLOR);
            mSystemUiScrimDelegate.setScrimColor(color);
            mModel.addObserver(mOnModelChange);
        }

        // Make sure alpha is reset to 0 since the model may be reused.
        setAlphaInternal(0.f);

        if (mOverlayFadeInAnimator == null) {
            mOverlayFadeInAnimator = ValueAnimator.ofFloat(0, 1);
            mOverlayFadeInAnimator.setDuration(fadeDurationMs);
            mOverlayFadeInAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
            mOverlayFadeInAnimator.addListener(
                    new CancelAwareAnimatorListener() {
                        @Override
                        public void onEnd(Animator animation) {
                            mOverlayAnimator = null;
                        }

                        @Override
                        public void onCancel(Animator animation) {
                            setAlphaInternal(0.f);
                            onEnd(animation);
                        }
                    });
            mOverlayFadeInAnimator.addUpdateListener(
                    animation -> {
                        setAlphaInternal((float) animation.getAnimatedValue());
                    });
        }

        if (model.getAllSetProperties().contains(ScrimProperties.GESTURE_DETECTOR)) {
            mIsNewEventFilter = model.get(ScrimProperties.GESTURE_DETECTOR) != null;
        }
        mOverlayFadeInAnimator.setFloatValues(mModel.get(ScrimProperties.ALPHA), 1f);
        runFadeAnimation(mOverlayFadeInAnimator);
    }

    private void onModelChange(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
        assert mSystemUiScrimDelegate != null;
        if (propertyKey == ScrimProperties.BACKGROUND_COLOR) {
            @ColorInt int color = mModel.get(ScrimProperties.BACKGROUND_COLOR);
            mSystemUiScrimDelegate.setScrimColor(color);
        }
    }

    private int getAnimationDuration(int animDurationMs) {
        return mDisableAnimationForTesting ? 0 : animDurationMs;
    }

    /**
     * Triggers a fade out of the scrim creating a new animation if necessary.
     * @param animate Whether the scrim should fade out.
     * @param animDurationMs Duration for animation run.
     */
    void hideScrim(boolean animate, int animDurationMs) {
        assert mModel != null : "#hideScrim(...) was called on an inactive scrim!";
        if (mIsHidingOrHidden) {
            if (mOverlayAnimator != null && !animate) mOverlayAnimator.end();
            return;
        }

        int fadeDurationMs = getAnimationDuration(animDurationMs);

        if (mOverlayFadeOutAnimator == null) {
            mOverlayFadeOutAnimator = ValueAnimator.ofFloat(1, 0);
            mOverlayFadeOutAnimator.setDuration(fadeDurationMs);
            mOverlayFadeOutAnimator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
            mOverlayFadeOutAnimator.addListener(
                    new CancelAwareAnimatorListener() {
                        @Override
                        public void onEnd(Animator animation) {
                            // If the animation wasn't ended early, alpha will already be 0 and the
                            // model will be null as a result of #setAlphaInternal().
                            if (mModel != null) setAlphaInternal(0.f);
                            mOverlayAnimator = null;
                        }

                        @Override
                        public void onCancel(Animator animation) {
                            onEnd(animation);
                        }
                    });
            mOverlayFadeOutAnimator.addUpdateListener(
                    animation -> {
                        setAlphaInternal((float) animation.getAnimatedValue());
                    });
            mOverlayFadeOutAnimator.addListener(
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            // Reset the scrim color stored in the SystemUiScrimDelegate.
                            if (mSystemUiScrimDelegate != null) {
                                mSystemUiScrimDelegate.setScrimColor(ScrimProperties.INVALID_COLOR);
                            }
                        }
                    });
        }

        mIsHidingOrHidden = true;
        mOverlayFadeOutAnimator.setFloatValues(mModel.get(ScrimProperties.ALPHA), 0f);
        runFadeAnimation(mOverlayFadeOutAnimator);
        if (!animate) mOverlayFadeOutAnimator.end();
    }

    /**
     * Manually set the alpha for the scrim. This is exposed as part of the public API and should
     * not be called as part of animations as it cancels the currently running one.
     * @param alpha The new alpha for the scrim in range [0, 1].
     */
    void setAlpha(float alpha) {
        if (mOverlayAnimator != null) mOverlayAnimator.cancel();
        setAlphaInternal(alpha);
    }

    /**
     * This method actually changes the alpha and can be used for setting the alpha via animation.
     * @param alpha The new alpha for the scrim in range [0, 1].
     */
    private void setAlphaInternal(float alpha) {
        // TODO(mdjones): This null check is exclusively for Android K which has a slightly
        //                different order for animation events. Once deprecated we should remove it.
        if (mModel == null) return;
        if (MathUtils.areFloatsEqual(alpha, mModel.get(ScrimProperties.ALPHA))) return;
        mModel.set(ScrimProperties.ALPHA, alpha);
        if (mModel.get(ScrimProperties.AFFECTS_STATUS_BAR) && mSystemUiScrimDelegate != null) {
            mSystemUiScrimDelegate.setStatusBarScrimFraction(alpha);
        }
        if (mModel.getAllSetProperties().contains(ScrimProperties.AFFECTS_NAVIGATION_BAR)
                && mModel.get(ScrimProperties.AFFECTS_NAVIGATION_BAR)
                && mSystemUiScrimDelegate != null) {
            mSystemUiScrimDelegate.setNavigationBarScrimFraction(alpha);
        }

        boolean isVisible = alpha > Float.MIN_NORMAL;
        if (mModel.get(ScrimProperties.VISIBILITY_CALLBACK) != null
                && mCurrentVisibility != isVisible) {
            mModel.get(ScrimProperties.VISIBILITY_CALLBACK).onResult(isVisible);
        }
        mCurrentVisibility = isVisible;

        if (mIsHidingOrHidden && !isVisible && mModel != null) {
            if (mSystemUiScrimDelegate != null) {
                mModel.removeObserver(mOnModelChange);
            }
            mModel = null;
            mScrimHiddenRunnable.run();
        }
    }

    /**
     * Runs an animation for this view. If one is running, the existing one will be canceled.
     * @param fadeAnimation The animation to run.
     */
    private void runFadeAnimation(Animator fadeAnimation) {
        if (mOverlayAnimator == fadeAnimation && mOverlayAnimator.isRunning()) {
            return;
        } else if (mOverlayAnimator != null) {
            mOverlayAnimator.cancel();
        }
        mOverlayAnimator = fadeAnimation;
        mOverlayAnimator.start();
    }

    /** @return Whether the scrim is still active (has a non-null model). */
    boolean isActive() {
        return mModel != null;
    }

    /** Force the current animation to run to completion immediately. */
    void forceAnimationToFinish() {
        if (mOverlayAnimator != null) {
            mOverlayAnimator.end();
        }
    }

    /** "Destroy" the mediator and clean up any state. */
    void destroy() {
        // If the scrim was active, ending the animation will clean up any state, otherwise noop.
        forceAnimationToFinish();
    }

    void disableAnimationForTesting(boolean disable) {
        mDisableAnimationForTesting = disable;
    }

    @VisibleForTesting
    boolean areAnimationsRunning() {
        return mOverlayAnimator != null && mOverlayAnimator.isRunning();
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        if (mIsHidingOrHidden) return false;
        if (!mModel.getAllSetProperties().contains(ScrimProperties.GESTURE_DETECTOR)) return false;

        // Make sure the first event that goes through the filter is an ACTION_DOWN, even in the
        // case where the filter is added while a gesture is already in progress.
        if (mIsNewEventFilter && e.getActionMasked() != MotionEvent.ACTION_DOWN) {
            MotionEvent downEvent = MotionEvent.obtain(e);
            downEvent.setAction(MotionEvent.ACTION_DOWN);
            if (!mModel.get(ScrimProperties.GESTURE_DETECTOR).onTouchEvent(downEvent)) return false;
        }
        mIsNewEventFilter = false;
        return mModel.get(ScrimProperties.GESTURE_DETECTOR).onTouchEvent(e);
    }
}
