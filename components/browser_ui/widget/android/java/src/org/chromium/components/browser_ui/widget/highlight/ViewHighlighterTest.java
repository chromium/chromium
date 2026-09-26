// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.highlight;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.ColorDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.R;

/** Tests the utility methods for highlighting of a view. */
@RunWith(BaseRobolectricTestRunner.class)
public class ViewHighlighterTest {
    private static final int DEFAULT_VIEW_WIDTH = 100;
    private static final int DEFAULT_VIEW_HEIGHT = 100;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final ViewHighlighter.HighlightParams mCircleParams =
            new ViewHighlighter.HighlightParams(ViewHighlighter.HighlightShape.CIRCLE);
    private final ViewHighlighter.HighlightParams mRectangleParams =
            new ViewHighlighter.HighlightParams(ViewHighlighter.HighlightShape.RECTANGLE);

    private @Mock Canvas mCanvas;

    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testRepeatedCallsToHighlightWorksCorrectly() {
        View tintedImageButton = new ImageView(mContext);
        // Create and lay out the view.
        tintedImageButton.measure(
                View.MeasureSpec.makeMeasureSpec(DEFAULT_VIEW_WIDTH, MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(DEFAULT_VIEW_HEIGHT, MeasureSpec.EXACTLY));
        tintedImageButton.layout(0, 0, DEFAULT_VIEW_WIDTH, DEFAULT_VIEW_HEIGHT);

        tintedImageButton.setBackground(new ColorDrawable(Color.LTGRAY));
        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOffHighlight(tintedImageButton);
        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOnHighlight(tintedImageButton, mCircleParams);
        ViewHighlighter.turnOnHighlight(tintedImageButton, mCircleParams);
        checkHighlightOn(tintedImageButton);

        ViewHighlighter.turnOffHighlight(tintedImageButton);
        ViewHighlighter.turnOffHighlight(tintedImageButton);
        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOnHighlight(tintedImageButton, mRectangleParams);
        checkHighlightOn(tintedImageButton);
    }

    @Test
    public void testViewWithNullBackground() {
        View tintedImageButton = new ImageView(mContext);
        // Create and lay out the view.
        tintedImageButton.measure(
                View.MeasureSpec.makeMeasureSpec(DEFAULT_VIEW_WIDTH, MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(DEFAULT_VIEW_HEIGHT, MeasureSpec.EXACTLY));
        tintedImageButton.layout(0, 0, DEFAULT_VIEW_WIDTH, DEFAULT_VIEW_HEIGHT);

        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOffHighlight(tintedImageButton);
        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOnHighlight(tintedImageButton, mCircleParams);
        checkHighlightOn(tintedImageButton);

        ViewHighlighter.turnOffHighlight(tintedImageButton);
        checkHighlightOff(tintedImageButton);

        ViewHighlighter.turnOnHighlight(tintedImageButton, mRectangleParams);
        checkHighlightOn(tintedImageButton);
    }

    @Test
    public void testHighlightExtension() {
        int highlightExtension = 10;
        View tintedImageButton = new ImageView(mContext);
        // Create and lay out the view.
        tintedImageButton.measure(
                View.MeasureSpec.makeMeasureSpec(DEFAULT_VIEW_WIDTH, MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(DEFAULT_VIEW_HEIGHT, MeasureSpec.EXACTLY));
        tintedImageButton.layout(0, 0, DEFAULT_VIEW_WIDTH, DEFAULT_VIEW_HEIGHT);

        ViewHighlighter.HighlightParams highlightParams =
                new ViewHighlighter.HighlightParams(ViewHighlighter.HighlightShape.RECTANGLE);
        highlightParams.setHighlightExtension(highlightExtension);

        ViewHighlighter.turnOnHighlight(tintedImageButton, highlightParams);
        checkHighlightOn(tintedImageButton);

        // Get the highlight.
        PulseDrawable pulseDrawable =
                (PulseDrawable) tintedImageButton.getTag(R.id.highlight_drawable);
        assertNotNull("Highlight should not be null", pulseDrawable);

        // Get the highlight bounds.
        Rect highlightBounds = pulseDrawable.getBounds();

        // Check that the bounds are configured properly.
        assertEquals(-highlightExtension, highlightBounds.left);
        assertEquals(-highlightExtension, highlightBounds.top);
        assertEquals(DEFAULT_VIEW_WIDTH + highlightExtension, highlightBounds.right);
        assertEquals(DEFAULT_VIEW_HEIGHT + highlightExtension, highlightBounds.bottom);

        // Verify that the highlight is drawn properly.
        ViewHighlighterTestUtils.drawPulseDrawable(tintedImageButton, mCanvas);
        RectF expectedBounds = new RectF(highlightBounds);
        Mockito.verify(mCanvas)
                .drawRoundRect(
                        Mockito.eq(expectedBounds),
                        Mockito.anyFloat(),
                        Mockito.anyFloat(),
                        Mockito.any());
    }

    /**
     * Assert that the provided view is highlighted.
     *
     * @param view The view of interest.
     */
    private static void checkHighlightOn(View view) {
        assertTrue(ViewHighlighterTestUtils.checkHighlightOn(view));
    }

    /**
     * Assert that the provided view is not highlighted.
     *
     * @param view The view of interest.
     */
    private static void checkHighlightOff(View view) {
        assertTrue(ViewHighlighterTestUtils.checkHighlightOff(view));
    }
}
