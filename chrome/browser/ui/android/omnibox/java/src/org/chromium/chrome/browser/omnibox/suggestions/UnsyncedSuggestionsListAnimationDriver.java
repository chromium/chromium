// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/**
 * {@link SuggestionsListAnimationDriver} that runs an unsynced, fixed duration fade + translate
 * animation against the given list property model when focusing the omnibox.
 */
@NullMarked
public class UnsyncedSuggestionsListAnimationDriver
        implements SuggestionsListAnimationDriver, AnimatorUpdateListener, AnimatorListener {
    private static boolean sAnimationsDisabledForTesting;

    // This duration is chosen to match the duration of the IME show animation when it is unsynced,
    // which it is in our case.
    // https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/view/InsetsController.java;l=236;drc=cad0f6adc5e8ca56f9a35a20f23ddd87c13af33e
    private static final int DURATION = 200;

    private final PropertyModel mListPropertyModel;
    private final Runnable mShowSuggestionsListCallback;
    private final BooleanSupplier mIsToolbarBottomAnchoredSupplier;
    private final Supplier<Float> mOmniboxVerticalTranslationSupplier;
    private final Context mContext;
    private int mStartingVerticalOffset;
    private @Nullable ValueAnimator mAnimator;

    /**
     * @param listPropertyModel Property model for the suggestions list view being animated.
     * @param showSuggestionsListCallback Callback that shows the suggestions list when invoked.
     * @param isToolbarBottomAnchoredSupplier Supplier that tells us if the toolbar is
     *     bottom-anchored at the beginning of the focus animation process.
     */
    public UnsyncedSuggestionsListAnimationDriver(
            PropertyModel listPropertyModel,
            Runnable showSuggestionsListCallback,
            BooleanSupplier isToolbarBottomAnchoredSupplier,
            Supplier<Float> omniboxVerticalTranslationSupplier,
            Context context) {

        mListPropertyModel = listPropertyModel;
        mShowSuggestionsListCallback = showSuggestionsListCallback;
        mIsToolbarBottomAnchoredSupplier = isToolbarBottomAnchoredSupplier;
        mOmniboxVerticalTranslationSupplier = omniboxVerticalTranslationSupplier;
        mContext = context;
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
        return !sAnimationsDisabledForTesting
                && (mIsToolbarBottomAnchoredSupplier.getAsBoolean()
                        || OmniboxFeatures.shouldAnimateSuggestionsListAppearance());
    }

    static void setAnimationsDisabledForTesting(boolean disabledForTesting) {
        sAnimationsDisabledForTesting = disabledForTesting;
    }

    private void startAnimation() {
        mStartingVerticalOffset = getStartingVerticalOffset();
        mAnimator = ValueAnimator.ofFloat(0.f, 1.f).setDuration(DURATION);
        mAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        mAnimator.addUpdateListener(this);
        mAnimator.addListener(this);
        mAnimator.start();
    }

    @Override
    public void onAnimationUpdate(ValueAnimator valueAnimator) {
        mListPropertyModel.set(SuggestionListProperties.ALPHA, valueAnimator.getAnimatedFraction());
        float verticalTranslationOfOmnibox = mOmniboxVerticalTranslationSupplier.get();
        if (verticalTranslationOfOmnibox > 0.0f
                || mListPropertyModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y) > 0.0f) {
            mListPropertyModel.set(
                    SuggestionListProperties.CHILD_TRANSLATION_Y,
                    verticalTranslationOfOmnibox
                            + mStartingVerticalOffset
                                    * (1.0f - valueAnimator.getAnimatedFraction()));
        }
    }

    @Override
    public void onAnimationStart(Animator animator) {
        mShowSuggestionsListCallback.run();
        mListPropertyModel.set(SuggestionListProperties.ALPHA, 0.0f);
        mListPropertyModel.set(
                SuggestionListProperties.CHILD_TRANSLATION_Y, mStartingVerticalOffset);
    }

    @Override
    public void onAnimationEnd(Animator animator) {
        mListPropertyModel.set(SuggestionListProperties.ALPHA, 1.0f);
        mListPropertyModel.set(SuggestionListProperties.CHILD_TRANSLATION_Y, 0.0f);
    }

    @Override
    public void onAnimationCancel(Animator animator) {
        // Show the list in case we get cancelled ahead of starting.
        mShowSuggestionsListCallback.run();
        onAnimationEnd(animator);
    }

    @Override
    public void onAnimationRepeat(Animator animator) {}

    private int getStartingVerticalOffset() {
        if (mIsToolbarBottomAnchoredSupplier.getAsBoolean()) {
            return mContext.getResources()
                    .getDimensionPixelOffset(
                            org.chromium.chrome.browser.omnibox.R.dimen
                                    .omnibox_suggestion_list_bottom_animation_starting_vertical_offset);
        } else {
            return mContext.getResources()
                    .getDimensionPixelOffset(
                            org.chromium.chrome.browser.omnibox.R.dimen
                                    .omnibox_suggestion_list_animation_added_vertical_offset);
        }
    }
}
