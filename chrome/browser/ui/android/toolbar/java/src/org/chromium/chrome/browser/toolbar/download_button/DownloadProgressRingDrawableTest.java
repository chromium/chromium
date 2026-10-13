// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.download_button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;

/** Unit tests for {@link DownloadProgressRingDrawable}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadProgressRingDrawableTest {
    private static final float EPSILON = 0.001f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Canvas mCanvas;

    private Resources mResources;
    private DownloadProgressRingDrawable mDrawable;
    private float mRingRadiusPx;

    @Before
    public void setUp() {
        Context context = RuntimeEnvironment.getApplication();
        mResources = context.getResources();
        mRingRadiusPx = mResources.getDimension(R.dimen.download_progress_ring_radius);
        mDrawable = new DownloadProgressRingDrawable(context);
        mDrawable.setBounds(0, 0, 48, 48);
        mDrawable.setColors(Color.GRAY, Color.BLUE, Color.WHITE, Color.BLUE);
    }

    @Test
    public void testSweepFor_convertsAndClampsPercent() {
        assertEquals(0f, DownloadProgressRingDrawable.sweepFor(-10), EPSILON);
        assertEquals(0f, DownloadProgressRingDrawable.sweepFor(0), EPSILON);
        assertEquals(90f, DownloadProgressRingDrawable.sweepFor(25), EPSILON);
        assertEquals(180f, DownloadProgressRingDrawable.sweepFor(50), EPSILON);
        assertEquals(360f, DownloadProgressRingDrawable.sweepFor(100), EPSILON);
        assertEquals(360f, DownloadProgressRingDrawable.sweepFor(150), EPSILON);
    }

    @Test
    public void testBadgeTextFor_formatsCountAndCapsAtNinePlus() {
        assertEquals("", DownloadProgressRingDrawable.badgeTextFor(0));
        assertEquals("", DownloadProgressRingDrawable.badgeTextFor(1));
        assertEquals("2", DownloadProgressRingDrawable.badgeTextFor(2));
        assertEquals("9", DownloadProgressRingDrawable.badgeTextFor(9));
        assertEquals("9+", DownloadProgressRingDrawable.badgeTextFor(10));
        assertEquals("9+", DownloadProgressRingDrawable.badgeTextFor(42));
    }

    @Test
    public void testIdleState_drawsNothing() {
        // Even a stale badge count must not render without an in-progress ring.
        mDrawable.setBadgeCount(3);
        mDrawable.draw(mCanvas);
        verifyNoInteractions(mCanvas);
    }

    @Test
    public void testDraw_skipsWhenBoundsCannotFitRing() {
        mDrawable.setScanning();
        mDrawable.setBadgeCount(3);

        mDrawable.setBounds(0, 0, 0, 0);
        mDrawable.draw(mCanvas);
        // Smaller than the stroke width, so the computed radius is non-positive.
        mDrawable.setBounds(0, 0, 2, 2);
        mDrawable.draw(mCanvas);
        verifyNoInteractions(mCanvas);
    }

    @Test
    public void testDownloadingState_drawsTrackAndProgressArc() {
        mDrawable.setDownloading(50);
        mDrawable.draw(mCanvas);
        verify(mCanvas).drawOval(any(), any());
        verify(mCanvas)
                .drawArc(
                        any(),
                        eq(DownloadProgressRingDrawable.START_ANGLE),
                        eq(180f),
                        eq(false),
                        any());

        // At 0%, the track is drawn but the zero-sweep progress arc is skipped.
        clearInvocations(mCanvas);
        mDrawable.setDownloading(0);
        mDrawable.draw(mCanvas);
        verify(mCanvas).drawOval(any(), any());
        verify(mCanvas, never()).drawArc(any(), anyFloat(), anyFloat(), anyBoolean(), any());
    }

    @Test
    public void testScanningAnimation_lifecycleAcrossStatesAndVisibility() {
        // Scanning while hidden must not start the animator; becoming visible starts it.
        mDrawable.setVisible(/* visible= */ false, /* restart= */ false);
        mDrawable.setScanning();
        assertFalse(mDrawable.isScanningAnimationRunningForTesting());
        mDrawable.setVisible(/* visible= */ true, /* restart= */ false);
        assertTrue(mDrawable.isScanningAnimationRunningForTesting());

        mDrawable.draw(mCanvas);
        verify(mCanvas).drawOval(any(), any());
        verify(mCanvas)
                .drawArc(
                        any(),
                        anyFloat(),
                        eq(DownloadProgressRingDrawable.SCANNING_SWEEP_ANGLE),
                        eq(false),
                        any());

        // Hiding pauses the animator; switching to downloading or idle stops it.
        mDrawable.setVisible(/* visible= */ false, /* restart= */ false);
        assertFalse(mDrawable.isScanningAnimationRunningForTesting());
        mDrawable.setVisible(/* visible= */ true, /* restart= */ false);
        assertTrue(mDrawable.isScanningAnimationRunningForTesting());
        mDrawable.setDownloading(40);
        assertFalse(mDrawable.isScanningAnimationRunningForTesting());
        mDrawable.setScanning();
        mDrawable.setIdle();
        assertFalse(mDrawable.isScanningAnimationRunningForTesting());
    }

    @Test
    public void testBadge_drawsAtTrailingCornerAndMirrorsInRtl() {
        float centerX = mDrawable.getBounds().exactCenterX();
        mDrawable.setDownloading(50);
        mDrawable.setBadgeCount(3);

        mDrawable.draw(mCanvas);
        verify(mCanvas).drawCircle(eq(centerX + mRingRadiusPx), anyFloat(), anyFloat(), any());
        verify(mCanvas).drawText(eq("3"), eq(centerX + mRingRadiusPx), anyFloat(), any());

        clearInvocations(mCanvas);
        mDrawable.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mDrawable.draw(mCanvas);
        verify(mCanvas).drawCircle(eq(centerX - mRingRadiusPx), anyFloat(), anyFloat(), any());
    }

    @Test
    public void testPaintState_alphaComposesWithColorAndFilterForwards() {
        // A half-transparent track must stay half-transparent relative to the drawable alpha
        // rather than being overwritten by it.
        mDrawable.setColors(0x80FF0000, Color.BLUE, Color.WHITE, Color.BLUE);
        mDrawable.setDownloading(50);
        ArgumentCaptor<Paint> paintCaptor = ArgumentCaptor.forClass(Paint.class);

        mDrawable.draw(mCanvas);
        verify(mCanvas).drawOval(any(), paintCaptor.capture());
        assertEquals(0x80, Color.alpha(paintCaptor.getValue().getColor()));

        clearInvocations(mCanvas);
        mDrawable.setAlpha(128);
        ColorFilter filter = new PorterDuffColorFilter(Color.GREEN, PorterDuff.Mode.SRC_IN);
        mDrawable.setColorFilter(filter);
        mDrawable.draw(mCanvas);
        verify(mCanvas).drawOval(any(), paintCaptor.capture());
        assertEquals(128, mDrawable.getAlpha());
        assertEquals(0x80 * 128 / 255, Color.alpha(paintCaptor.getValue().getColor()));
        assertEquals(filter, paintCaptor.getValue().getColorFilter());
    }

    @Test
    public void testIntrinsicSize_coversRingAndBadge() {
        float badgeRadiusPx = mResources.getDimension(R.dimen.download_progress_ring_badge_radius);
        int expected = Math.round(2f * (mRingRadiusPx + badgeRadiusPx));
        assertEquals(expected, mDrawable.getIntrinsicWidth());
        assertEquals(expected, mDrawable.getIntrinsicHeight());
    }
}
