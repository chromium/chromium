// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.TimeAnimator;
import android.animation.TimeAnimator.TimeListener;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.animation.Interpolator;
import android.widget.ProgressBar;

import androidx.core.view.ViewCompat;
import androidx.core.view.animation.PathInterpolatorCompat;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;

/**
 * Progress bar for use in the Toolbar view. If no progress updates are received for 5 seconds, an
 * indeterminate animation will begin playing and the animation will move across the screen smoothly
 * instead of jumping.
 */
@NullMarked
public class ToolbarProgressBar extends ClipDrawableProgressBar
        implements BrowserControlsStateProvider.Observer {
    /** Interface for progress bar animation interpolation logics. */
    interface AnimationLogic {
        /**
         * Resets internal data. It must be called on every loading start.
         *
         * @param startProgress The progress for the animation to start at. This is used when the
         *     animation logic switches.
         */
        void reset(float startProgress);

        /**
         * Returns interpolated progress for animation.
         *
         * @param targetProgress Actual page loading progress.
         * @param frameTimeSec   Duration since the last call.
         * @param resolution     Resolution of the displayed progress bar. Mainly for rounding.
         */
        float updateProgress(float targetProgress, float frameTimeSec, int resolution);
    }

    /**
     * The amount of time in ms that the progress bar has to be stopped before the indeterminate
     * animation starts.
     */
    private static final long ANIMATION_START_THRESHOLD = 5000;

    private static final long HIDE_DELAY_MS = 100;

    // Android progress bar animation constants
    private static final float THEMED_BACKGROUND_WHITE_FRACTION = 0.2f;
    private static final float ANIMATION_WHITE_FRACTION = 0.4f;

    private static final long PROGRESS_FRAME_TIME_CAP_MS = 50;
    private static final long ALPHA_ANIMATION_DURATION_MS = 140;

    // Composited progress bar animation constants
    private static final long LOADING_ANIMATION_DURATION_MS = 3000;
    private static final long FINISH_ANIMATION_DURATION_MS = 1000;

    @Nullable private Integer mCachedFpsCap;

    /** Whether or not the progress bar has started processing updates. */
    private boolean mIsStarted;

    /** The target progress the smooth animation should move to (when animating smoothly). */
    private float mTargetProgress;

    private long mTimeSinceLastFrameMs;
    private long mLastUpdateTimeMs;

    /** The current progress displayed by the animation. */
    private float mAnimatedProgress;

    /** The logic used to animate the progress bar during smooth animation. */
    private final AnimationLogic mAnimationLogic;

    /** The number of times the progress bar has started (used for testing). */
    private int mProgressStartCount;

    /** The theme color currently being used. */
    private int mThemeColor;

    /** The indeterminate animating view for the progress bar. */
    private @Nullable ToolbarProgressBarAnimatingView mAnimatingView;

    /** The current running animator that controls the fade in/out of the progress bar. */
    private @Nullable Animator mFadeAnimator;

    private final Runnable mStartSmoothIndeterminate =
            new Runnable() {
                @Override
                public void run() {
                    if (!mIsStarted
                            || ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.ANDROID_PB_DISABLE_PULSE_ANIMATION)) {
                        return;
                    }
                    mAnimationLogic.reset(getProgress());

                    if (!ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.ANDROID_PB_DISABLE_SMOOTH_ANIMATION)
                            && !shouldAnimateCompositedLayer()) {
                        mSmoothProgressAnimator.start();
                    }

                    if (mAnimatingView != null) {
                        int width =
                                Math.abs(
                                        getDrawable().getBounds().right
                                                - getDrawable().getBounds().left);
                        mAnimatingView.update(getProgress() * width);

                        if (shouldAnimateCompositedLayer()
                                && getDesiredAndroidVisibility() == VISIBLE) {
                            mAnimatingView.setVisibility(VISIBLE);
                        }
                        mAnimatingView.startAnimation();
                    }
                }
            };

    private final TimeAnimator mSmoothProgressAnimator = new TimeAnimator();
    TimeListener mSmoothProgressAnimatorListener =
            new TimeListener() {
                @Override
                public void onTimeUpdate(
                        TimeAnimator animation, long totalTimeMs, long deltaTimeMs) {
                    // If we are at the target progress already, do nothing.
                    if (MathUtils.areFloatsEqual(getProgress(), mTargetProgress)) return;

                    // Cap progress bar animation frame time so that it doesn't jump too much
                    // even when the animation is janky.
                    float progress =
                            mAnimationLogic.updateProgress(
                                    mTargetProgress,
                                    Math.min(deltaTimeMs, PROGRESS_FRAME_TIME_CAP_MS) * 0.001f,
                                    getWidth());
                    progress = Math.max(progress, 0);

                    // TODO(mdjones): Find a sane way to have this call setProgressInternal so
                    // the finish logic can be recycled. Consider stopping the progress
                    // throttle if the smooth animation is running.
                    ToolbarProgressBar.super.setProgress(progress);

                    if (mAnimatingView != null) {
                        int width =
                                Math.abs(
                                        getDrawable().getBounds().right
                                                - getDrawable().getBounds().left);
                        mAnimatingView.update(progress * width);
                    }

                    // If progress is at 100%, start hiding the progress bar.
                    if (MathUtils.areFloatsEqual(getProgress(), 1.f)) finish(true);
                }
            };

    private final TimeAnimator mCompositedProgressBarAnimation = new TimeAnimator();
    TimeListener mCompositedProgressBarAnimationListener =
            new TimeListener() {
                @Override
                public void onTimeUpdate(
                        TimeAnimator animation, long totalTimeMs, long deltaTimeMs) {
                    if (MathUtils.areFloatsEqual(mAnimatedProgress, mTargetProgress)
                            || mAnimatedProgress > mTargetProgress) {
                        mCompositedProgressBarAnimation.cancel();
                    }

                    // deltaTimeMs is always 0 on the first frame
                    if (deltaTimeMs != 0) {
                        mTimeSinceLastFrameMs += deltaTimeMs;
                        long fps = (long) (1000 * 1.0f / mTimeSinceLastFrameMs);
                        long fpsCap = getCompositedAnimationFpsCap();
                        if (fpsCap > 0 && fps > fpsCap) {
                            return;
                        }
                        mTimeSinceLastFrameMs = 0;
                    }

                    // TODO(peilinwang): Maybe introduce a max increment to reduce jank?
                    mAnimatedProgress += (deltaTimeMs / ((float) LOADING_ANIMATION_DURATION_MS));
                    mAnimatedProgress = Math.min(mAnimatedProgress, mTargetProgress);
                    ToolbarProgressBar.super.setProgress(mAnimatedProgress);
                    if (MathUtils.areFloatsEqual(getProgress(), 1.f)) finish(true);
                }
            };

    private final ValueAnimator mProgressBarAnimationBc25 = ValueAnimator.ofFloat(0, 1);
    AnimatorUpdateListener mProgressBarAnimationBc25Listener =
            new AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    float progress = (float) animation.getAnimatedValue();
                    mAnimatedProgress = progress;
                    if (MathUtils.areFloatsEqual(mAnimatedProgress, 1.f)) {
                        ToolbarProgressBar.super.setProgress(mAnimatedProgress);
                        finish(true);
                    } else {
                        long currentTime = animation.getCurrentPlayTime();
                        // currentTime is always 0 on the first frame
                        if (currentTime != 0) {
                            mTimeSinceLastFrameMs = currentTime - mLastUpdateTimeMs;
                            long fps = (long) (1000 * 1.0f / mTimeSinceLastFrameMs);
                            long fpsCap = getCompositedAnimationFpsCap();
                            if (fpsCap > 0 && fps > fpsCap) {
                                return;
                            }
                            mTimeSinceLastFrameMs = 0;
                        }
                        ToolbarProgressBar.super.setProgress(mAnimatedProgress);
                        mLastUpdateTimeMs = currentTime;
                    }
                }
            };

    {
        // manually selected to look like bc25 mocks
        mProgressBarAnimationBc25.setInterpolator(
                PathInterpolatorCompat.create(0.57f, 0f, 0.12f, 1.0f));
        mProgressBarAnimationBc25.setDuration(FINISH_ANIMATION_DURATION_MS);
    }

    /**
     * Creates a toolbar progress bar.
     *
     * @param context The application environment.
     */
    public ToolbarProgressBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setAlpha(0.0f);
        mAnimationLogic = new ProgressAnimationSmooth();

        if (!(ChromeFeatureList.sAndroidAnimatedProgressBarInViz.isEnabled()
                || ChromeFeatureList.sAndroidAnimatedProgressBarInBrowser.isEnabled())) {
            setVisibility(View.VISIBLE);
        }

        // This tells accessibility services that progress bar changes are important enough to
        // announce to the user even when not focused.
        ViewCompat.setAccessibilityLiveRegion(this, ViewCompat.ACCESSIBILITY_LIVE_REGION_POLITE);
        setProgressBarColors();
    }

    @Override
    protected boolean useGradientDrawable() {
        return ChromeFeatureList.sAndroidProgressBarVisualUpdate.isEnabled();
    }

    public void setAnimatingView(ToolbarProgressBarAnimatingView animatingView) {
        mAnimatingView = animatingView;

        // TODO(peilinwang): after AndroidAnimatedCompositedProgressBar launches, make the xml
        // property for this view default invisible and remove this.
        if (shouldAnimateCompositedLayer()) {
            mAnimatingView.setVisibility(INVISIBLE);
        }

        if (useGradientDrawable()) {
            mAnimatingView.setCornerRadius((float) mProgressBarHeight / 2);
        }
        setProgressBarColors();
    }

    /**
     * Returns the height the progress bar would be when it is displayed. This is different from
     * getHeight() which returns the progress bar height only if it's currently in the layout.
     */
    public int getDefaultHeight() {
        return mProgressBarHeight;
    }

    @Override
    public void onAndroidControlsVisibilityChanged(int visibility) {
        setVisibility(visibility);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        mSmoothProgressAnimator.setTimeListener(mSmoothProgressAnimatorListener);

        if (shouldAnimateCompositedLayer()) {
            mCompositedProgressBarAnimation.setTimeListener(
                    mCompositedProgressBarAnimationListener);
            mProgressBarAnimationBc25.addUpdateListener(mProgressBarAnimationBc25Listener);
        }
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        mSmoothProgressAnimator.setTimeListener(null);
        mSmoothProgressAnimator.cancel();

        if (shouldAnimateCompositedLayer()) {
            mCompositedProgressBarAnimation.setTimeListener(null);
            mCompositedProgressBarAnimation.cancel();
            mProgressBarAnimationBc25.removeAllUpdateListeners();
            mProgressBarAnimationBc25.cancel();
        }
    }

    @Override
    public void setAlpha(float alpha) {
        super.setAlpha(alpha);
        if (mAnimatingView != null) mAnimatingView.setAlpha(alpha);
    }

    @Override
    public void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        // If the size changed, the animation width needs to be manually updated.
        if (mAnimatingView != null) mAnimatingView.update(width * getProgress());
    }

    /** Start showing progress bar animation. */
    public void start() {
        ThreadUtils.assertOnUiThread();

        mIsStarted = true;
        mProgressStartCount++;

        removeCallbacks(mStartSmoothIndeterminate);
        postDelayed(mStartSmoothIndeterminate, ANIMATION_START_THRESHOLD);

        if (shouldAnimateCompositedLayer()) {
            mProgressBarAnimationBc25.cancel();
            mCompositedProgressBarAnimation.cancel();
        }

        super.setProgress(0.0f);
        mAnimatedProgress = 0.0f;
        mAnimationLogic.reset(0.0f);
        animateAlphaTo(1.0f);
    }

    /**
     * @return True if the progress bar is showing and started.
     */
    public boolean isStarted() {
        return mIsStarted;
    }

    /**
     * Start hiding progress bar animation. Progress does not necessarily need to be at 100% to
     * finish. If 'fadeOut' is set to true, progress will forced to 100% (if not already) and then
     * fade out. If false, the progress will hide regardless of where it currently is.
     * @param fadeOut Whether the progress bar should fade out. If false, the progress bar will
     *                disappear immediately, regardless of animation.
     *                TODO(mdjones): This param should be "force" but involves inverting all calls
     *                to this method.
     */
    public void finish(boolean fadeOut) {
        ThreadUtils.assertOnUiThread();

        if (!MathUtils.areFloatsEqual(getProgress(), 1.0f)) {
            // If any of the animators are running while this method is called, set the internal
            // progress and wait for the animation to end.
            setProgress(1.0f);
            if (fadeOut
                    && shouldAnimateCompositedLayer()
                    && !mProgressBarAnimationBc25.isRunning()) {
                mCompositedProgressBarAnimation.cancel();
                mLastUpdateTimeMs = 0;
                mProgressBarAnimationBc25.setFloatValues(mAnimatedProgress, 1.0f);
                mProgressBarAnimationBc25.start();
            }
            if (areProgressAnimatorsRunning() && fadeOut) return;
        }

        if (shouldAnimateCompositedLayer()) {
            mCompositedProgressBarAnimation.cancel();
            mProgressBarAnimationBc25.cancel();
        }

        mIsStarted = false;
        mTargetProgress = 0;

        removeCallbacks(mStartSmoothIndeterminate);
        if (mAnimatingView != null) {
            mAnimatingView.cancelAnimation();
        }
        mSmoothProgressAnimator.cancel();

        if (fadeOut) {
            postDelayed(() -> hideProgressBar(true), HIDE_DELAY_MS);
        } else {
            hideProgressBar(false);
        }
    }

    /**
     * Hide the progress bar.
     * @param animate Whether to animate the opacity.
     */
    private void hideProgressBar(boolean animate) {
        ThreadUtils.assertOnUiThread();

        if (mIsStarted) return;
        if (!animate) animate().cancel();
        if (shouldAnimateCompositedLayer() && mAnimatingView != null) {
            mAnimatingView.setVisibility(INVISIBLE);
        }

        // Make invisible.
        if (animate) {
            animateAlphaTo(0.0f);
        } else {
            setAlpha(0.0f);
        }
    }

    /**
     * @return Whether any animator that delays the showing of progress is running.
     */
    private boolean areProgressAnimatorsRunning() {
        boolean areCompositedAnimationsRunning =
                shouldAnimateCompositedLayer()
                        ? mCompositedProgressBarAnimation.isRunning()
                                || mProgressBarAnimationBc25.isRunning()
                        : false;
        return mSmoothProgressAnimator.isRunning() || areCompositedAnimationsRunning;
    }

    /**
     * Animate the alpha of all of the parts of the progress bar.
     * @param targetAlpha The alpha in range [0, 1] to animate to.
     */
    private void animateAlphaTo(float targetAlpha) {
        float alphaDiff = targetAlpha - getAlpha();
        if (alphaDiff == 0.0f) return;

        long duration = (long) Math.abs(alphaDiff * ALPHA_ANIMATION_DURATION_MS);

        Interpolator interpolator = Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR;
        if (alphaDiff < 0) interpolator = Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR;

        if (mFadeAnimator != null) mFadeAnimator.cancel();

        ObjectAnimator alphaFade = ObjectAnimator.ofFloat(this, ALPHA, getAlpha(), targetAlpha);
        alphaFade.setDuration(duration);
        alphaFade.setInterpolator(interpolator);
        mFadeAnimator = alphaFade;

        if (mAnimatingView != null) {
            alphaFade =
                    ObjectAnimator.ofFloat(
                            mAnimatingView, ALPHA, mAnimatingView.getAlpha(), targetAlpha);
            alphaFade.setDuration(duration);
            alphaFade.setInterpolator(interpolator);

            AnimatorSet fadeSet = new AnimatorSet();
            fadeSet.playTogether(mFadeAnimator, alphaFade);
            mFadeAnimator = fadeSet;
        }

        mFadeAnimator.start();
    }

    // ClipDrawableProgressBar implementation.

    @Override
    public void setProgress(float progress) {
        ThreadUtils.assertOnUiThread();
        setProgressInternal(progress);
    }

    /**
     * Set the progress bar state based on the external updates coming in.
     * @param progress The current progress.
     */
    private void setProgressInternal(float progress) {
        if (!mIsStarted || MathUtils.areFloatsEqual(mTargetProgress, progress)) return;
        mTargetProgress = progress;

        // If the progress bar was updated, reset the callback that triggers the
        // smooth-indeterminate animation.
        removeCallbacks(mStartSmoothIndeterminate);

        if (shouldAnimateCompositedLayer()) {
            if (mAnimatingView != null && !mAnimatingView.isRunning()) {
                postDelayed(mStartSmoothIndeterminate, ANIMATION_START_THRESHOLD);
            }
        } else {
            if (!mSmoothProgressAnimator.isRunning()) {
                postDelayed(mStartSmoothIndeterminate, ANIMATION_START_THRESHOLD);
                super.setProgress(mTargetProgress);
            }
        }

        if (shouldAnimateCompositedLayer() && !mCompositedProgressBarAnimation.isRunning()) {
            mCompositedProgressBarAnimation.start();
        }

        sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SELECTED);

        if (MathUtils.areFloatsEqual(progress, 1.0f) || progress > 1.0f) finish(true);
    }

    private void setProgressBarColors() {
        if (mThemeColor != 0) {
            setThemeColor(mThemeColor, false);
        } else {
            setThemeColor(ChromeColors.getDefaultThemeColor(getContext(), false), false);
        }
    }

    private int getCompositedAnimationFpsCap() {
        if (mCachedFpsCap == null) {
            mCachedFpsCap = ChromeFeatureList.sAndroidAnimatedProgressBarFpsCap.getValue();
        }
        return mCachedFpsCap;
    }

    @Override
    public void setVisibility(int visibility) {
        // Hide the progress bar if it is being forced externally.
        super.setVisibility(visibility);
        if (mAnimatingView != null) {
            boolean shouldUpdateAnimatingView = true;
            if (shouldAnimateCompositedLayer()
                    && visibility == VISIBLE
                    && !mAnimatingView.isRunning()) {
                shouldUpdateAnimatingView = false;
            }

            if (shouldUpdateAnimatingView) {
                mAnimatingView.setVisibility(visibility);
            }
        }
    }

    /**
     * Color the progress bar based on the toolbar theme color.
     * @param color The Android color the toolbar is using.
     */
    public void setThemeColor(int color, boolean isIncognito) {
        mThemeColor = color;
        boolean isDefaultTheme =
                ThemeUtils.isUsingDefaultToolbarColor(getContext(), isIncognito, mThemeColor);

        // The default toolbar has specific colors to use.
        if ((isDefaultTheme || ColorUtils.isThemeColorTooBright(color)) && !isIncognito) {
            setForegroundColor(SemanticColorUtils.getProgressBarForeground(getContext()));
            if (ChromeFeatureList.sAndroidProgressBarVisualUpdate.isEnabled()) {
                setBackgroundColor(SemanticColorUtils.getProgressBarTrackColor(getContext()));
                setProgressGapBackgroundColor(color);
            } else {
                setBackgroundColor(getContext().getColor(R.color.progress_bar_bg_color_list));
            }
            return;
        }

        setForegroundColor(ColorUtils.getThemedAssetColor(color, isIncognito));

        if (mAnimatingView != null
                && (ColorUtils.shouldUseLightForegroundOnBackground(color) || isIncognito)) {
            mAnimatingView.setColor(
                    ColorUtils.getColorWithOverlay(color, Color.WHITE, ANIMATION_WHITE_FRACTION));
        }

        setBackgroundColor(
                ColorUtils.getColorWithOverlay(
                        color, Color.WHITE, THEMED_BACKGROUND_WHITE_FRACTION));
        if (ChromeFeatureList.sAndroidProgressBarVisualUpdate.isEnabled()) {
            setProgressGapBackgroundColor(color);
        }
    }

    @Override
    public void setForegroundColor(int color) {
        super.setForegroundColor(color);
        if (mAnimatingView != null) {
            mAnimatingView.setColor(
                    ColorUtils.getColorWithOverlay(color, Color.WHITE, ANIMATION_WHITE_FRACTION));
        }
    }

    @Override
    public CharSequence getAccessibilityClassName() {
        return ProgressBar.class.getName();
    }

    @Override
    public void onInitializeAccessibilityEvent(AccessibilityEvent event) {
        super.onInitializeAccessibilityEvent(event);
        event.setCurrentItemIndex((int) (mTargetProgress * 100));
        event.setItemCount(100);
    }

    /**
     * @return The number of times the progress bar has been triggered.
     */
    public int getStartCountForTesting() {
        return mProgressStartCount;
    }

    /** Reset the number of times the progress bar has been triggered. */
    public void resetStartCountForTesting() {
        mProgressStartCount = 0;
    }

    /** Start the indeterminate progress bar animation. */
    public void startIndeterminateAnimationForTesting() {
        mStartSmoothIndeterminate.run();
    }

    /**
     * @return The indeterminate animator.
     */
    public Animator getIndeterminateAnimatorForTesting() {
        return mSmoothProgressAnimator;
    }
}
