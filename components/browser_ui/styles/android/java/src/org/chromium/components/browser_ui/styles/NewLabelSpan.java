// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.text.TextPaint;
import android.text.style.ReplacementSpan;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Draws the small raised "New" badge without letting it get clipped.
 *
 * <p>Use {@link NewLabelUtils} rather than this class.
 *
 * <p>SuperscriptSpan raises the baseline but never tells the layout it needs the extra room, so the
 * raised text spills out of the line box and TextView clips it off. Latin fonts leave enough gap
 * between top and ascent to hide that; Thai, Urdu and Devanagari don't, so their badges lose their
 * upper marks. See crbug.com/512323229.
 *
 * <p>A ReplacementSpan gets around it because {@link #getSize} is handed the line's FontMetricsInt
 * and can grow it. TextLine folds in whatever we report, on both the StaticLayout and BoringLayout
 * paths, so the line ends up tall enough for the badge.
 */
@NullMarked
class NewLabelSpan extends ReplacementSpan {
    /** Badge text size, relative to the text around it. */
    static final float DEFAULT_RELATIVE_SIZE = 0.75f;

    /**
     * How far to raise the badge, as a fraction of the surrounding ascent. Half the ascent is what
     * SuperscriptSpan uses, so switching over shouldn't move anything on screen.
     */
    static final float DEFAULT_BASELINE_SHIFT_RATIO = 0.5f;

    private final @ColorInt int mTextColor;
    private final float mRelativeSize;
    private final float mBaselineShiftRatio;
    private final TextPaint mBadgePaint = new TextPaint();
    private final Paint.FontMetricsInt mBaseMetrics = new Paint.FontMetricsInt();
    private final Paint.FontMetricsInt mBadgeMetrics = new Paint.FontMetricsInt();

    /**
     * @param textColor Colour of the badge text, usually an accent colour.
     * @param relativeSize Badge text size, relative to the text around it.
     * @param baselineShiftRatio How far to raise the badge, as a fraction of the surrounding
     *     ascent.
     */
    NewLabelSpan(@ColorInt int textColor, float relativeSize, float baselineShiftRatio) {
        mTextColor = textColor;
        mRelativeSize = relativeSize;
        mBaselineShiftRatio = baselineShiftRatio;
    }

    @Override
    public int getSize(
            Paint paint, CharSequence text, int start, int end, Paint.@Nullable FontMetricsInt fm) {
        TextPaint badgePaint = configureBadgePaint(paint);
        if (fm != null) {
            paint.getFontMetricsInt(mBaseMetrics);
            badgePaint.getFontMetricsInt(mBadgeMetrics);
            reserveSpaceForBadge(fm, mBaseMetrics, mBadgeMetrics, mBaselineShiftRatio);
        }
        return Math.round(badgePaint.measureText(text, start, end));
    }

    @Override
    public void draw(
            Canvas canvas,
            CharSequence text,
            int start,
            int end,
            float x,
            int top,
            int y,
            int bottom,
            Paint paint) {
        TextPaint badgePaint = configureBadgePaint(paint);
        paint.getFontMetricsInt(mBaseMetrics);
        int shift = baselineShift(mBaseMetrics, mBaselineShiftRatio);
        canvas.drawText(text, start, end, x, y + shift, badgePaint);
    }

    /**
     * Grows {@code fm} until the raised badge fits inside it.
     *
     * <p>This is plain arithmetic on metrics that get passed in, so tests can feed it made-up
     * tall-script numbers instead of needing a real font on the machine.
     *
     * @param fm The line metrics, modified in place.
     * @param baseMetrics Metrics of the text around the badge.
     * @param badgeMetrics Metrics of the smaller badge text.
     * @param baselineShiftRatio How far the badge is raised, as a fraction of the base ascent.
     */
    @VisibleForTesting
    static void reserveSpaceForBadge(
            Paint.FontMetricsInt fm,
            Paint.FontMetricsInt baseMetrics,
            Paint.FontMetricsInt badgeMetrics,
            float baselineShiftRatio) {
        int shift = baselineShift(baseMetrics, baselineShiftRatio);

        // top and ascent are negative above the baseline, so growing upwards means min(), not
        // max(). We fold in the base metrics as well because callers are allowed to hand us a
        // zeroed FontMetricsInt, and then this span is the only thing deciding the line height.
        fm.top = Math.min(fm.top, Math.min(baseMetrics.top, shift + badgeMetrics.top));
        fm.ascent = Math.min(fm.ascent, Math.min(baseMetrics.ascent, shift + badgeMetrics.ascent));

        // The badge sits above the baseline, so only the surrounding text can set the descent.
        fm.descent = Math.max(fm.descent, baseMetrics.descent);
        fm.bottom = Math.max(fm.bottom, baseMetrics.bottom);
    }

    /** Returns how far to move the badge's baseline. Negative means up. */
    @VisibleForTesting
    static int baselineShift(Paint.FontMetricsInt baseMetrics, float baselineShiftRatio) {
        return Math.round(baseMetrics.ascent * baselineShiftRatio);
    }

    /**
     * Copies {@code basePaint} into {@link #mBadgePaint} and styles it for the badge. Don't touch
     * the original; the whole layout shares it.
     */
    private TextPaint configureBadgePaint(Paint basePaint) {
        mBadgePaint.set(basePaint);
        mBadgePaint.setTextSize(basePaint.getTextSize() * mRelativeSize);
        mBadgePaint.setColor(mTextColor);
        return mBadgePaint;
    }
}
