// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.drawable.AnimatedImageDrawable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.transition.ChangeBounds;
import android.transition.TransitionManager;
import android.util.AttributeSet;
import android.util.FloatProperty;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.animation.LinearInterpolator;
import android.widget.ImageView;

import androidx.annotation.VisibleForTesting;

import jp.tomorrowkey.android.gifplayer.BaseGifDrawable;
import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;

/**
 * This view shows the default search provider's logo and fades in a new logo if one becomes
 * available. It also maintains a {@link BaseGifDrawable} that will be played when the user clicks
 * this view and we have an animated GIF logo ready.
 */
@NullMarked
public class LogoView extends ImageView implements OnClickListener {
    // Number of milliseconds for a new logo to fade in.
    private static final int LOGO_TRANSITION_TIME_MS = 400;

    // mLogo and mNewLogo are remembered for cross fading animation.
    private @Nullable Drawable mLogoDrawable;
    private @Nullable Drawable mNewLogoDrawable;
    private @Nullable Drawable mDefaultGoogleLogoDrawable;
    private @Nullable Drawable mAnimatedLogoDrawable;

    private @Nullable ObjectAnimator mFadeAnimation;
    private boolean mLogoIsDefault;
    private boolean mNewLogoIsDefault;
    private boolean mAnimationEnabled = true;

    /**
     * A measure from 0 to 1 of how much the new logo has faded in. 0 shows the old logo, 1 shows
     * the new logo, and intermediate values show the new logo cross-fading in over the old logo.
     * Set to 0 when not transitioning.
     */
    private float mTransitionAmount;

    private LogoProperties.@Nullable ClickHandler mClickHandler;
    private @Nullable Callback<LogoBridge.Logo> mOnLogoAvailableCallback;
    private int mDoodleSize;

    private final FloatProperty<LogoView> mTransitionProperty =
            new FloatProperty<>("") {
                @Override
                public Float get(LogoView logoView) {
                    return logoView.mTransitionAmount;
                }

                @Override
                public void setValue(LogoView logoView, float amount) {
                    assert amount >= 0f;
                    assert amount <= 1f;
                    logoView.mTransitionAmount = amount;
                    if (amount <= 0.5f) {
                        logoView.setAlpha(1.0f - amount * 2.0f);
                    } else {
                        if (logoView.mNewLogoDrawable != null
                                && logoView.getDrawable() != logoView.mNewLogoDrawable) {
                            logoView.setImageDrawable(logoView.mNewLogoDrawable);
                        }
                        logoView.setAlpha((amount - 0.5f) * 2.0f);
                    }
                }
            };

    /** Constructor used to inflate a LogoView from XML. */
    public LogoView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mLogoIsDefault = true;

        // Mark this view as non-clickable so that accessibility will ignore it. When a non-default
        // logo is shown, this view will be marked clickable again.
        setOnClickListener(this);
        setClickable(false);
        setWillNotDraw(false);
    }

    /** Clean up member variables when this view is no longer needed.*/
    void destroy() {
        // Need to end the animation otherwise it can cause memory leaks since the AnimationHandler
        // has a reference to the animation callback which then can link back to the
        // {@code mTransitionProperty}.
        endFadeAnimation();
    }

    /** Clears the logo drawable and stops drawing it. */
    void clearLogo() {
        endFadeAnimation();
        mLogoDrawable = null;
        mNewLogoDrawable = null;
        setImageDrawable(null);
    }

    /** Sets the {@link LogoProperties.ClickHandler} to notify when the logo is pressed. */
    void setClickHandler(LogoProperties.ClickHandler clickHandler) {
        mClickHandler = clickHandler;
    }

    /** Sets the onLogoAvailableCallback to notify when the new logo has faded in. */
    void setLogoAvailableCallback(Callback<Logo> onLogoAvailableCallback) {
        mOnLogoAvailableCallback = onLogoAvailableCallback;
    }

    /**
     * Sets the logo size to use when logo is a google doodle. When logo is not a google doodle,
     * this value should be invalid.
     */
    void setDoodleSize(int doodleSize) {
        mDoodleSize = doodleSize;
    }

    /**
     * Sets the logo top margin.
     *
     * @param topMargin The top margin in pixels.
     */
    void setLogoTopMargin(int topMargin) {
        MarginLayoutParams marginLayoutParams = (MarginLayoutParams) getLayoutParams();
        marginLayoutParams.topMargin = topMargin;
        setLayoutParams(marginLayoutParams);
    }

    /**
     * Sets the logo height.
     *
     * @param height The height of the logo in pixels.
     */
    void setLogoHeight(int height) {
        MarginLayoutParams marginLayoutParams = (MarginLayoutParams) getLayoutParams();
        marginLayoutParams.height = height;
        setLayoutParams(marginLayoutParams);
    }

    /** Jumps to the end of the logo cross-fading animation, if any. */
    void endFadeAnimation() {
        if (mFadeAnimation != null) {
            mFadeAnimation.end();
            mFadeAnimation = null;
        }
    }

    /**
     * @return True after we receive an animated logo from the server.
     */
    private boolean isAnimatedLogoShowing() {
        return mAnimatedLogoDrawable != null;
    }

    /** Starts playing the given animated GIF logo. */
    // TODO(crbug.com/434200490): Replace Object reference with ImageDecoder.Source when the
    // refactoring is fully rolled out.
    void playAnimatedLogo(Object animatedLogo) {
        if (animatedLogo instanceof BaseGifImage) {
            mAnimatedLogoDrawable =
                    new BaseGifDrawable((BaseGifImage) animatedLogo, Config.ARGB_8888);
        } else if (animatedLogo instanceof AnimatedImageDrawable) {
            mAnimatedLogoDrawable = (AnimatedImageDrawable) animatedLogo;
        } else {
            assert false : "Unexpected logo type: " + animatedLogo;
            return;
        }

        setImageDrawable(mAnimatedLogoDrawable);
        setScaleType(ScaleType.FIT_CENTER);
        setAlpha(1.0f);

        if (mAnimatedLogoDrawable instanceof BaseGifDrawable) {
            ((BaseGifDrawable) mAnimatedLogoDrawable).start();
        } else if (mAnimatedLogoDrawable instanceof AnimatedImageDrawable) {
            ((AnimatedImageDrawable) mAnimatedLogoDrawable).start();
        }
    }

    /**
     * Fades in a new logo over the current logo.
     *
     * @param logo The new logo to fade in.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void updateLogo(Logo logo) {
        if (logo == null) {
            clearLogo();

            if (mOnLogoAvailableCallback != null) {
                mOnLogoAvailableCallback.onResult(logo);
            }
            return;
        }

        String contentDescription =
                TextUtils.isEmpty(logo.altText)
                        ? null
                        : getResources()
                                .getString(R.string.accessibility_google_doodle, logo.altText);
        Runnable onAnimationFinished = null;
        if (mOnLogoAvailableCallback != null) {
            onAnimationFinished = mOnLogoAvailableCallback.bind(logo);
        }
        updateLogoDrawableImpl(
                new BitmapDrawable(getResources(), logo.image),
                contentDescription,
                /* isDefaultLogo= */ false,
                isLogoClickable(logo),
                onAnimationFinished);
    }

    void setAnimationEnabled(boolean animationEnabled) {
        mAnimationEnabled = animationEnabled;
    }

    private static boolean isLogoClickable(Logo logo) {
        return !TextUtils.isEmpty(logo.animatedLogoUrl) || !TextUtils.isEmpty(logo.onClickUrl);
    }

    private void updateLogoDrawableImpl(
            Drawable logoDrawable,
            final @Nullable String contentDescription,
            boolean isDefaultLogo,
            boolean isClickable,
            @Nullable Runnable onAnimationFinished) {
        assert logoDrawable != null;

        if (mFadeAnimation != null) mFadeAnimation.end();

        mAnimatedLogoDrawable = null;

        // Don't crossfade if the new logoDrawable is the same as the old one.
        if (mLogoDrawable == logoDrawable) return;

        mNewLogoDrawable = logoDrawable;
        mNewLogoIsDefault = isDefaultLogo;

        setScaleType(isDefaultLogo ? ScaleType.CENTER_INSIDE : ScaleType.FIT_CENTER);

        MarginLayoutParams logoViewLayoutParams = (MarginLayoutParams) getLayoutParams();
        int oldLogoHeight = logoViewLayoutParams.height;
        int oldLogoTopMargin = logoViewLayoutParams.topMargin;
        int[] newLogoViewLayoutParams =
                LogoUtils.getLogoViewLayoutParams(getResources(), !isDefaultLogo, mDoodleSize);
        int newLogoHeight = newLogoViewLayoutParams[0];
        int newLogoTopMargin = newLogoViewLayoutParams[1];

        if (newLogoHeight != oldLogoHeight || newLogoTopMargin != oldLogoTopMargin) {
            ChangeBounds boundsTransition = new ChangeBounds();
            boundsTransition.setStartDelay(mAnimationEnabled ? (LOGO_TRANSITION_TIME_MS / 2) : 0);
            boundsTransition.setDuration(mAnimationEnabled ? (LOGO_TRANSITION_TIME_MS / 2) : 0);

            TransitionManager.beginDelayedTransition((ViewGroup) getParent(), boundsTransition);
            LogoUtils.setLogoViewLayoutParamsForDoodle(
                    LogoView.this, newLogoHeight, newLogoTopMargin);
        }

        mFadeAnimation = ObjectAnimator.ofFloat(this, mTransitionProperty, 0f, 1f);
        mFadeAnimation.setInterpolator(new LinearInterpolator());
        mFadeAnimation.setDuration(mAnimationEnabled ? LOGO_TRANSITION_TIME_MS : 0);
        mFadeAnimation.addListener(
                new Animator.AnimatorListener() {
                    @Override
                    public void onAnimationStart(Animator animation) {}

                    @Override
                    public void onAnimationRepeat(Animator animation) {}

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mLogoDrawable = mNewLogoDrawable;
                        if (mNewLogoDrawable != null) {
                            setImageDrawable(mNewLogoDrawable);
                            setAlpha(1.0f);
                        }
                        mLogoIsDefault = mNewLogoIsDefault;
                        mNewLogoDrawable = null;
                        mTransitionAmount = 0f;
                        mFadeAnimation = null;
                        setContentDescription(contentDescription);
                        setClickable(isClickable);
                        setFocusable(isClickable || !TextUtils.isEmpty(contentDescription));
                        if (!isDefaultLogo && onAnimationFinished != null) {
                            onAnimationFinished.run();
                        }
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                        onAnimationEnd(animation);
                    }
                });
        mFadeAnimation.start();
    }

    void setDefaultGoogleLogoDrawable(Drawable defaultGoogleLogoDrawable) {
        mDefaultGoogleLogoDrawable = defaultGoogleLogoDrawable;
    }

    /**
     * Shows the default search engine logo if available.
     *
     * @return Whether the default search engine logo drawable is available.
     */
    boolean maybeShowDefaultLogoDrawable() {
        if (mDefaultGoogleLogoDrawable != null) {
            updateLogoDrawableImpl(
                    mDefaultGoogleLogoDrawable,
                    /* contentDescription= */ null,
                    /* isDefaultLogo= */ true,
                    /* isClickable= */ false,
                    /* onAnimationFinished= */ null);
            return true;
        }
        return false;
    }

    /**
     * @return Whether a new logo is currently fading in over the old logo.
     */
    private boolean isTransitioning() {
        return mTransitionAmount != 0f;
    }

    @Override
    public void onClick(View view) {
        if (view == this && mClickHandler != null && !isTransitioning()) {
            mClickHandler.onLogoClicked(isAnimatedLogoShowing());
        }
    }

    void endAnimationsForTesting() {
        if (mFadeAnimation != null) mFadeAnimation.end();
    }

    @Nullable ObjectAnimator getFadeAnimationForTesting() {
        return mFadeAnimation;
    }

    @Nullable Bitmap getNewLogoDrawableBitmapForTesting() {
        if (mNewLogoDrawable instanceof BitmapDrawable) {
            return ((BitmapDrawable) mNewLogoDrawable).getBitmap();
        }
        return null;
    }

    @Nullable Drawable getLogoDrawableForTesting() {
        return mLogoDrawable;
    }

    boolean getAnimationEnabledForTesting() {
        return mAnimationEnabled;
    }

    LogoProperties.@Nullable ClickHandler getClickHandlerForTesting() {
        return mClickHandler;
    }

    @Nullable Drawable getDefaultGoogleLogoDrawableForTesting() {
        return mDefaultGoogleLogoDrawable;
    }

    int getDoodleSizeForTesting() {
        return mDoodleSize;
    }
}
