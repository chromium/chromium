// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.text.Spanned;
import android.text.TextPaint;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link NewLabelUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NewLabelUtilsUnitTest {
    /** Same shape as IDS_PREFS_NEW_LABEL, double space and all. */
    private static final String MARKED_UP = "Appearance  <new>New</new>";

    private static final String WITHOUT_MARKERS = "Appearance  New";
    private static final String WITHOUT_BADGE = "Appearance  ";
    private static final int BADGE_START = WITHOUT_BADGE.length();
    private static final int BADGE_END = WITHOUT_MARKERS.length();
    private static final float BASE_TEXT_SIZE = 48f;

    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
    }

    private static NewLabelSpan[] spansOf(CharSequence text) {
        return ((Spanned) text).getSpans(0, text.length(), NewLabelSpan.class);
    }

    /**
     * Draws the badge onto a fake canvas and hands back the paint it used.
     *
     * <p>Asking the span to measure itself would be the obvious way to check size, but Robolectric
     * doesn't have real fonts and its measureText ignores text size entirely. Reading the paint
     * back works no matter what fonts the machine has.
     */
    private static Paint capturedBadgePaint(CharSequence badged) {
        TextPaint paint = new TextPaint();
        paint.setTextSize(BASE_TEXT_SIZE);
        Canvas canvas = Mockito.mock(Canvas.class);

        spansOf(badged)[0].draw(canvas, badged, BADGE_START, BADGE_END, 0f, 0, 100, 120, paint);

        ArgumentCaptor<Paint> captor = ArgumentCaptor.forClass(Paint.class);
        verify(canvas)
                .drawText(
                        any(CharSequence.class),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        captor.capture());
        return captor.getValue();
    }

    @Test
    public void withBadge_stripsMarkersAndSpansOnlyTheBadge() {
        CharSequence result = NewLabelUtils.withBadge(mContext, MARKED_UP);

        assertEquals(
                "Markers should not make it into the displayed string",
                WITHOUT_MARKERS,
                result.toString());

        NewLabelSpan[] spans = spansOf(result);
        assertEquals("Expected exactly one badge span", 1, spans.length);

        Spanned spanned = (Spanned) result;
        assertEquals(
                "Span should start at the badge, not the title",
                BADGE_START,
                spanned.getSpanStart(spans[0]));
        assertEquals(BADGE_END, spanned.getSpanEnd(spans[0]));
    }

    @Test
    public void withoutBadge_dropsTheBadgeText() {
        CharSequence result = NewLabelUtils.withoutBadge(MARKED_UP);

        assertEquals("Badge text should be gone", WITHOUT_BADGE, result.toString());
        // Nothing was styled, so there is no need for this to be a Spanned at all.
        assertFalse("Plain text is expected when there's no badge", result instanceof Spanned);
    }

    /** The colour has to come from the theme, which is the reason this lives next to styles. */
    @Test
    public void withBadge_paintsTheBadgeInTheThemeAccentColour() {
        Paint badgePaint = capturedBadgePaint(NewLabelUtils.withBadge(mContext, MARKED_UP));

        assertEquals(
                SemanticColorUtils.getDefaultTextColorAccent1(mContext), badgePaint.getColor());
    }

    @Test
    public void withBadge_usesTheDefaultSizeWhenNoneIsGiven() {
        Paint badgePaint = capturedBadgePaint(NewLabelUtils.withBadge(mContext, MARKED_UP));

        assertEquals(
                BASE_TEXT_SIZE * NewLabelSpan.DEFAULT_RELATIVE_SIZE,
                badgePaint.getTextSize(),
                0.01f);
    }

    /** Autofill AI is the one surface that asks for a smaller badge, so the knob has to work. */
    @Test
    public void withBadge_passesTheGivenSizeThroughToTheBadge() {
        Paint badgePaint =
                capturedBadgePaint(
                        NewLabelUtils.withBadge(mContext, MARKED_UP, /* relativeTextSize= */ 0.6f));

        assertEquals(BASE_TEXT_SIZE * 0.6f, badgePaint.getTextSize(), 0.01f);
    }

    @Test
    public void withBadge_reservesVerticalRoomForTheBadge() {
        TextPaint paint = new TextPaint();
        paint.setTextSize(BASE_TEXT_SIZE);
        CharSequence result = NewLabelUtils.withBadge(mContext, MARKED_UP);

        Paint.FontMetricsInt fm = new Paint.FontMetricsInt();
        spansOf(result)[0].getSize(paint, result, BADGE_START, BADGE_END, fm);

        // The whole reason this span exists: the line gets told to make room above the baseline.
        assertTrue("Line should reserve room above the baseline", fm.top < 0);
        assertTrue(
                "Reserved room should go past the unshifted ascent",
                fm.top < paint.getFontMetricsInt().ascent);
    }
}
