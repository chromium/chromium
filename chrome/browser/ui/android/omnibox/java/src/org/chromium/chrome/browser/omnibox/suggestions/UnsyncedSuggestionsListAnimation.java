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
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/**
 * {@link SuggestionsListAnimation} that manages an unsynced, fixed duration fade + translate
 * animation against the given list property model when focusing the omnibox.
 */
@NullMarked
public class UnsyncedSuggestionsListAnimation
        implements SuggestionsListAnimation, AnimatorUpdateListener, AnimatorListener {
    private static boolean sAnimationsDisabledForTesting;

    // This duration is chosen to match the duration of the IME show animation when it is unsynced,
    // which it is in our case.
    // https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/view/InsetsController.java;l=236;drc=cad0f6adc5e8ca56f9a35a20f23ddd87c13af33e
    private static final long DURATION = 150;

    private final PropertyModel mListPropertyModel;
    private final Runnable mShowSuggestionsListCallback;
    private final BooleanSupplier mIsToolbarBottomAnchoredSupplier;
    private final Supplier<Float> mOmniboxVerticalTranslationSupplier;
    private final Context mContext;
    private final BooleanSupplier mUsePopoverAnimationSupplier;
    private int mStartingVerticalOffset;
    private @Nullable OmniboxAnimator mAnimator;

    /**
     * @param listPropertyModel Property model for the suggestions list view being animated.
     * @param showSuggestionsListCallback Callback that shows the suggestions list when invoked.
     * @param isToolbarBottomAnchoredSupplier Supplier that tells us if the toolbar is
     *     bottom-anchored at the beginning of the focus animation process.
     */
    public UnsyncedSuggestionsListAnimation(
            PropertyModel listPropertyModel,
            Runnable showSuggestionsListCallback,
            BooleanSupplier isToolbarBottomAnchoredSupplier,
            Supplier<Float> omniboxVerticalTranslationSupplier,
            BooleanSupplier usePopoverAnimationSupplier,
            Context context) {

        mListPropertyModel = listPropertyModel;
        mShowSuggestionsListCallback = showSuggestionsListCallback;
        mIsToolbarBottomAnchoredSupplier = isToolbarBottomAnchoredSupplier;
        mOmniboxVerticalTranslationSupplier = omniboxVerticalTranslationSupplier;
        mUsePopoverAnimationSupplier = usePopoverAnimationSupplier;
        mContext = context;
    }

    @Override
    public void onOmniboxSessionStateChange(boolean active) {
        if (active) {
            // Started externally.
        } else if (mAnimator != null && mAnimator.isRunning()) {
            mAnimator.cancel();
            mAnimator = null;
        }
    }

    private long getDuration() {
        if (sAnimationsDisabledForTesting) return 0;
        if (OmniboxCapabilities.isDesktopPlatform()
                && !mUsePopoverAnimationSupplier.getAsBoolean()) {
            return 0;
        }
        return DURATION;
    }

    private float getStartAlpha() {
        return mUsePopoverAnimationSupplier.getAsBoolean() ? 0.0f : 1.0f;
    }

    @Override
    public OmniboxAnimator getAnimator() {
        if (mAnimator == null) {
            mAnimator = new OmniboxAnimator(getStartAlpha(), getDuration());
            mAnimator.setFloatValues(0.f, 1.f);
            mAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
            mAnimator.addUpdateListener(this);
            mAnimator.addListener(this);
        }
        return mAnimator;
    }

    static void setAnimationsDisabledForTesting(boolean disabledForTesting) {
        sAnimationsDisabledForTesting = disabledForTesting;
    }

    @Override
    public void onAnimationUpdate(ValueAnimator valueAnimator) {
        if (getDuration() == 0) return;
        mListPropertyModel.set(SuggestionListProperties.ALPHA, valueAnimator.getAnimatedFraction());
        if (mUsePopoverAnimationSupplier.getAsBoolean()) return;
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
        if (getDuration() == 0) return;
        mListPropertyModel.set(SuggestionListProperties.ALPHA, 0.0f);
        if (mUsePopoverAnimationSupplier.getAsBoolean()) return;
        mStartingVerticalOffset = getStartingVerticalOffset();
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

    public boolean isRunning() {
        return mAnimator != null && mAnimator.isRunning();
    }

    private int getStartingVerticalOffset() {
        if (mIsToolbarBottomAnchoredSupplier.getAsBoolean()) {
            return mContext.getResources()
                    .getDimensionPixelOffset(
                            R.dimen
                                    .omnibox_suggestion_list_bottom_animation_starting_vertical_offset);
        } else {
            return mContext.getResources()
                    .getDimensionPixelOffset(
                            R.dimen.omnibox_suggestion_list_animation_added_vertical_offset);
        }
    }
}
