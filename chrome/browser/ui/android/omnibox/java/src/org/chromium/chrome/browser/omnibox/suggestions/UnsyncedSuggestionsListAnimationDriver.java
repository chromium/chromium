// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;

import androidx.annotation.NonNull;

import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.BooleanSupplier;

/**
 * {@link SuggestionsListAnimationDriver} that runs an unsynced, fixed duration fade + translate
 * animation against the given list property model when focusing the omnibox.
 */
public class UnsyncedSuggestionsListAnimationDriver
        implements SuggestionsListAnimationDriver, AnimatorUpdateListener, AnimatorListener {

    // This duration is chosen to match the duration of the IME show animation when it is unsynced,
    // which it is in our case.
    // https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/view/InsetsController.java;l=236;drc=cad0f6adc5e8ca56f9a35a20f23ddd87c13af33e
    private static final int DURATION = 200;

    @NonNull private final PropertyModel mListPropertyModel;
    @NonNull private final Runnable mShowSuggestionsListCallback;
    @NonNull private final BooleanSupplier mShouldAnimateSuggestions;
    private final int mStartingVerticalOffset;
    private ValueAnimator mAnimator;

    /**
     * @param listPropertyModel Property model for the suggestions list view being animated.
     * @param showSuggestionsListCallback Callback that shows the suggestions list when invoked.
     * @param shouldAnimateSuggestionsSupplier Supplier telling us if we can run the animation at a
     *     given point in time.
     * @param startingVerticalOffset The number of pixels down that suggestions should be translated
     *     at the start of the animation; the ending translation will be 0.
     */
    public UnsyncedSuggestionsListAnimationDriver(
            @NonNull PropertyModel listPropertyModel,
            @NonNull Runnable showSuggestionsListCallback,
            @NonNull BooleanSupplier shouldAnimateSuggestionsSupplier,
            int startingVerticalOffset) {

        mListPropertyModel = listPropertyModel;
        mShowSuggestionsListCallback = showSuggestionsListCallback;
        mShouldAnimateSuggestions = shouldAnimateSuggestionsSupplier;
        mStartingVerticalOffset = startingVerticalOffset;
    }

    @Override
    public void onOmniboxSessionStateChange(boolean active) {
        if (active) {
            startAnimation();
        } else if (mAnimator != null && mAnimator.isRunning()) {
            mAnimator.cancel();
            mAnimator = null;
        }
    }

    @Override
    public boolean isAnimationEnabled() {
        return mShouldAnimateSuggestions.getAsBoolean();
    }

    private void startAnimation() {
        mAnimator = ValueAnimator.ofFloat(0.f, 1.f).setDuration(DURATION);
        mAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        mAnimator.addUpdateListener(this);
        mAnimator.addListener(this);
        mAnimator.start();
    }

    @Override
    public void onAnimationUpdate(@NonNull ValueAnimator valueAnimator) {
        mListPropertyModel.set(SuggestionListProperties.ALPHA, valueAnimator.getAnimatedFraction());
        mListPropertyModel.set(
                SuggestionListProperties.CHILD_TRANSLATION_Y,
                mStartingVerticalOffset * (1.0f - valueAnimator.getAnimatedFraction()));
    }

    @Override
    public void onAnimationStart(@NonNull Animator animator) {
        mShowSuggestionsListCallback.run();
        mListPropertyModel.set(SuggestionListProperties.ALPHA, 0.0f);
        mListPropertyModel.set(
                SuggestionListProperties.CHILD_TRANSLATION_Y, mStartingVerticalOffset);
    }

    @Override
    public void onAnimationEnd(@NonNull Animator animator) {
        mListPropertyModel.set(SuggestionListProperties.ALPHA, 1.0f);
        mListPropertyModel.set(SuggestionListProperties.CHILD_TRANSLATION_Y, 0.0f);
    }

    @Override
    public void onAnimationCancel(@NonNull Animator animator) {
        // Show the list in case we get cancelled ahead of starting.
        mShowSuggestionsListCallback.run();
        onAnimationEnd(animator);
    }

    @Override
    public void onAnimationRepeat(@NonNull Animator animator) {}
}
