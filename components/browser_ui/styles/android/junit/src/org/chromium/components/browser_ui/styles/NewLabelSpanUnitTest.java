// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.text.TextPaint;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link NewLabelSpan}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NewLabelSpanUnitTest {
    private static final float RATIO = NewLabelSpan.DEFAULT_BASELINE_SHIFT_RATIO;

    /**
     * A font with no gap between top and ascent, like Thai or Urdu. There is no spare room up there
     * for a raised badge to borrow, which is why these scripts are the ones that clip.
     */
    private static Paint.FontMetricsInt tallScriptBaseMetrics() {
        return metrics(-30, -30, 8, 10);
    }

    /** A Latin-like font, with 6px of room to spare between top and ascent. */
    private static Paint.FontMetricsInt latinBaseMetrics() {
        return metrics(-30, -24, 8, 10);
    }

    private static Paint.FontMetricsInt badgeMetrics() {
        return metrics(-22, -22, 6, 8);
    }

    private static Paint.FontMetricsInt metrics(int top, int ascent, int descent, int bottom) {
        Paint.FontMetricsInt fm = new Paint.FontMetricsInt();
        fm.top = top;
        fm.ascent = ascent;
        fm.descent = descent;
        fm.bottom = bottom;
        return fm;
    }

    /** A span set up the way {@link NewLabelUtils} sets one up. */
    private static NewLabelSpan newSpan() {
        return new NewLabelSpan(Color.BLUE, NewLabelSpan.DEFAULT_RELATIVE_SIZE, RATIO);
    }

    // --- Just the arithmetic. No real fonts, so a bot's font set can't break these. ---

    /**
     * Shows the bug and the fix side by side. Shifting the baseline on its own, the way
     * SuperscriptSpan does, pushes the badge out of the line box when the font has no room to
     * spare. Reserving the room first keeps it in.
     *
     * <p>Both halves shift by the same amount, so the reservation is the only difference.
     */
    @Test
    public void baselineShiftAloneEscapesLineBox_reservingSpaceDoesNot() {
        Paint.FontMetricsInt base = tallScriptBaseMetrics();
        Paint.FontMetricsInt badge = badgeMetrics();

        // What SuperscriptSpan does: shift by half the ascent and leave the line metrics alone.
        // Written out instead of reusing RATIO so it keeps describing SuperscriptSpan even if our
        // own default drifts away from it later.
        int superscriptShift = Math.round(base.ascent * 0.5f);

        Paint.FontMetricsInt untouched = tallScriptBaseMetrics();
        assertTrue(
                "If a raised badge fits in the untouched line box, this test isn't reproducing"
                        + " the bug",
                superscriptShift + badge.top < untouched.top);

        // What NewLabelSpan does instead: ask for the room up front.
        Paint.FontMetricsInt reserved = tallScriptBaseMetrics();
        NewLabelSpan.reserveSpaceForBadge(reserved, base, badge, RATIO);

        int shift = NewLabelSpan.baselineShift(base, RATIO);
        assertTrue(
                "Badge top should fit inside the grown line box",
                shift + badge.top >= reserved.top);
        assertTrue(
                "Badge ascent should fit inside the grown line box",
                shift + badge.ascent >= reserved.ascent);
    }

    @Test
    public void reserveSpaceForBadge_expandsUpwardForTallScriptFont() {
        Paint.FontMetricsInt fm = tallScriptBaseMetrics();
        int originalTop = fm.top;
        int originalAscent = fm.ascent;

        NewLabelSpan.reserveSpaceForBadge(fm, tallScriptBaseMetrics(), badgeMetrics(), RATIO);

        // top and ascent are negative above the baseline, so growing means going more negative.
        assertTrue("top should have expanded upward", fm.top < originalTop);
        assertTrue("ascent should have expanded upward", fm.ascent < originalAscent);
    }

    @Test
    public void reserveSpaceForBadge_expandsUpwardForLatinFont() {
        Paint.FontMetricsInt fm = latinBaseMetrics();
        int originalAscent = fm.ascent;

        NewLabelSpan.reserveSpaceForBadge(fm, latinBaseMetrics(), badgeMetrics(), RATIO);

        assertTrue("ascent should have expanded upward", fm.ascent < originalAscent);
    }

    @Test
    public void reserveSpaceForBadge_doesNotShrinkTallerExistingMetrics() {
        Paint.FontMetricsInt fm = metrics(-100, -100, 50, 60);

        NewLabelSpan.reserveSpaceForBadge(fm, tallScriptBaseMetrics(), badgeMetrics(), RATIO);

        // Some other span on the same line may already have made it taller. Don't undo that.
        assertEquals("top shouldn't shrink", -100, fm.top);
        assertEquals("ascent shouldn't shrink", -100, fm.ascent);
        assertEquals("descent shouldn't shrink", 50, fm.descent);
        assertEquals("bottom shouldn't shrink", 60, fm.bottom);
    }

    @Test
    public void reserveSpaceForBadge_populatesZeroedMetrics() {
        // TextLine normally hands us a zeroed FontMetricsInt and merges the result afterwards,
        // but nothing guarantees it will, so we can't count on anyone else filling in the descent.
        Paint.FontMetricsInt fm = metrics(0, 0, 0, 0);
        Paint.FontMetricsInt base = tallScriptBaseMetrics();

        NewLabelSpan.reserveSpaceForBadge(fm, base, badgeMetrics(), RATIO);

        assertTrue("top should sit above the baseline", fm.top < 0);
        assertTrue("ascent should sit above the baseline", fm.ascent < 0);
        assertEquals("descent should come from the base font", base.descent, fm.descent);
        assertEquals("bottom should come from the base font", base.bottom, fm.bottom);
    }

    @Test
    public void baselineShift_raisesProportionallyToAscent() {
        Paint.FontMetricsInt base = tallScriptBaseMetrics();

        int shift = NewLabelSpan.baselineShift(base, RATIO);

        assertTrue("shift should be upward, so negative", shift < 0);
        assertEquals(Math.round(base.ascent * RATIO), shift);
        assertTrue(
                "badge should not be raised clear off the line",
                Math.abs(shift) < Math.abs(base.ascent));
    }

    @Test
    public void baselineShift_zeroRatioDoesNotMove() {
        assertEquals(0, NewLabelSpan.baselineShift(tallScriptBaseMetrics(), 0f));
    }

    // --- Now the real ReplacementSpan API, with a real Paint behind it. ---

    @Test
    public void getSize_neverShrinksMetrics() {
        NewLabelSpan span = newSpan();
        TextPaint paint = new TextPaint();
        paint.setTextSize(48f);
        Paint.FontMetricsInt fm = metrics(-100, -100, 50, 60);

        span.getSize(paint, "New", 0, 3, fm);

        // Same no-shrinking rule as above, but this time through the real API.
        assertTrue(fm.top <= -100);
        assertTrue(fm.ascent <= -100);
        assertTrue(fm.descent >= 50);
        assertTrue(fm.bottom >= 60);
    }

    @Test
    public void getSize_toleratesNullMetrics() {
        NewLabelSpan span = newSpan();
        TextPaint paint = new TextPaint();
        paint.setTextSize(48f);

        // TextLine passes null when all it wants back is the width.
        assertTrue(span.getSize(paint, "New", 0, 3, null) >= 0);
    }

    @Test
    public void getSize_doesNotMutateSourcePaint() {
        NewLabelSpan span = newSpan();
        TextPaint paint = new TextPaint();
        paint.setTextSize(48f);
        paint.setColor(Color.RED);

        span.getSize(paint, "New", 0, 3, new Paint.FontMetricsInt());

        // This paint belongs to the whole layout. Scribble on it and the title changes too.
        assertEquals("text size should be untouched", 48f, paint.getTextSize(), 0f);
        assertEquals("colour should be untouched", Color.RED, paint.getColor());
    }

    @Test
    public void draw_doesNotMutateSourcePaint() {
        NewLabelSpan span = newSpan();
        TextPaint paint = new TextPaint();
        paint.setTextSize(48f);
        paint.setColor(Color.RED);
        Canvas canvas = Mockito.mock(Canvas.class);

        span.draw(canvas, "New", 0, 3, 0f, 0, 100, 120, paint);

        assertEquals(48f, paint.getTextSize(), 0f);
        assertEquals(Color.RED, paint.getColor());
    }

    @Test
    public void draw_appliesTheSameShiftAsTheMetricCalculation() {
        NewLabelSpan span = newSpan();
        TextPaint paint = new TextPaint();
        paint.setTextSize(48f);
        Canvas canvas = Mockito.mock(Canvas.class);
        int baseline = 100;

        span.draw(canvas, "New", 0, 3, 0f, 0, baseline, 120, paint);

        // Drawing and measuring have to agree, or we reserve the room in the wrong place.
        int expectedShift = NewLabelSpan.baselineShift(paint.getFontMetricsInt(), RATIO);
        ArgumentCaptor<Float> y = ArgumentCaptor.forClass(Float.class);
        // The cast picks drawText(CharSequence, ...), the overload the span actually calls.
        verify(canvas)
                .drawText(
                        eq((CharSequence) "New"),
                        eq(0),
                        eq(3),
                        eq(0f),
                        y.capture(),
                        any(Paint.class));
        assertEquals(baseline + expectedShift, y.getValue(), 0f);
    }
}
