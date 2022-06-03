// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.ApiCompatibilityUtils;

/**
 * Material-styled horizontal progress bar
 *
 * This class is to be used in place of the support library's progress bar, which does not support
 * indeterminate progress bar styling on JB or KK.
 *
 * -------------------------------------------------------------------------------------------------
 * DESIGN SPEC
 *
 * https://material.io/guidelines/components/progress-activity.html
 *
 * Secondary progress is represented by a second bar that is drawn on top of the primary progress,
 * and is completely optional.
 *
 * -------------------------------------------------------------------------------------------------
 * DEFINING THE CONTROL STYLING IN AN XML LAYOUT
 *
 * Add the "app" namespace as an attribute to the main tag of the layout file:
 * xmlns:app="http://schemas.android.com/apk/res-auto
 *
 * These attributes control styling of the bar:
 * app:colorBackground         Background color of the progress bar.
 * app:colorProgress           Represents progress along the determinate progress bar.
 *                             Also used as the pulsing color.
 * app:colorSecondaryProgress  Represents secondary progress on top of the regular progress.
 */
public class MaterialProgressBar extends View implements AnimatorUpdateListener {
    private static final long INDETERMINATE_ANIMATION_DURATION_MS = 3000;

    private final ValueAnimator mIndeterminateAnimator = ValueAnimator.ofFloat(0.0f, 3.0f);
    private final Paint mBackgroundPaint = new Paint();
    private final Paint mProgressPaint = new Paint();
    private final Paint mSecondaryProgressPaint = new Paint();

    private boolean mIsIndeterminate;
    private int mProgress;
    private int mSecondaryProgress;

    public MaterialProgressBar(Context context) {
        super(context);
        initialize(context, null, 0);
    }

    public MaterialProgressBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        initialize(context, attrs, 0);
    }

    public MaterialProgressBar(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        initialize(context, attrs, defStyle);
    }

    /** Sets the background color, corresponding to "chrome:colorBackground". */
    @Override
    public void setBackgroundColor(int color) {
        mBackgroundPaint.setColor(color);
        postInvalidateOnAnimation();
    }

    /** Sets the progress color, corresponding to "chrome:colorProgress". */
    public void setProgressColor(int color) {
        mProgressPaint.setColor(color);
        postInvalidateOnAnimation();
    }

    /** Sets the secondary color, corresponding to "chrome:colorSecondaryProgress". */
    public void setSecondaryProgressColor(int color) {
        mSecondaryProgressPaint.setColor(color);
        postInvalidateOnAnimation();
    }

    /**
     * Sets the progress value being displayed.
     * @param progress Progress value, ranging from 0 to 100.  Will be clamped into the range.
     */
    public void setProgress(int progress) {
        mProgress = Math.max(0, Math.min(100, progress));
        postInvalidateOnAnimation();
    }

    /**
     * Sets the secondary progress value being displayed.
     * @param progress Progress value, ranging from 0 to 100.  Will be clamped into the range.
     */
    public void setSecondaryProgress(int progress) {
        mSecondaryProgress = Math.max(0, Math.min(100, progress));
        postInvalidateOnAnimation();
    }

    /** Sets whether the progress bar is indeterminate or not. */
    public void setIndeterminate(boolean indeterminate) {
        if (mIsIndeterminate == indeterminate) return;
        mIsIndeterminate = indeterminate;

        if (mIsIndeterminate) startIndeterminateAnimation();
        postInvalidateOnAnimation();
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);
        if (visibility == View.VISIBLE) {
            startIndeterminateAnimation();
        } else {
            stopIndeterminateAnimation();
        }
    }

    @Override
    public void onAnimationUpdate(ValueAnimator animation) {
        postInvalidateOnAnimation();
    }

    @Override
    public void onDraw(Canvas canvas) {
        if (mIsIndeterminate) {
            drawIndeterminateBar(canvas);
        } else {
            drawDeterminateBar(canvas);
        }
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        startIndeterminateAnimation();
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        stopIndeterminateAnimation();
    }

    private void initialize(Context context, AttributeSet attrs, int defStyle) {
        Resources resources = context.getResources();
        int backgroundColor =
                ApiCompatibilityUtils.getColor(resources, R.color.progress_bar_background);
        int progressColor =
                ApiCompatibilityUtils.getColor(resources, R.color.progress_bar_foreground);
        int secondaryProgressColor =
                ApiCompatibilityUtils.getColor(resources, R.color.progress_bar_secondary);

        if (attrs != null) {
            TypedArray a = context.obtainStyledAttributes(
                    attrs, R.styleable.MaterialProgressBar, defStyle, 0);
            backgroundColor =
                    a.getColor(R.styleable.MaterialProgressBar_colorBackground, backgroundColor);
            progressColor =
                    a.getColor(R.styleable.MaterialProgressBar_colorProgress, progressColor);
            secondaryProgressColor = a.getColor(
                    R.styleable.MaterialProgressBar_colorSecondaryProgress, secondaryProgressColor);
            a.recycle();
        }

        setBackgroundColor(backgroundColor);
        setProgressColor(progressColor);
        setSecondaryProgressColor(secondaryProgressColor);

        mIndeterminateAnimator.setRepeatCount(ValueAnimator.INFINITE);
        mIndeterminateAnimator.setDuration(INDETERMINATE_ANIMATION_DURATION_MS);
        mIndeterminateAnimator.addUpdateListener(this);
    }

    private void startIndeterminateAnimation() {
        if (!mIsIndeterminate || mIndeterminateAnimator.isRunning()) return;
        if (!ViewCompat.isAttachedToWindow(this) || getVisibility() != View.VISIBLE) return;
        mIndeterminateAnimator.start();
    }

    private void stopIndeterminateAnimation() {
        if (!mIndeterminateAnimator.isRunning()) return;
        mIndeterminateAnimator.cancel();
    }

    private void drawIndeterminateBar(Canvas canvas) {
        int width = canvas.getWidth();
        drawRect(canvas, mBackgroundPaint, 0, width);

        // The first pulse fires off at the beginning of the animation.
        float value = (Float) mIndeterminateAnimator.getAnimatedValue();
        float left = width * (float) (Math.pow(value, 1.5f) - 0.5f);
        float right = width * value;
        drawRect(canvas, mProgressPaint, left, right);

        // The second pulse fires off at some point after the first pulse has been fired.
        final float secondPulseStart = 1.1f;
        final float secondPulseLength = 1.0f;
        if (value >= secondPulseStart) {
            float percentage = (value - secondPulseStart) / secondPulseLength;
            left = width * (float) (Math.pow(percentage, 2.5f) - 0.1f);
            right = width * percentage;
            drawRect(canvas, mProgressPaint, left, right);
        }
    }

    private void drawDeterminateBar(Canvas canvas) {
        int width = canvas.getWidth();
        drawRect(canvas, mBackgroundPaint, 0, width);

        if (mProgress > 0) {
            float percentage = mProgress / 100.0f;
            drawRect(canvas, mProgressPaint, 0, width * percentage);
        }

        if (mSecondaryProgress > 0) {
            float percentage = mSecondaryProgress / 100.0f;
            drawRect(canvas, mSecondaryProgressPaint, 0, width * percentage);
        }
    }

    private void drawRect(Canvas canvas, Paint paint, float start, float end) {
        if (ViewCompat.getLayoutDirection(this) == View.LAYOUT_DIRECTION_RTL) {
            int width = canvas.getWidth();
            float rtlStart = width - end;
            float rtlEnd = width - start;
            canvas.drawRect(rtlStart, 0, rtlEnd, canvas.getHeight(), paint);
        } else {
            canvas.drawRect(start, 0, end, canvas.getHeight(), paint);
        }
    }

    /** @return The current progress value. */
    @VisibleForTesting
    public int getProgressForTesting() {
        return mProgress;
    }
}
