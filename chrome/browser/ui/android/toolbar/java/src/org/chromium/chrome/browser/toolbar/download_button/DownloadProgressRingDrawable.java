// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.download_button;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.animation.LinearInterpolator;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * Custom {@link Drawable} that renders the circular download progress ring, indeterminate scanning
 * animation, and concurrent-download count badge around the download toolbar button icon. Mirrors
 * desktop's {@code DownloadProgressRing} and {@code DownloadsImageBadge}.
 */
@NullMarked
public class DownloadProgressRingDrawable extends Drawable {
    /** Ring rendering mode, mirroring desktop's {@code ActionItemProgressRingStatus}. */
    @IntDef({RingStatus.IDLE, RingStatus.DOWNLOADING, RingStatus.SCANNING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RingStatus {
        /** No ring is drawn. */
        int IDLE = 0;

        /** Determinate ring filled according to the download progress percentage. */
        int DOWNLOADING = 1;

        /** Indeterminate continuously rotating arc. */
        int SCANNING = 2;
    }

    @VisibleForTesting static final float START_ANGLE = -90f;
    @VisibleForTesting static final float SCANNING_SWEEP_ANGLE = 90f;
    @VisibleForTesting static final long SCANNING_PERIOD_MS = 2500L;
    @VisibleForTesting static final int MIN_BADGE_COUNT = 2;
    @VisibleForTesting static final int MAX_BADGE_COUNT_DISPLAYED = 9;

    private final float mRingRadiusPx;
    private final float mBadgeRadiusPx;
    private final int mIntrinsicSizePx;
    private final float mBadgeTextBaselineOffsetPx;
    private final RectF mRingBounds = new RectF();

    private final Paint mTrackPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mProgressPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBadgeBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBadgeTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ValueAnimator mScanningAnimator;

    private @RingStatus int mStatus = RingStatus.IDLE;
    private int mProgressPercent;
    private int mBadgeCount;
    // Cached badgeTextFor(mBadgeCount) so draw() doesn't format per frame.
    private String mBadgeText = "";
    private float mRotationAngle;

    // Colors are stored unmodified so that setAlpha() can be composed with each color's own alpha
    // rather than overwriting it.
    private @ColorInt int mTrackColor = Color.TRANSPARENT;
    private @ColorInt int mProgressColor = Color.TRANSPARENT;
    private @ColorInt int mBadgeBackgroundColor = Color.TRANSPARENT;
    private @ColorInt int mBadgeTextColor = Color.TRANSPARENT;
    private int mAlpha = 255;

    /**
     * Creates a new {@link DownloadProgressRingDrawable}.
     *
     * @param context The Android context used to resolve dimensions.
     */
    public DownloadProgressRingDrawable(Context context) {
        Resources resources = context.getResources();
        mRingRadiusPx = resources.getDimension(R.dimen.download_progress_ring_radius);
        mBadgeRadiusPx = resources.getDimension(R.dimen.download_progress_ring_badge_radius);
        float strokeWidthPx = resources.getDimension(R.dimen.download_progress_ring_stroke_width);
        // The badge sits on the ring's bounding square corner, so it is the outermost element.
        mIntrinsicSizePx = Math.round(2f * (mRingRadiusPx + mBadgeRadiusPx));

        mTrackPaint.setStyle(Paint.Style.STROKE);
        mTrackPaint.setStrokeWidth(strokeWidthPx);

        mProgressPaint.setStyle(Paint.Style.STROKE);
        mProgressPaint.setStrokeWidth(strokeWidthPx);
        mProgressPaint.setStrokeCap(Paint.Cap.ROUND);

        mBadgeTextPaint.setTextAlign(Paint.Align.CENTER);
        mBadgeTextPaint.setTextSize(
                resources.getDimension(R.dimen.download_progress_ring_badge_text_size));
        mBadgeTextPaint.setTypeface(Typeface.DEFAULT_BOLD);
        Paint.FontMetrics fontMetrics = mBadgeTextPaint.getFontMetrics();
        mBadgeTextBaselineOffsetPx = -(fontMetrics.ascent + fontMetrics.descent) / 2f;
        // Paints default to opaque black; sync them with the transparent color defaults so nothing
        // is drawn until setColors() is called.
        applyPaintColors();

        mScanningAnimator = ValueAnimator.ofFloat(0f, 360f);
        mScanningAnimator.setDuration(SCANNING_PERIOD_MS);
        mScanningAnimator.setInterpolator(new LinearInterpolator());
        mScanningAnimator.setRepeatCount(ValueAnimator.INFINITE);
        mScanningAnimator.addUpdateListener(
                animator -> {
                    mRotationAngle = (float) animator.getAnimatedValue();
                    invalidateSelf();
                });
    }

    /** Hides the progress ring and stops any running animation. */
    public void setIdle() {
        mStatus = RingStatus.IDLE;
        stopScanningAnimation();
        invalidateSelf();
    }

    /**
     * Shows a determinate progress ring for {@code percent} in {@code [0, 100]}.
     *
     * @param percent The download progress percentage.
     */
    public void setDownloading(int percent) {
        mStatus = RingStatus.DOWNLOADING;
        mProgressPercent = MathUtils.clamp(percent, 0, 100);
        stopScanningAnimation();
        invalidateSelf();
    }

    /** Shows an indeterminate rotating progress ring. */
    public void setScanning() {
        mStatus = RingStatus.SCANNING;
        if (isVisible() && !mScanningAnimator.isStarted()) {
            mScanningAnimator.start();
        }
        invalidateSelf();
    }

    /**
     * Sets the concurrent download count to display in the badge. Values below {@link
     * #MIN_BADGE_COUNT} hide the badge; values above {@link #MAX_BADGE_COUNT_DISPLAYED} render as
     * {@code "9+"}.
     *
     * @param count Number of concurrent downloads, or 0 to hide the badge.
     */
    public void setBadgeCount(int count) {
        if (mBadgeCount == count) return;
        mBadgeCount = count;
        mBadgeText = badgeTextFor(count);
        invalidateSelf();
    }

    /**
     * Sets the colors used to draw the ring track, progress arc, and badge.
     *
     * @param trackColor Color of the full circular background track.
     * @param progressColor Color of the active/paused progress sweep arc.
     * @param badgeBackgroundColor Fill color of the circular badge background.
     * @param badgeTextColor Color of the count text inside the badge.
     */
    public void setColors(
            @ColorInt int trackColor,
            @ColorInt int progressColor,
            @ColorInt int badgeBackgroundColor,
            @ColorInt int badgeTextColor) {
        mTrackColor = trackColor;
        mProgressColor = progressColor;
        mBadgeBackgroundColor = badgeBackgroundColor;
        mBadgeTextColor = badgeTextColor;
        applyPaintColors();
    }

    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();
        if (bounds.isEmpty()) return;

        float centerX = bounds.exactCenterX();
        float centerY = bounds.exactCenterY();
        float maxRadius =
                (Math.min(bounds.width(), bounds.height()) - mTrackPaint.getStrokeWidth()) / 2f;
        float radius = Math.min(mRingRadiusPx, maxRadius);
        if (radius <= 0f) return;
        // Idle draws nothing, badge included: a count is meaningless without an in-progress ring.
        if (mStatus == RingStatus.IDLE) return;

        mRingBounds.set(centerX - radius, centerY - radius, centerX + radius, centerY + radius);
        canvas.drawOval(mRingBounds, mTrackPaint);

        if (mStatus == RingStatus.DOWNLOADING) {
            float sweepAngle = sweepFor(mProgressPercent);
            if (sweepAngle > 0f) {
                canvas.drawArc(
                        mRingBounds,
                        START_ANGLE,
                        sweepAngle,
                        /* useCenter= */ false,
                        mProgressPaint);
            }
        } else if (mStatus == RingStatus.SCANNING) {
            canvas.drawArc(
                    mRingBounds,
                    START_ANGLE + mRotationAngle,
                    SCANNING_SWEEP_ANGLE,
                    /* useCenter= */ false,
                    mProgressPaint);
        }

        if (!mBadgeText.isEmpty()) {
            // The badge sits at the top trailing corner; the ring itself is never mirrored since
            // clockwise progress reads the same in both layout directions.
            boolean isRtl = getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
            float badgeCenterX = isRtl ? centerX - radius : centerX + radius;
            float badgeCenterY = centerY - radius;
            canvas.drawCircle(badgeCenterX, badgeCenterY, mBadgeRadiusPx, mBadgeBackgroundPaint);
            canvas.drawText(
                    mBadgeText,
                    badgeCenterX,
                    badgeCenterY + mBadgeTextBaselineOffsetPx,
                    mBadgeTextPaint);
        }
    }

    @Override
    public int getIntrinsicWidth() {
        return mIntrinsicSizePx;
    }

    @Override
    public int getIntrinsicHeight() {
        return mIntrinsicSizePx;
    }

    @Override
    public boolean onLayoutDirectionChanged(int layoutDirection) {
        invalidateSelf();
        return true;
    }

    @Override
    public boolean setVisible(boolean visible, boolean restart) {
        boolean changed = super.setVisible(visible, restart);
        if (!visible) {
            stopScanningAnimation();
        } else if (mStatus == RingStatus.SCANNING && !mScanningAnimator.isStarted()) {
            mScanningAnimator.start();
        }
        return changed;
    }

    @Override
    public void setAlpha(int alpha) {
        if (mAlpha == alpha) return;
        mAlpha = alpha;
        applyPaintColors();
    }

    @Override
    public int getAlpha() {
        return mAlpha;
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mTrackPaint.setColorFilter(colorFilter);
        mProgressPaint.setColorFilter(colorFilter);
        mBadgeBackgroundPaint.setColorFilter(colorFilter);
        mBadgeTextPaint.setColorFilter(colorFilter);
        invalidateSelf();
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    private void applyPaintColors() {
        mTrackPaint.setColor(withAlpha(mTrackColor));
        mProgressPaint.setColor(withAlpha(mProgressColor));
        mBadgeBackgroundPaint.setColor(withAlpha(mBadgeBackgroundColor));
        mBadgeTextPaint.setColor(withAlpha(mBadgeTextColor));
        invalidateSelf();
    }

    /** Scales {@code color}'s own alpha by the drawable alpha so translucent colors survive. */
    private @ColorInt int withAlpha(@ColorInt int color) {
        int alpha = Color.alpha(color) * mAlpha / 255;
        return (color & 0x00FFFFFF) | (alpha << 24);
    }

    private void stopScanningAnimation() {
        if (mScanningAnimator.isStarted()) {
            mScanningAnimator.cancel();
        }
        mRotationAngle = 0f;
    }

    /** Converts a progress percentage in {@code [0, 100]} to a clockwise sweep angle in degrees. */
    @VisibleForTesting
    static float sweepFor(int percent) {
        return 360f * MathUtils.clamp(percent, 0, 100) / 100f;
    }

    /** Returns the badge label for {@code count}, or {@code ""} if no badge should be shown. */
    @VisibleForTesting
    static String badgeTextFor(int count) {
        if (count < MIN_BADGE_COUNT) {
            return "";
        }
        if (count > MAX_BADGE_COUNT_DISPLAYED) {
            return String.format(Locale.getDefault(), "%d+", MAX_BADGE_COUNT_DISPLAYED);
        }
        return String.format(Locale.getDefault(), "%d", count);
    }

    boolean isScanningAnimationRunningForTesting() {
        return mScanningAnimator.isStarted();
    }
}
