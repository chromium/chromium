// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.view.GestureDetector;
import android.view.MotionEvent;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator.TouchEventDelegate;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.util.ColorUtils;

/** This class holds the animation and related business logic for the scrim. */
@NullMarked
class ScrimMediator implements TouchEventDelegate {
    private static final String TAG = "ScrimMediator";

    /** A callback that is run when the scrim has completely hidden. */
    private final Runnable mScrimHiddenRunnable;

    private final @ColorInt int mDefaultScrimColor;
    // TODO(skym): Re-implement to not have legacy suppliers.
    private final ObservableSupplierImpl<Integer> mFullScrimColorSupplier =
            new ObservableSupplierImpl<>(ScrimProperties.INVALID_COLOR);
    private final ObservableSupplierImpl<Integer> mStatusBarColorSupplier =
            new ObservableSupplierImpl<>(ScrimProperties.INVALID_COLOR);
    private final ObservableSupplierImpl<Integer> mNavigationBarColorSupplier =
            new ObservableSupplierImpl<>(ScrimProperties.INVALID_COLOR);
    private final ObservableSupplierImpl<Float> mStatusBarScrimFractionSupplier =
            new ObservableSupplierImpl<>(0f);
    private final ObservableSupplierImpl<Float> mNavigationBarScrimFractionSupplier =
            new ObservableSupplierImpl<>(0f);

    /** The animator for fading the view in. */
    private @Nullable ValueAnimator mOverlayFadeInAnimator;

    /** The animator for fading the view out. */
    private @Nullable ValueAnimator mOverlayFadeOutAnimator;

    /** The active animator (if any). */
    private @Nullable Animator mOverlayAnimator;

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
     * @param defaultScrimColor The color of the scrim when not explicitly set.
     */
    ScrimMediator(Runnable scrimHiddenRunnable, @ColorInt int defaultScrimColor) {
        mScrimHiddenRunnable = scrimHiddenRunnable;
        mDefaultScrimColor = defaultScrimColor;

        mFullScrimColorSupplier.addObserver((ignored) -> updateCompositeSuppliers());
        mStatusBarScrimFractionSupplier.addObserver((ignored) -> updateCompositeSuppliers());
        mNavigationBarScrimFractionSupplier.addObserver((ignored) -> updateCompositeSuppliers());
    }

    private void updateCompositeSuppliers() {
        mStatusBarColorSupplier.set(
                calculateCurrentCompositeColor(ScrimProperties.AFFECTS_STATUS_BAR));
        mNavigationBarColorSupplier.set(
                calculateCurrentCompositeColor(ScrimProperties.AFFECTS_NAVIGATION_BAR));
    }

    private @ColorInt int calculateCurrentCompositeColor(
            ReadableBooleanPropertyKey isAffectedProperty) {
        if (mModel == null) return ScrimProperties.INVALID_COLOR;

        boolean isAffected = mModel.get(isAffectedProperty);
        if (!isAffected) return ScrimProperties.INVALID_COLOR;

        float alpha = mModel.get(ScrimProperties.ALPHA);
        if (MathUtils.areFloatsEqual(alpha, 0f)) {
            return ScrimProperties.INVALID_COLOR;
        }

        @ColorInt int color = mModel.get(ScrimProperties.BACKGROUND_COLOR);
        return ColorUtils.applyAlphaFloat(color, alpha);
    }

    /* package */ @Nullable PropertyModel getModel() {
        return mModel;
    }

    /* package */ ObservableSupplier<Integer> getStatusBarColorSupplier() {
        return mStatusBarColorSupplier;
    }

    /* package */ ObservableSupplier<Integer> getNavigationBarColorSupplier() {
        return mNavigationBarColorSupplier;
    }

    /** Triggers a fade in of the scrim creating a new animation if necessary. */
    void showScrim(PropertyModel model, boolean animate, int animDurationMs) {
        // ALPHA is a protected property for this component that will only get added to the model
        // if ScrimProperties is used to build it.
        assert model.getAllProperties().contains(ScrimProperties.ALPHA)
                : "Use ScrimProperties to build the model used to show the scrim.";

        // Check the anchor here rather than in the model since clearing the scrim params
        // internally allows the anchor to be null.
        assert model.get(ScrimProperties.ANCHOR_VIEW) != null
                : "The anchor for the scrim cannot be null.";

        mModel = model;
        mModel.set(ScrimProperties.TOUCH_EVENT_DELEGATE, this);
        mIsHidingOrHidden = false;

        // When clients do not specify a background color, use the default.
        if (mModel.get(ScrimProperties.BACKGROUND_COLOR) == ScrimProperties.INVALID_COLOR) {
            mModel.set(ScrimProperties.BACKGROUND_COLOR, mDefaultScrimColor);
        }

        @ColorInt int currentScrimColor = model.get(ScrimProperties.BACKGROUND_COLOR);
        mFullScrimColorSupplier.set(currentScrimColor);

        // Make sure alpha is reset to 0 since the model may be reused.
        setAlphaInternal(0.f);

        if (mOverlayFadeInAnimator == null) {
            mOverlayFadeInAnimator = ValueAnimator.ofFloat(0, 1);

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
        mOverlayFadeInAnimator.setDuration(getAnimationDuration(animDurationMs));

        mIsNewEventFilter = model.get(ScrimProperties.GESTURE_DETECTOR) != null;
        mOverlayFadeInAnimator.setFloatValues(mModel.get(ScrimProperties.ALPHA), 1f);
        runFadeAnimation(mOverlayFadeInAnimator);
        if (!animate) mOverlayFadeInAnimator.end();
    }

    private int getAnimationDuration(int animDurationMs) {
        return mDisableAnimationForTesting ? 0 : animDurationMs;
    }

    /**
     * Triggers a fade out of the scrim creating a new animation if necessary.
     *
     * @param animate Whether the scrim should fade out.
     * @param animDurationMs Duration for animation run.
     */
    void hideScrim(boolean animate, int animDurationMs) {
        assert mModel != null : "#hideScrim(...) was called on an inactive scrim!";
        if (mIsHidingOrHidden) {
            if (mOverlayAnimator != null && !animate) mOverlayAnimator.end();
            return;
        }

        if (mOverlayFadeOutAnimator == null) {
            mOverlayFadeOutAnimator = ValueAnimator.ofFloat(1, 0);
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
        }
        mOverlayFadeOutAnimator.setDuration(getAnimationDuration(animDurationMs));

        mIsHidingOrHidden = true;
        mOverlayFadeOutAnimator.setFloatValues(mModel.get(ScrimProperties.ALPHA), 0f);
        runFadeAnimation(mOverlayFadeOutAnimator);
        if (!animate) mOverlayFadeOutAnimator.end();
    }

    /* package */ void setAlpha(float alpha) {
        if (mOverlayAnimator != null) {
            Log.w(TAG, "Scrim setAlpha was called during an animation.");
            mOverlayAnimator.cancel();
        }
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
        mModel.set(ScrimProperties.ALPHA, alpha);
        if (mModel.get(ScrimProperties.AFFECTS_STATUS_BAR)) {
            mStatusBarScrimFractionSupplier.set(alpha);
        }
        if (mModel.get(ScrimProperties.AFFECTS_NAVIGATION_BAR)) {
            mNavigationBarScrimFractionSupplier.set(alpha);
        }

        boolean isVisible = alpha > Float.MIN_NORMAL;
        if (mModel.get(ScrimProperties.VISIBILITY_CALLBACK) != null
                && mCurrentVisibility != isVisible) {
            mModel.get(ScrimProperties.VISIBILITY_CALLBACK).onResult(isVisible);
        }
        mCurrentVisibility = isVisible;

        if (mIsHidingOrHidden && !isVisible && mModel != null) {
            mModel = null;
            mFullScrimColorSupplier.set(ScrimProperties.INVALID_COLOR);
            mScrimHiddenRunnable.run();
        }
    }

    /* package */ void setScrimColor(@ColorInt int scrimColor) {
        assumeNonNull(mModel); // https://github.com/uber/NullAway/issues/1136
        mModel.set(ScrimProperties.BACKGROUND_COLOR, scrimColor);
        mFullScrimColorSupplier.set(scrimColor);
    }

    /**
     * Runs an animation for this view. If one is running, the existing one will be canceled.
     *
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
        assumeNonNull(mModel);
        GestureDetector gestureDetector = mModel.get(ScrimProperties.GESTURE_DETECTOR);
        if (gestureDetector == null) return false;

        // Make sure the first event that goes through the filter is an ACTION_DOWN, even in the
        // case where the filter is added while a gesture is already in progress.
        if (mIsNewEventFilter && e.getActionMasked() != MotionEvent.ACTION_DOWN) {
            MotionEvent downEvent = MotionEvent.obtain(e);
            downEvent.setAction(MotionEvent.ACTION_DOWN);
            if (!gestureDetector.onTouchEvent(downEvent)) return false;
        }
        mIsNewEventFilter = false;
        return gestureDetector.onTouchEvent(e);
    }
}
