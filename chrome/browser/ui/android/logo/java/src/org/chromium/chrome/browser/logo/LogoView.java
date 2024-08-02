// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.FloatProperty;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.animation.LinearInterpolator;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import jp.tomorrowkey.android.gifplayer.BaseGifDrawable;
import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.ui.widget.LoadingView;
import org.chromium.ui.widget.LoadingView.Observer;

/**
 * This view shows the default search provider's logo and fades in a new logo if one becomes
 * available. It also maintains a {@link BaseGifDrawable} that will be played when the user clicks
 * this view and we have an animated GIF logo ready.
 */
public class LogoView extends FrameLayout implements OnClickListener {
    // Number of milliseconds for a new logo to fade in.
    private static final int LOGO_TRANSITION_TIME_MS = 400;

    // mLogo and mNewLogo are remembered for cross fading animation.
    private Bitmap mLogo;
    private Bitmap mNewLogo;
    private Bitmap mDefaultGoogleLogo;
    private BaseGifDrawable mAnimatedLogoDrawable;

    private ObjectAnimator mFadeAnimation;
    private Paint mPaint;
    private Matrix mLogoMatrix;
    private Matrix mNewLogoMatrix;
    private Matrix mAnimatedLogoMatrix;
    private boolean mLogoIsDefault;
    private boolean mNewLogoIsDefault;
    private boolean mAnimationEnabled = true;

    private LoadingView mLoadingView;

    /**
     * A measure from 0 to 1 of how much the new logo has faded in. 0 shows the old logo, 1 shows
     * the new logo, and intermediate values show the new logo cross-fading in over the old logo.
     * Set to 0 when not transitioning.
     */
    private float mTransitionAmount;

    private ClickHandler mClickHandler;
    private Callback<LogoBridge.Logo> mOnLogoAvailableCallback;
    private boolean mIsLogoPolishFlagEnabled;
    private int mLogoSizeForLogoPolish;

    private final FloatProperty<LogoView> mTransitionProperty =
            new FloatProperty<LogoView>("") {
                @Override
                public Float get(LogoView logoView) {
                    return logoView.mTransitionAmount;
                }

                @Override
                public void setValue(LogoView logoView, float amount) {
                    assert amount >= 0f;
                    assert amount <= 1f;
                    if (logoView.mTransitionAmount != amount) {
                        logoView.mTransitionAmount = amount;
                        invalidate();
                    }
                }
            };

    /** Handles tasks for the {@link LogoView} shown on an NTP.*/
    @FunctionalInterface
    interface ClickHandler {
        /**
         * Called when the user clicks on the logo.
         * @param isAnimatedLogoShowing Whether the animated GIF logo is playing.
         */
        void onLogoClicked(boolean isAnimatedLogoShowing);
    }

    /** Constructor used to inflate a LogoView from XML.*/
    public LogoView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mLogoMatrix = new Matrix();
        mLogoIsDefault = true;

        mPaint = new Paint();
        mPaint.setFilterBitmap(true);

        // Mark this view as non-clickable so that accessibility will ignore it. When a non-default
        // logo is shown, this view will be marked clickable again.
        setOnClickListener(this);
        setClickable(false);
        setWillNotDraw(false);

        mLoadingView = new LoadingView(getContext());
        LayoutParams lp = new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.CENTER;
        mLoadingView.setLayoutParams(lp);
        mLoadingView.setVisibility(View.GONE);
        addView(mLoadingView);
    }

    /** Clean up member variables when this view is no longer needed.*/
    void destroy() {
        // Need to end the animation otherwise it can cause memory leaks since the AnimationHandler
        // has a reference to the animation callback which then can link back to the
        // {@code mTransitionProperty}.
        endFadeAnimation();
        mLoadingView.destroy();
    }

    /** Sets the {@link ClickHandler} to notify when the logo is pressed.*/
    void setClickHandler(ClickHandler clickHandler) {
        mClickHandler = clickHandler;
    }

    /** Sets the onLogoAvailableCallback to notify when the new logo has faded in. */
    void setLogoAvailableCallback(Callback<Logo> onLogoAvailableCallback) {
        mOnLogoAvailableCallback = onLogoAvailableCallback;
    }

    /** Sets the isLogoPolishFlagEnabled to determine if logo polish flag is enabled. */
    void setLogoPolishFlagEnabled(boolean isLogoPolishFlagEnabled) {
        mIsLogoPolishFlagEnabled = isLogoPolishFlagEnabled;
    }

    /**
     * Sets the logo size to use when logo polish is enabled. When logo polish is disabled, this
     * value should be invalid.
     */
    void setLogoSizeForLogoPolish(int logoSizeForLogoPolish) {
        mLogoSizeForLogoPolish = logoSizeForLogoPolish;
    }

    /** Jumps to the end of the logo cross-fading animation, if any. */
    void endFadeAnimation() {
        if (mFadeAnimation != null) {
            mFadeAnimation.end();
            mFadeAnimation = null;
        }
    }

    /** @return True after we receive an animated logo from the server.*/
    private boolean isAnimatedLogoShowing() {
        return mAnimatedLogoDrawable != null;
    }

    /** Starts playing the given animated GIF logo.*/
    void playAnimatedLogo(BaseGifImage gifImage) {
        mLoadingView.hideLoadingUI();
        mAnimatedLogoDrawable = new BaseGifDrawable(gifImage, Config.ARGB_8888);
        mAnimatedLogoMatrix = new Matrix();
        setMatrix(
                mAnimatedLogoDrawable.getIntrinsicWidth(),
                mAnimatedLogoDrawable.getIntrinsicHeight(),
                mAnimatedLogoMatrix,
                false);
        // Set callback here to ensure #invalidateDrawable() is called.
        mAnimatedLogoDrawable.setCallback(this);
        mAnimatedLogoDrawable.start();
    }

    /** Show a spinning progressbar.*/
    void showLoadingView() {
        mLogo = null;
        invalidate();
        mLoadingView.showLoadingUI();
    }

    /**
     * Show a loading indicator or a baked-in default search provider logo, based on what is
     * available.
     */
    void showSearchProviderInitialView() {
        if (maybeShowDefaultLogo()) return;

        showLoadingView();
    }

    /**
     * Fades in a new logo over the current logo.
     *
     * @param logo The new logo to fade in.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void updateLogo(Logo logo) {
        if (logo == null) {
            if (!maybeShowDefaultLogo()) {
                mLogo = null;
                invalidate();
            }

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
        updateLogoImpl(
                logo.image,
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

    private void updateLogoImpl(
            Bitmap logo,
            final String contentDescription,
            boolean isDefaultLogo,
            boolean isClickable,
            @Nullable Runnable onAnimationFinished) {
        assert logo != null;

        if (mFadeAnimation != null) mFadeAnimation.end();

        mLoadingView.hideLoadingUI();

        // Don't crossfade if the new logo is the same as the old one.
        if (mLogo == logo) return;

        mNewLogo = logo;
        mNewLogoMatrix = new Matrix();
        mNewLogoIsDefault = isDefaultLogo;

        MarginLayoutParams logoViewLayoutParams = (MarginLayoutParams) getLayoutParams();
        int oldLogoHeight = logoViewLayoutParams.height;
        int oldLogoTopMargin = logoViewLayoutParams.topMargin;
        int[] newLogoViewLayoutParams =
                LogoUtils.getLogoViewLayoutParams(
                        getResources(),
                        mIsLogoPolishFlagEnabled && !isDefaultLogo,
                        mLogoSizeForLogoPolish);
        int newLogoHeight = newLogoViewLayoutParams[0];
        int newLogoTopMargin = newLogoViewLayoutParams[1];

        setMatrix(mNewLogo.getWidth(), mNewLogo.getHeight(), mNewLogoMatrix, mNewLogoIsDefault);

        mFadeAnimation = ObjectAnimator.ofFloat(this, mTransitionProperty, 0f, 1f);
        mFadeAnimation.setInterpolator(new LinearInterpolator());
        mFadeAnimation.setDuration(mAnimationEnabled ? LOGO_TRANSITION_TIME_MS : 0);
        mFadeAnimation.addUpdateListener(
                new ValueAnimator.AnimatorUpdateListener() {
                    @Override
                    public void onAnimationUpdate(ValueAnimator animation) {
                        if (!ChromeFeatureList.sLogoPolishAnimationKillSwitch.isEnabled()
                                || newLogoHeight == oldLogoHeight) return;

                        float animationValue = (Float) animation.getAnimatedValue();
                        if (animationValue <= 0.5f) {
                            return;
                        }

                        // Interpolate height
                        int logoHeight =
                                Math.round(
                                        (oldLogoHeight
                                                + (newLogoHeight - oldLogoHeight)
                                                        * 2
                                                        * (animationValue - 0.5f)));

                        // Interpolate top margin
                        int logoTopMargin =
                                Math.round(
                                        (oldLogoTopMargin
                                                + (newLogoTopMargin - oldLogoTopMargin)
                                                        * 2
                                                        * (animationValue - 0.5f)));

                        LogoUtils.setLogoViewLayoutParams(LogoView.this, logoHeight, logoTopMargin);
                    }
                });
        mFadeAnimation.addListener(
                new Animator.AnimatorListener() {
                    @Override
                    public void onAnimationStart(Animator animation) {}

                    @Override
                    public void onAnimationRepeat(Animator animation) {}

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mLogo = mNewLogo;
                        mLogoMatrix = mNewLogoMatrix;
                        mLogoIsDefault = mNewLogoIsDefault;
                        mNewLogo = null;
                        mNewLogoMatrix = null;
                        mTransitionAmount = 0f;
                        mFadeAnimation = null;
                        if (newLogoHeight != oldLogoHeight) {
                            LogoUtils.setLogoViewLayoutParams(
                                    LogoView.this, newLogoHeight, newLogoTopMargin);
                        }
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
                        invalidate();
                    }
                });
        mFadeAnimation.start();
    }

    void setDefaultGoogleLogo(Bitmap defaultGoogleLogo) {
        mDefaultGoogleLogo = defaultGoogleLogo;
    }

    /**
     * Shows the default search engine logo if available.
     * @return Whether the default search engine logo is available.
     */
    private boolean maybeShowDefaultLogo() {
        if (mDefaultGoogleLogo != null) {
            updateLogoImpl(
                    mDefaultGoogleLogo,
                    /* contentDescription= */ null,
                    /* isDefaultLogo= */ true,
                    /* isClickable= */ false,
                    /* onAnimationFinished= */ null);
            return true;
        }
        return false;
    }

    /** @return Whether a new logo is currently fading in over the old logo.*/
    private boolean isTransitioning() {
        return mTransitionAmount != 0f;
    }

    /**
     * Sets the matrix to scale and translate the image so that it will be centered in the LogoView
     * and scaled to fit within the LogoView.
     *
     * @param preventUpscaling Whether the image should not be scaled up. If true, the image might
     *     not fill the entire view but will still be centered.
     */
    private void setMatrix(
            int imageWidth, int imageHeight, Matrix matrix, boolean preventUpscaling) {
        int width = getWidth();
        int height = getHeight();

        float scale = Math.min((float) width / imageWidth, (float) height / imageHeight);
        if (preventUpscaling) scale = Math.min(1.0f, scale);

        int imageOffsetX = Math.round((width - imageWidth * scale) * 0.5f);

        float whitespace = height - imageHeight * scale;
        int imageOffsetY = Math.round(whitespace * 0.5f);

        matrix.setScale(scale, scale);
        matrix.postTranslate(imageOffsetX, imageOffsetY);
    }

    @Override
    protected boolean verifyDrawable(Drawable who) {
        return (who == mAnimatedLogoDrawable) || super.verifyDrawable(who);
    }

    @Override
    public void invalidateDrawable(Drawable drawable) {
        // mAnimatedLogoDrawable doesn't actually know its bounds, so super.invalidateDrawable()
        // doesn't invalidate the right area. Instead invalidate the entire view; the drawable takes
        // up most of the view anyway so this is just as efficient.
        // @see ImageView#invalidateDrawable().
        if (drawable == mAnimatedLogoDrawable) {
            invalidate();
        } else {
            super.invalidateDrawable(drawable);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (isAnimatedLogoShowing()) {
            if (mFadeAnimation != null) mFadeAnimation.cancel();
            // Free the old bitmaps to allow them to be GC'd.
            mLogo = null;
            mNewLogo = null;

            canvas.save();
            canvas.concat(mAnimatedLogoMatrix);
            mAnimatedLogoDrawable.draw(canvas);
            canvas.restore();
        } else {
            if (mLogo != null && mTransitionAmount < 0.5f) {
                mPaint.setAlpha((int) (255 * 2 * (0.5f - mTransitionAmount)));
                canvas.save();
                canvas.concat(mLogoMatrix);
                canvas.drawBitmap(mLogo, 0, 0, mPaint);
                canvas.restore();
            }

            if (mNewLogo != null && mTransitionAmount > 0.5f) {
                if (ChromeFeatureList.sLogoPolishAnimationKillSwitch.isEnabled()) {
                    mPaint.setAlpha((int) (255 * Math.pow(2 * (mTransitionAmount - 0.5f), 3)));
                } else {
                    mPaint.setAlpha((int) (255 * 2 * (mTransitionAmount - 0.5f)));
                }
                canvas.save();
                canvas.concat(mNewLogoMatrix);
                canvas.drawBitmap(mNewLogo, 0, 0, mPaint);
                canvas.restore();
            }
        }
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (w != oldw || h != oldh) {
            if (mAnimatedLogoDrawable != null) {
                setMatrix(
                        mAnimatedLogoDrawable.getIntrinsicWidth(),
                        mAnimatedLogoDrawable.getIntrinsicHeight(),
                        mAnimatedLogoMatrix,
                        false);
            }
            if (mLogo != null) {
                setMatrix(mLogo.getWidth(), mLogo.getHeight(), mLogoMatrix, mLogoIsDefault);
            }
            if (mNewLogo != null) {
                setMatrix(
                        mNewLogo.getWidth(),
                        mNewLogo.getHeight(),
                        mNewLogoMatrix,
                        mNewLogoIsDefault);
            }
        }
    }

    @Override
    public void onClick(View view) {
        if (view == this && mClickHandler != null && !isTransitioning()) {
            mClickHandler.onLogoClicked(isAnimatedLogoShowing());
        }
    }

    public void endAnimationsForTesting() {
        mFadeAnimation.end();
    }

    ObjectAnimator getFadeAnimationForTesting() {
        return mFadeAnimation;
    }

    Bitmap getNewLogoForTesting() {
        return mNewLogo;
    }

    Bitmap getLogoForTesting() {
        return mLogo;
    }

    boolean getAnimationEnabledForTesting() {
        return mAnimationEnabled;
    }

    boolean checkLoadingViewObserverEmptyForTesting() {
        return mLoadingView.isObserverListEmpty();
    }

    void addLoadingViewObserverForTesting(Observer listener) {
        mLoadingView.addObserver(listener);
    }

    ClickHandler getClickHandlerForTesting() {
        return mClickHandler;
    }

    Bitmap getDefaultGoogleLogoForTesting() {
        return mDefaultGoogleLogo;
    }

    int getLoadingViewVisibilityForTesting() {
        return mLoadingView.getVisibility();
    }

    void setLoadingViewVisibilityForTesting(int visibility) {
        mLoadingView.setVisibility(visibility);
    }

    void setIsLogoPolishFlagEnabledForTesting(boolean isLogoPolishFlagEnabled) {
        mIsLogoPolishFlagEnabled = isLogoPolishFlagEnabled;
    }

    boolean getIsLogoPolishFlagEnabledForTesting() {
        return mIsLogoPolishFlagEnabled;
    }

    int getLogoSizeForLogoPolishForTesting() {
        return mLogoSizeForLogoPolish;
    }
}
