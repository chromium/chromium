// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.animation.AnimationListeners;
import org.chromium.ui.interpolators.Interpolators;

import java.util.List;
import java.util.Random;

/** View for rendering illustration card items in the AtMemory bottom sheet list. */
@NullMarked
public class AtMemoryBottomSheetIllustrationCardView extends LinearLayout {
    static final long TITLE_ROTATION_INTERVAL_MS = 3000L;
    private static final long CROSSFADE_DURATION_MS = 150L;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Random mRandom = new Random();
    private final AnimationHandler mTitleAnimationHandler = new AnimationHandler();

    private boolean mIsRotating;
    private int mNextRotatingPlaceholderIndex;
    private List<String> mRotatingPlaceholders = List.of();
    private TextView mSubtitleView;
    private TextView mTitleView;

    public AtMemoryBottomSheetIllustrationCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.illustration_card_title);
        mSubtitleView = findViewById(R.id.illustration_card_subtitle);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        startPlaceholderRotation();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        stopPlaceholderRotation();
        mTitleAnimationHandler.forceFinishAnimation();
    }

    public void setTitle(@Nullable String title) {
        mTitleAnimationHandler.forceFinishAnimation();

        if (TextUtils.isEmpty(title) || TextUtils.isEmpty(mTitleView.getText())) {
            mTitleView.setText(title);
            mTitleView.setVisibility(TextUtils.isEmpty(title) ? GONE : VISIBLE);
            return;
        }

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playSequentially(buildFadeOutAnimation(title), buildFadeInAnimation());
        mTitleAnimationHandler.startAnimation(animatorSet);
    }

    public void setSubtitle(@Nullable String subtitle) {
        mSubtitleView.setText(subtitle);
        mSubtitleView.setVisibility(TextUtils.isEmpty(subtitle) ? GONE : VISIBLE);
    }

    public void setRotatingPlaceholders(List<String> rotatingPlaceholders) {
        mRotatingPlaceholders = rotatingPlaceholders;
        stopPlaceholderRotation();
        startPlaceholderRotation();
    }

    private void startPlaceholderRotation() {
        if (mRotatingPlaceholders.isEmpty() || mIsRotating) {
            return;
        }
        mIsRotating = true;
        mNextRotatingPlaceholderIndex = mRandom.nextInt(mRotatingPlaceholders.size());
        mHandler.postDelayed(this::setPlaceholderText, TITLE_ROTATION_INTERVAL_MS);
    }

    private void stopPlaceholderRotation() {
        if (!mIsRotating) {
            return;
        }
        mIsRotating = false;
        mHandler.removeCallbacksAndMessages(null);
    }

    private void setPlaceholderText() {
        if (mRotatingPlaceholders.isEmpty()) {
            return;
        }
        setTitle(mRotatingPlaceholders.get(mNextRotatingPlaceholderIndex));
        mNextRotatingPlaceholderIndex =
                (mNextRotatingPlaceholderIndex + 1) % mRotatingPlaceholders.size();
        mHandler.postDelayed(this::setPlaceholderText, TITLE_ROTATION_INTERVAL_MS);
    }

    private Animator buildFadeInAnimation() {
        Animator fadeInAnimation = ObjectAnimator.ofFloat(mTitleView, View.ALPHA, 0f, 1f);
        fadeInAnimation.setDuration(CROSSFADE_DURATION_MS);
        fadeInAnimation.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        return fadeInAnimation;
    }

    private Animator buildFadeOutAnimation(String title) {
        Animator fadeOutAnimation = ObjectAnimator.ofFloat(mTitleView, View.ALPHA, 1f, 0f);
        fadeOutAnimation.setDuration(CROSSFADE_DURATION_MS);
        fadeOutAnimation.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        fadeOutAnimation.addListener(
                AnimationListeners.onAnimationEnd(() -> mTitleView.setText(title)));
        return fadeOutAnimation;
    }
}
