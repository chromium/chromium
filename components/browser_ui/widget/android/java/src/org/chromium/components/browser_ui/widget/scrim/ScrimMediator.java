// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.view.MotionEvent;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.modelutil.PropertyModel;

/** This class holds the animation and related business logic for the scrim. */
class ScrimMediator implements ScrimCoordinator.TouchEventDelegate {
    /** The duration for the fading animation. */
    private static final int FADE_DURATION_MS = 300;

    /** A callback that is run when the scrim has completely hidden. */
    private final Runnable mScrimHiddenRunnable;

    /** A means of changing the system UI color. */
    private ScrimCoordinator.SystemUiScrimDelegate mSystemUiScrimDelegate;

    /** The animator for fading the view in. */
    private ValueAnimator mOverlayFadeInAnimator;

    /** The animator for fading the view out. */
    private ValueAnimator mOverlayFadeOutAnimator;

    /** The active animator (if any). */
    private Animator mOverlayAnimator;

    /** The duration for the fading animation. This can be overridden for testing. */
    private int mFadeDurationMs;

    /** The model for the scrim component. */
    private PropertyModel mModel;

    /** Whether the scrim is currently visible. */
    private boolean mCurrentVisibility;

    /** If true, {@code mActiveParams.eventFilter} is set, but never had an event passed to it. */
    private boolean mIsNewEventFilter;

    /** Whether the scrim is in the process of hiding or is currently hidden. */
    private boolean mIsHidingOrHidden;

    /**
     * @param scrimHiddenRunnable A mechanism for hiding the scrim.
     * @param systemUiScrimDelegate A means of changing the scrim over the system UI.
     */
    ScrimMediator(@NonNull Runnable scrimHiddenRunnable,
            ScrimCoordinator.SystemUiScrimDelegate systemUiScrimDelegate) {
        mScrimHiddenRunnable = scrimHiddenRunnable;
        mSystemUiScrimDelegate = systemUiScrimDelegate;
        mFadeDurationMs = FADE_DURATION_MS;
    }

    /** Triggers a fade in of the scrim creating a new animation if necessary. */
    void showScrim(@NonNull PropertyModel model) {
        // ALPHA is a protected property for this component that will only get added to the model
        // if ScrimProperties is used to build it.
        assert model.getAllProperties().contains(ScrimProperties.ALPHA)
            : "Use ScrimProperties to build the model used to show the scrim.";

        // Check the anchor here rather than in the model since clearing the scrim params
        // internally allows the anchor to be null.
        assert model.get(ScrimProperties.ANCHOR_VIEW)
                != null : "The anchor for the scrim cannot be null.";

        mModel = model;
        mIsHidingOrHidden = false;

        // Make sure alpha is reset to 0 since the model may be reused.
        setAlphaInternal(0);

        if (mOverlayFadeInAnimator == null) {
            mOverlayFadeInAnimator = ValueAnimator.ofFloat(0, 1);
            mOverlayFadeInAnimator.setDuration(mFadeDurationMs);
            mOverlayFadeInAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
            mOverlayFadeInAnimator.addListener(new CancelAwareAnimatorListener() {
                @Override
                public void onEnd(Animator animation) {
                    mOverlayAnimator = null;
                }
            });
            mOverlayFadeInAnimator.addUpdateListener(
                    animation -> { setAlphaInternal((float) animation.getAnimatedValue()); });
        }

        if (model.getAllSetProperties().contains(ScrimProperties.GESTURE_DETECTOR)) {
            mIsNewEventFilter = model.get(ScrimProperties.GESTURE_DETECTOR) != null;
        }
        mOverlayFadeInAnimator.setFloatValues(mModel.get(ScrimProperties.ALPHA), 1f);
        runFadeAnimation(mOverlayFadeInAnimator);
    }

    /**
     * Triggers a fade out of the scrim creating a new animation if necessary.
     * @param animate Whether the scrim should fade out.
     */
    void hideScrim(boolean animate) {
        assert mModel != null : "#hideScrim(...) was called on an inactive scrim!";

        if (mOverlayFadeOutAnimator == null) {
            mOverlayFadeOutAnimator = ValueAnimator.ofFloat(1, 0);
            mOverlayFadeOutAnimator.setDuration(mFadeDurationMs);
            mOverlayFadeOutAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            mOverlayFadeOutAnimator.addListener(new CancelAwareAnimatorListener() {
                @Override
                public void onEnd(Animator animation) {
                    // If the animation wasn't ended early, alpha will already be 0 and the model
                    // will be null as a result of #setAlphaInternal().
                    if (mModel != null) setAlphaInternal(0);
                    mOverlayAnimator = null;
                }
            });
            mOverlayFadeOutAnimator.addUpdateListener(
                    animation -> { setAlphaInternal((float) animation.getAnimatedValue()); });
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

        boolean isVisible = alpha > 0;
        if (mModel.get(ScrimProperties.VISIBILITY_CALLBACK) != null
                && mCurrentVisibility != isVisible) {
            mModel.get(ScrimProperties.VISIBILITY_CALLBACK).onResult(isVisible);
        }
        mCurrentVisibility = isVisible;

        if (mIsHidingOrHidden && !isVisible && mModel != null) {
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

    /** "Destroy" the mediator and clean up any state. */
    void destroy() {
        // If the scrim was active, ending the animation will clean up any state, otherwise noop.
        if (mOverlayAnimator != null) mOverlayAnimator.end();
    }

    @VisibleForTesting
    void disableAnimationForTesting(boolean disable) {
        mFadeDurationMs = disable ? 0 : FADE_DURATION_MS;
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
