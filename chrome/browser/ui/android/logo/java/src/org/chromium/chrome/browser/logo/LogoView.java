// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.drawable.AnimatedImageDrawable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.FloatProperty;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.animation.LinearInterpolator;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import jp.tomorrowkey.android.gifplayer.BaseGifDrawable;
import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.ui.widget.LoadingView;

/**
 * This view shows the default search provider's logo and fades in a new logo if one becomes
 * available. It also maintains a {@link BaseGifDrawable} that will be played when the user clicks
 * this view and we have an animated GIF logo ready.
 */
@NullMarked
public class LogoView extends FrameLayout implements OnClickListener {
    // Number of milliseconds for a new logo to fade in.
    private static final int LOGO_TRANSITION_TIME_MS = 400;

    // mLogo and mNewLogo are remembered for cross fading animation.
    private @Nullable Bitmap mLogo;
    private @Nullable Bitmap mNewLogo;
    private @Nullable Bitmap mDefaultGoogleLogo;
    private @Nullable Drawable mLogoDrawable;
    private @Nullable Drawable mNewLogoDrawable;
    private @Nullable Drawable mDefaultGoogleLogoDrawable;
    private @Nullable Drawable mAnimatedLogoDrawable;

    private @Nullable ObjectAnimator mFadeAnimation;
    private final Paint mPaint;
    private @Nullable Matrix mLogoMatrix;
    private @Nullable Matrix mNewLogoMatrix;
    private @Nullable Matrix mAnimatedLogoMatrix;
    private boolean mLogoIsDefault;
    private boolean mNewLogoIsDefault;
    private boolean mAnimationEnabled = true;

    private final LoadingView mLoadingView;
    private final boolean mIsRefactorEnabled;

    /**
     * A measure from 0 to 1 of how much the new logo has faded in. 0 shows the old logo, 1 shows
     * the new logo, and intermediate values show the new logo cross-fading in over the old logo.
     * Set to 0 when not transitioning.
     */
    private float mTransitionAmount;

    private @Nullable ClickHandler mClickHandler;
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
        mIsRefactorEnabled = ChromeFeatureList.sAndroidLogoViewRefactor.isEnabled();

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

    /**
     * Sets the logo size to use when logo is a google doodle. When logo is not a google doodle,
     * this value should be invalid.
     */
    void setDoodleSize(int doodleSize) {
        mDoodleSize = doodleSize;
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
        mLoadingView.hideLoadingUi();

        if (animatedLogo instanceof BaseGifImage) {
            mAnimatedLogoDrawable =
                    new BaseGifDrawable((BaseGifImage) animatedLogo, Config.ARGB_8888);
        } else if (animatedLogo instanceof AnimatedImageDrawable) {
            mAnimatedLogoDrawable = (AnimatedImageDrawable) animatedLogo;
        } else {
            assert false : "Unexpected logo type: " + animatedLogo;
            return;
        }

        mAnimatedLogoMatrix = new Matrix();
        setMatrix(
                mAnimatedLogoDrawable.getIntrinsicWidth(),
                mAnimatedLogoDrawable.getIntrinsicHeight(),
                mAnimatedLogoMatrix,
                false);
        // Set callback here to ensure #invalidateDrawable() is called.
        mAnimatedLogoDrawable.setCallback(this);
        if (mAnimatedLogoDrawable instanceof BaseGifDrawable) {
            ((BaseGifDrawable) mAnimatedLogoDrawable).start();
        } else if (mAnimatedLogoDrawable instanceof AnimatedImageDrawable) {
            ((AnimatedImageDrawable) mAnimatedLogoDrawable).start();
        }
    }

    /** Show a spinning progressbar. */
    void showLoadingView() {
        mLogo = null;
        mLogoDrawable = null;
        invalidate();
        mLoadingView.showLoadingUi();
    }

    /**
     * Show a loading indicator or a baked-in default search provider logo, based on what is
     * available.
     */
    void showSearchProviderInitialView() {
        boolean isLogoAvailable;
        if (mIsRefactorEnabled) {
            isLogoAvailable = maybeShowDefaultLogoDrawable();
        } else {
            isLogoAvailable = maybeShowDefaultLogo();
        }
        if (isLogoAvailable) return;

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
            boolean isLogoAvailable;
            if (mIsRefactorEnabled) {
                isLogoAvailable = maybeShowDefaultLogoDrawable();
            } else {
                isLogoAvailable = maybeShowDefaultLogo();
            }

            if (!isLogoAvailable) {
                mLogo = null;
                mLogoDrawable = null;
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
        if (mIsRefactorEnabled) {
            updateLogoDrawableImpl(
                    new BitmapDrawable(getResources(), logo.image),
                    contentDescription,
                    /* isDefaultLogo= */ false,
                    isLogoClickable(logo),
                    onAnimationFinished);
        } else {
            updateLogoImpl(
                    logo.image,
                    contentDescription,
                    /* isDefaultLogo= */ false,
                    isLogoClickable(logo),
                    onAnimationFinished);
        }
    }

    void setAnimationEnabled(boolean animationEnabled) {
        mAnimationEnabled = animationEnabled;
    }

    private static boolean isLogoClickable(Logo logo) {
        return !TextUtils.isEmpty(logo.animatedLogoUrl) || !TextUtils.isEmpty(logo.onClickUrl);
    }

    private void updateLogoImpl(
            Bitmap logo,
            final @Nullable String contentDescription,
            boolean isDefaultLogo,
            boolean isClickable,
            @Nullable Runnable onAnimationFinished) {
        assert logo != null;

        if (mFadeAnimation != null) mFadeAnimation.end();

        mLoadingView.hideLoadingUi();

        // Don't crossfade if the new logo is the same as the old one.
        if (mLogo == logo) return;

        mNewLogo = logo;
        mNewLogoMatrix = new Matrix();
        mNewLogoIsDefault = isDefaultLogo;

        MarginLayoutParams logoViewLayoutParams = (MarginLayoutParams) getLayoutParams();
        int oldLogoHeight = logoViewLayoutParams.height;
        int oldLogoTopMargin = logoViewLayoutParams.topMargin;
        int[] newLogoViewLayoutParams =
                LogoUtils.getLogoViewLayoutParams(getResources(), !isDefaultLogo, mDoodleSize);
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
                        if (newLogoHeight == oldLogoHeight) return;

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

                        LogoUtils.setLogoViewLayoutParamsForDoodle(
                                LogoView.this, logoHeight, logoTopMargin);
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
                            LogoUtils.setLogoViewLayoutParamsForDoodle(
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

    private void updateLogoDrawableImpl(
            Drawable logoDrawable,
            final @Nullable String contentDescription,
            boolean isDefaultLogo,
            boolean isClickable,
            @Nullable Runnable onAnimationFinished) {
        assert logoDrawable != null;

        if (mFadeAnimation != null) mFadeAnimation.end();

        mLoadingView.hideLoadingUi();

        // Don't crossfade if the new logoDrawable is the same as the old one.
        if (mLogoDrawable == logoDrawable) return;

        mNewLogoDrawable = logoDrawable;
        mNewLogoIsDefault = isDefaultLogo;

        MarginLayoutParams logoViewLayoutParams = (MarginLayoutParams) getLayoutParams();
        int oldLogoHeight = logoViewLayoutParams.height;
        int oldLogoTopMargin = logoViewLayoutParams.topMargin;
        int[] newLogoViewLayoutParams =
                LogoUtils.getLogoViewLayoutParams(getResources(), !isDefaultLogo, mDoodleSize);
        int newLogoHeight = newLogoViewLayoutParams[0];
        int newLogoTopMargin = newLogoViewLayoutParams[1];

        setLogoBounds(mNewLogoDrawable, mNewLogoIsDefault);

        mFadeAnimation = ObjectAnimator.ofFloat(this, mTransitionProperty, 0f, 1f);
        mFadeAnimation.setInterpolator(new LinearInterpolator());
        mFadeAnimation.setDuration(mAnimationEnabled ? LOGO_TRANSITION_TIME_MS : 0);
        mFadeAnimation.addUpdateListener(
                new ValueAnimator.AnimatorUpdateListener() {
                    @Override
                    public void onAnimationUpdate(ValueAnimator animation) {
                        if (newLogoHeight == oldLogoHeight) return;

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

                        LogoUtils.setLogoViewLayoutParamsForDoodle(
                                LogoView.this, logoHeight, logoTopMargin);
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
                        mLogoDrawable = mNewLogoDrawable;
                        mLogoIsDefault = mNewLogoIsDefault;
                        mNewLogoDrawable = null;
                        mTransitionAmount = 0f;
                        mFadeAnimation = null;
                        if (newLogoHeight != oldLogoHeight) {
                            LogoUtils.setLogoViewLayoutParamsForDoodle(
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

    void setDefaultGoogleLogoDrawable(Drawable defaultGoogleLogoDrawable) {
        mDefaultGoogleLogoDrawable = defaultGoogleLogoDrawable;
    }

    /**
     * Shows the default search engine logo if available.
     *
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

    /**
     * Shows the default search engine logo if available.
     *
     * @return Whether the default search engine logo drawable is available.
     */
    private boolean maybeShowDefaultLogoDrawable() {
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

    /**
     * Sets the logo bounds to scale and translate the image so that it will be centered in the
     * LogoView and scaled to fit within the LogoView.
     *
     * @param preventUpscaling Whether the image should not be scaled up. If true, the image might
     *     not fill the entire view but will still be centered.
     */
    private void setLogoBounds(Drawable logo, boolean preventUpscaling) {
        if (logo == null) return;

        int imageWidth = logo.getIntrinsicWidth();
        int imageHeight = logo.getIntrinsicHeight();

        int width = getWidth();
        int height = getHeight();

        float scale = Math.min((float) width / imageWidth, (float) height / imageHeight);
        if (preventUpscaling) scale = Math.min(1.0f, scale);

        int scaledWidth = Math.round(imageWidth * scale);
        int scaledHeight = Math.round(imageHeight * scale);

        int imageOffsetX = Math.round((width - scaledWidth) * 0.5f);

        float whitespace = height - scaledHeight;
        int imageOffsetY = Math.round(whitespace * 0.5f);

        logo.setBounds(
                imageOffsetX,
                imageOffsetY,
                imageOffsetX + scaledWidth,
                imageOffsetY + scaledHeight);
    }

    @Override
    protected boolean verifyDrawable(Drawable who) {
        return who == mAnimatedLogoDrawable || super.verifyDrawable(who);
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
        if (mIsRefactorEnabled) {
            onDrawImplDrawable(canvas);
        } else {
            onDrawImpl(canvas);
        }
    }

    private void onDrawImpl(Canvas canvas) {
        if (isAnimatedLogoShowing()) {
            if (mFadeAnimation != null) mFadeAnimation.cancel();
            // Free the old bitmaps to allow them to be GC'd.
            mLogo = null;
            mNewLogo = null;

            canvas.save();
            canvas.concat(mAnimatedLogoMatrix);
            assumeNonNull(mAnimatedLogoDrawable).draw(canvas);
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
                mPaint.setAlpha((int) (255 * Math.pow(2 * (mTransitionAmount - 0.5f), 3)));
                canvas.save();
                canvas.concat(mNewLogoMatrix);
                canvas.drawBitmap(mNewLogo, 0, 0, mPaint);
                canvas.restore();
            }
        }
    }

    private void onDrawImplDrawable(Canvas canvas) {
        if (isAnimatedLogoShowing()) {
            if (mFadeAnimation != null) mFadeAnimation.cancel();
            // Free the old bitmaps to allow them to be GC'd.
            mLogoDrawable = null;
            mNewLogoDrawable = null;

            assumeNonNull(mAnimatedLogoDrawable).draw(canvas);
        } else {
            if (mLogoDrawable != null && mTransitionAmount < 0.5f) {
                mLogoDrawable.setAlpha((int) (255 * 2 * (0.5f - mTransitionAmount)));
                mLogoDrawable.draw(canvas);
            }
            if (mNewLogoDrawable != null && mTransitionAmount > 0.5f) {
                mNewLogoDrawable.setAlpha(
                        (int) (255 * Math.pow(2 * (mTransitionAmount - 0.5f), 3)));
                mNewLogoDrawable.draw(canvas);
            }
        }
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (mIsRefactorEnabled) {
            onSizeChangedImplDrawable(w, h, oldw, oldh);
        } else {
            onSizeChangedImpl(w, h, oldw, oldh);
        }
    }

    private void onSizeChangedImpl(int w, int h, int oldw, int oldh) {
        if (w != oldw || h != oldh) {
            if (mAnimatedLogoDrawable != null && mAnimatedLogoMatrix != null) {
                setMatrix(
                        mAnimatedLogoDrawable.getIntrinsicWidth(),
                        mAnimatedLogoDrawable.getIntrinsicHeight(),
                        mAnimatedLogoMatrix,
                        false);
            }
            if (mLogo != null && mLogoMatrix != null) {
                setMatrix(mLogo.getWidth(), mLogo.getHeight(), mLogoMatrix, mLogoIsDefault);
            }
            if (mNewLogo != null && mNewLogoMatrix != null) {
                setMatrix(
                        mNewLogo.getWidth(),
                        mNewLogo.getHeight(),
                        mNewLogoMatrix,
                        mNewLogoIsDefault);
            }
        }
    }

    private void onSizeChangedImplDrawable(int w, int h, int oldw, int oldh) {
        if (w != oldw || h != oldh) {
            if (mAnimatedLogoDrawable != null) {
                setLogoBounds(mAnimatedLogoDrawable, false);
            }
            if (mLogoDrawable != null) {
                setLogoBounds(mLogoDrawable, mLogoIsDefault);
            }
            if (mNewLogoDrawable != null) {
                setLogoBounds(mNewLogoDrawable, mNewLogoIsDefault);
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
        if (mFadeAnimation != null) mFadeAnimation.end();
    }

    @Nullable ObjectAnimator getFadeAnimationForTesting() {
        return mFadeAnimation;
    }

    @Nullable Bitmap getNewLogoForTesting() {
        return mNewLogo;
    }

    @Nullable Bitmap getNewLogoDrawableBitmapForTesting() {
        if (mNewLogoDrawable instanceof BitmapDrawable) {
            return ((BitmapDrawable) mNewLogoDrawable).getBitmap();
        }
        return null;
    }

    @Nullable Bitmap getLogoForTesting() {
        return mLogo;
    }

    @Nullable Drawable getLogoDrawableForTesting() {
        return mLogoDrawable;
    }

    boolean getAnimationEnabledForTesting() {
        return mAnimationEnabled;
    }

    @Nullable ClickHandler getClickHandlerForTesting() {
        return mClickHandler;
    }

    @Nullable Bitmap getDefaultGoogleLogoForTesting() {
        return mDefaultGoogleLogo;
    }

    @Nullable Drawable getDefaultGoogleLogoDrawableForTesting() {
        return mDefaultGoogleLogoDrawable;
    }

    int getLoadingViewVisibilityForTesting() {
        return mLoadingView.getVisibility();
    }

    void setLoadingViewVisibilityForTesting(int visibility) {
        mLoadingView.setVisibility(visibility);
    }

    int getDoodleSizeForTesting() {
        return mDoodleSize;
    }
}
