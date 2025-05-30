// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.widget.ImageView;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** An alternative progress bar implemented using ClipDrawable for simplicity and performance. */
@NullMarked
public class ClipDrawableProgressBar extends ImageView {
    /** Structure that has complete {@link ClipDrawableProgressBar} drawing information. */
    public static class DrawingInfo {
        public final Rect progressBarRect = new Rect();
        public final Rect progressBarBackgroundRect = new Rect();
        public final Rect progressBarEndIndicator = new Rect();

        public int progressBarColor;
        public int progressBarBackgroundColor;
        public float cornerRadius;
    }

    /** An observer for visible progress updates. */
    @VisibleForTesting
    public interface ProgressBarObserver {
        /**
         * A notification that the visible progress has been updated. This may not coincide with
         * updates from the web page due to animations for the progress bar running.
         */
        void onVisibleProgressUpdated();

        /** A notification that the visibility of the progress bar has changed. */
        void onVisibilityChanged();
    }

    // ClipDrawable's max is a fixed constant 10000.
    // http://developer.android.com/reference/android/graphics/drawable/ClipDrawable.html
    private static final int CLIP_DRAWABLE_MAX = 10000;

    @Nullable private ColorDrawable mForegroundColorDrawable;
    @Nullable private GradientDrawable mForegroundGradientDrawable;
    @Nullable private GradientDrawable mBackgroundGradientDrawable;
    @Nullable private GradientDrawable mEndCapCircleDrawable;
    private int mForegroundColor;
    private int mBackgroundColor;
    protected final int mProgressBarHeight;
    private float mProgress;
    private int mDesiredVisibility;

    /** An observer of updates to the progress bar. */
    private @Nullable ProgressBarObserver mProgressBarObserver;

    /**
     * Create the progress bar with a custom height.
     *
     * @param context An Android context.
     */
    public ClipDrawableProgressBar(Context context, AttributeSet attrs) {
        super(context, attrs);

        setScaleType(ScaleType.FIT_XY); // Ensure the drawable fills the ImageView
        mDesiredVisibility = getVisibility();

        mForegroundColor = SemanticColorUtils.getProgressBarForeground(getContext());
        mBackgroundColor = getContext().getColor(R.color.progress_bar_bg_color_list);
        if (useGradientDrawable()) {
            mProgressBarHeight = getResources().getDimensionPixelSize(
                    R.dimen.toolbar_progress_bar_increased_height);
        } else {
            mProgressBarHeight = getResources().getDimensionPixelSize(
                    R.dimen.toolbar_progress_bar_height);
        }
        initializeDrawables();
        setBackgroundColor(mBackgroundColor);
    }

    /**
     * Initializes the underlying drawables for the progress bar.
     * If {@link #useGradientDrawable()} is true, this sets up a {@link LayerDrawable}
     * with a moving foreground and background components, otherwise it sets up just a moving
     * foreground component with a plain background.
     */
    private void initializeDrawables() {
        if (useGradientDrawable()) {
            mForegroundGradientDrawable = createGradientDrawable(mForegroundColor,
                    GradientDrawable.RECTANGLE);
            mForegroundGradientDrawable.setCornerRadius((float) mProgressBarHeight / 2);
            ClipDrawable foregroundClipDrawable = new ClipDrawable(mForegroundGradientDrawable,
                    Gravity.START, ClipDrawable.HORIZONTAL);

            mBackgroundGradientDrawable = createGradientDrawable(mBackgroundColor,
                    GradientDrawable.RECTANGLE);
            mBackgroundGradientDrawable.setCornerRadius((float) mProgressBarHeight / 2);
            ClipDrawable backgroundClipDrawable = new ClipDrawable(
                    mBackgroundGradientDrawable, Gravity.END, ClipDrawable.HORIZONTAL);
            // Background will be fully visible.
            backgroundClipDrawable.setLevel(CLIP_DRAWABLE_MAX);

            // Create the end circular stop indicator
            mEndCapCircleDrawable = createGradientDrawable(mForegroundColor, GradientDrawable.OVAL);
            mEndCapCircleDrawable.setSize(mProgressBarHeight, mProgressBarHeight);

            // A layerDrawable with the 2 moving components, foreground and background, and the
            // end stop indicator. Layers are drawn in the order they are added to the array,
            // with the last one appearing on top.
            Drawable[] layers =
                    {foregroundClipDrawable, backgroundClipDrawable, mEndCapCircleDrawable};
            LayerDrawable layerDrawable = new LayerDrawable(layers);

            // The circle (layer 2) will be drawn at the right end of the progress bar.
            layerDrawable.setLayerGravity(2, Gravity.END | Gravity.CENTER_VERTICAL);

            setImageDrawable(layerDrawable);
        } else {
            mForegroundColorDrawable = new ColorDrawable(mForegroundColor);
            setImageDrawable(new ClipDrawable(mForegroundColorDrawable, Gravity.START,
                    ClipDrawable.HORIZONTAL));
        }
    }

    /**
     * Creates a new {@link GradientDrawable} with a rectangular shape and the specified color.
     *
     * @param color The color to set for the drawable.
     * @return A new {@link GradientDrawable} instance.
     */
    private static GradientDrawable createGradientDrawable(int color, int shape) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setShape(shape);
        drawable.setColor(color);
        return drawable;
    }

    /**
     * Determines whether to use a {@link GradientDrawable} for the progress bar,
     * which allows for a moving foreground and background. If false, only a moving foreground is
     * used.
     *
     * @return True if {@link GradientDrawable} should be used, false otherwise.
     * Currently, this always returns false.
     */
    protected boolean useGradientDrawable() {
        return false;
    }

    /** @param observer An update observer for the progress bar. */
    @VisibleForTesting
    public void setProgressBarObserver(ProgressBarObserver observer) {
        assert mProgressBarObserver == null;
        mProgressBarObserver = observer;
    }

    /**
     * Get the progress bar's current level of progress.
     *
     * @return The current progress, between 0.0 and 1.0.
     */
    public float getProgress() {
        return mProgress;
    }

    /**
     * Set the current progress to the specified value.
     *
     * @param progress The new progress, between 0.0 and 1.0.
     */
    public void setProgress(float progress) {
        assert 0.0f <= progress && progress <= 1.0f;
        if (mProgress == progress) return;

        mProgress = progress;

        if (useGradientDrawable()) {
            updateGradientDrawableProgress(progress);
        } else {
            getDrawable().setLevel(Math.round(progress * CLIP_DRAWABLE_MAX));
        }
        if (mProgressBarObserver != null) mProgressBarObserver.onVisibleProgressUpdated();
    }

    /**
     * Updates the progress of the foreground and background drawables.
     * The foreground drawable's level is set proportional to the progress,
     * and the background drawable's level is set to the inverse of the progress.
     *
     * @param progress The current progress value, between 0.0 and 1.0.
     */
    private void updateGradientDrawableProgress(float progress) {
        LayerDrawable layerDrawable = (LayerDrawable) getDrawable();
        ClipDrawable foregroundClip = (ClipDrawable) layerDrawable.getDrawable(0);
        foregroundClip.setLevel(Math.round(progress * CLIP_DRAWABLE_MAX));
        if (layerDrawable.getNumberOfLayers() >= 2) {
            ClipDrawable backgroundClip = (ClipDrawable) layerDrawable.getDrawable(1);
            backgroundClip.setLevel(Math.round((1.0f - progress) * CLIP_DRAWABLE_MAX));
        }
    }

    /** @return Foreground color of the progress bar. */
    public int getForegroundColor() {
        return mForegroundColor;
    }

    /**
     * @return Background color of the progress bar.
     */
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    /**
     * Get progress bar drawing information.
     *
     * @param drawingInfoOut An instance that the result will be written.
     */
    public void getDrawingInfo(DrawingInfo drawingInfoOut) {
        float effectiveAlpha = getVisibility() == VISIBLE ? getAlpha() : 0.0f;
        drawingInfoOut.progressBarColor = applyAlpha(mForegroundColor, effectiveAlpha);
        drawingInfoOut.progressBarBackgroundColor = applyAlpha(mBackgroundColor, effectiveAlpha);

        drawingInfoOut.cornerRadius = 0;
        if (useGradientDrawable()) {
            drawingInfoOut.cornerRadius = (float) (getBottom() - getTop()) / 2;
        }

        int endIndicatorSize = getBottom() - getTop();
        if (ViewCompat.getLayoutDirection(this) == LAYOUT_DIRECTION_LTR) {
            drawingInfoOut.progressBarRect.set(
                    getLeft(),
                    getTop(),
                    getLeft() + Math.round(mProgress * getWidth()),
                    getBottom());
            drawingInfoOut.progressBarBackgroundRect.set(
                    drawingInfoOut.progressBarRect.right, getTop(), getRight(), getBottom());
            drawingInfoOut.progressBarEndIndicator.set(
                    getRight() - endIndicatorSize,
                    getTop(),
                    getRight(),
                    getBottom());
        } else {
            drawingInfoOut.progressBarRect.set(
                    getRight() - Math.round(mProgress * getWidth()),
                    getTop(),
                    getRight(),
                    getBottom());
            drawingInfoOut.progressBarBackgroundRect.set(
                    getLeft(), getTop(), drawingInfoOut.progressBarRect.left, getBottom());
            drawingInfoOut.progressBarEndIndicator.set(
                    getLeft(),
                    getTop(),
                    getLeft() + endIndicatorSize,
                    getBottom());
        }
    }

    private void updateInternalVisibility() {
        int oldVisibility = getVisibility();
        int newVisibility = mDesiredVisibility;
        if (getAlpha() == 0 && mDesiredVisibility == VISIBLE) newVisibility = INVISIBLE;
        if (oldVisibility != newVisibility) {
            super.setVisibility(newVisibility);
            if (mProgressBarObserver != null) mProgressBarObserver.onVisibilityChanged();
        }
    }

    private int applyAlpha(int color, float alpha) {
        return (Math.round(alpha * (color >>> 24)) << 24) | (0x00ffffff & color);
    }

    // View implementations.

    /**
     * Note that this visibility might not be respected for optimization. For example, if alpha
     * is 0, it will remain View#INVISIBLE even if this is called with View#VISIBLE.
     */
    @Override
    public void setVisibility(int visibility) {
        mDesiredVisibility = visibility;
        updateInternalVisibility();
    }

    /**
     * Sets the color for the background of the progress bar.
     *
     * @param color The new color of the progress bar background.
     */
    @Override
    public void setBackgroundColor(int color) {
        if (useGradientDrawable()) {
            assert mBackgroundGradientDrawable != null;
            if (color == Color.TRANSPARENT) {
                mBackgroundGradientDrawable.setColor(null);
                super.setBackground(null);
            } else {
                // The updated progress bar will have a fully transparent background and a
                // moving background clip.
                mBackgroundGradientDrawable.setColor(color);
                mBackgroundColor = color;
                super.setBackgroundColor(Color.TRANSPARENT);
            }
        } else if (color == Color.TRANSPARENT) {
            setBackground(null);
        } else {
            super.setBackgroundColor(color);
            mBackgroundColor = color;
        }
    }

    /**
     * Sets the color for the foreground (i.e. the moving part) of the progress bar.
     * @param color The new color of the progress bar foreground.
     */
    public void setForegroundColor(int color) {
        if (useGradientDrawable()) {
            assert mForegroundGradientDrawable != null;
            assert mEndCapCircleDrawable != null;
            mForegroundGradientDrawable.setColor(color);
            mEndCapCircleDrawable.setColor(color);
        } else {
            assert mForegroundColorDrawable != null;
            mForegroundColorDrawable.setColor(color);
        }
        mForegroundColor = color;
    }

    @Override
    protected boolean onSetAlpha(int alpha) {
        updateInternalVisibility();
        return super.onSetAlpha(alpha);
    }
}
