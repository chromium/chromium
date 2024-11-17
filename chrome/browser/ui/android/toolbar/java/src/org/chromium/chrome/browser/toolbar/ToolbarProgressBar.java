// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.TimeAnimator;
import android.animation.TimeAnimator.TimeListener;
import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.animation.Interpolator;
import android.widget.ProgressBar;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;

/**
 * Progress bar for use in the Toolbar view. If no progress updates are received for 5 seconds, an
 * indeterminate animation will begin playing and the animation will move across the screen smoothly
 * instead of jumping.
 */
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

    private static final float THEMED_BACKGROUND_WHITE_FRACTION = 0.2f;
    private static final float ANIMATION_WHITE_FRACTION = 0.4f;

    private static final long PROGRESS_FRAME_TIME_CAP_MS = 50;
    private static final long ALPHA_ANIMATION_DURATION_MS = 140;

    /** Whether or not the progress bar has started processing updates. */
    private boolean mIsStarted;

    /** The target progress the smooth animation should move to (when animating smoothly). */
    private float mTargetProgress;

    /** The logic used to animate the progress bar during smooth animation. */
    private final AnimationLogic mAnimationLogic;

    /** The number of times the progress bar has started (used for testing). */
    private int mProgressStartCount;

    /** The theme color currently being used. */
    private int mThemeColor;

    /** The indeterminate animating view for the progress bar. */
    private ToolbarProgressBarAnimatingView mAnimatingView;

    /** The progress bar's height. */
    private final int mProgressBarHeight;

    /** The current running animator that controls the fade in/out of the progress bar. */
    @Nullable private Animator mFadeAnimator;

    private final Runnable mStartSmoothIndeterminate =
            new Runnable() {
                @Override
                public void run() {
                    if (!mIsStarted) return;
                    mAnimationLogic.reset(getProgress());
                    mSmoothProgressAnimator.start();

                    if (mAnimatingView != null) {
                        int width =
                                Math.abs(
                                        getDrawable().getBounds().right
                                                - getDrawable().getBounds().left);
                        mAnimatingView.update(getProgress() * width);
                        mAnimatingView.startAnimation();
                    }
                }
            };

    private final TimeAnimator mSmoothProgressAnimator = new TimeAnimator();

    {
        mSmoothProgressAnimator.setTimeListener(
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
                });
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
        mProgressBarHeight =
                getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.toolbar.R.dimen
                                        .toolbar_progress_bar_height);

        setVisibility(View.VISIBLE);

        // This tells accessibility services that progress bar changes are important enough to
        // announce to the user even when not focused.
        ViewCompat.setAccessibilityLiveRegion(this, ViewCompat.ACCESSIBILITY_LIVE_REGION_POLITE);
        setForegroundOrThemeColor();
    }

    public void setAnimatingView(ToolbarProgressBarAnimatingView animatingView) {
        mAnimatingView = animatingView;
        setForegroundOrThemeColor();
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
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        mSmoothProgressAnimator.setTimeListener(null);
        mSmoothProgressAnimator.cancel();
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

        super.setProgress(0.0f);
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
            if (areProgressAnimatorsRunning() && fadeOut) return;
        }

        mIsStarted = false;
        mTargetProgress = 0;

        removeCallbacks(mStartSmoothIndeterminate);
        if (mAnimatingView != null) mAnimatingView.cancelAnimation();
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
        return mSmoothProgressAnimator.isRunning();
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

        if (!mSmoothProgressAnimator.isRunning()) {
            postDelayed(mStartSmoothIndeterminate, ANIMATION_START_THRESHOLD);
            super.setProgress(mTargetProgress);
        }

        sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SELECTED);

        if (MathUtils.areFloatsEqual(progress, 1.0f) || progress > 1.0f) finish(true);
    }

    private void setForegroundOrThemeColor() {
        if (mThemeColor != 0) {
            setThemeColor(mThemeColor, false);
        } else {
            setForegroundColor(getForegroundColor());
        }
    }

    @Override
    public void setVisibility(int visibility) {
        // Hide the progress bar if it is being forced externally.
        super.setVisibility(visibility);
        if (mAnimatingView != null) mAnimatingView.setVisibility(visibility);
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
            setBackgroundColor(getContext().getColor(R.color.progress_bar_bg_color_list));
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
