// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.highlight;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.ColorDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.ImageView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.widget.test.R;

/** Tests the utility methods for highlighting of a view. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ViewHighlighterTest {
    @Mock Canvas mCanvas;

    private Context mContext;
    private final ViewHighlighter.HighlightParams mCircleParams =
            new ViewHighlighter.HighlightParams(ViewHighlighter.HighlightShape.CIRCLE);
    private final ViewHighlighter.HighlightParams mRectangleParams =
            new ViewHighlighter.HighlightParams(ViewHighlighter.HighlightShape.RECTANGLE);

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        InstrumentationRegistry.getTargetContext(),
                        R.style.Theme_BrowserUI_DayNight);
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @MediumTest
    public void testRepeatedCallsToHighlightWorksCorrectly() {
        View tintedImageButton = new ImageView(mContext);
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
    @MediumTest
    public void testViewWithNullBackground() {
        View tintedImageButton = new ImageView(mContext);
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
    @MediumTest
    public void testHighlightExtension() {
        int highlightExtension = 10;
        View tintedImageButton = new ImageView(mContext);
        ViewHighlighter.HighlightParams highlightParams =
                new ViewHighlighter.HighlightParams(ViewHighlighter.HighlightShape.RECTANGLE);
        highlightParams.setHighlightExtension(highlightExtension);

        ViewHighlighter.turnOnHighlight(tintedImageButton, highlightParams);
        checkHighlightOn(tintedImageButton);

        Rect viewBounds = tintedImageButton.getBackground().getBounds();
        RectF expectedBounds =
                new RectF(
                        viewBounds.left - highlightExtension,
                        viewBounds.top - highlightExtension,
                        viewBounds.right + highlightExtension,
                        viewBounds.bottom + highlightExtension);

        ViewHighlighterTestUtils.drawPulseDrawable(tintedImageButton, mCanvas);

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
        Assert.assertTrue(ViewHighlighterTestUtils.checkHighlightOn(view));
    }

    /**
     * Assert that the provided view is not highlighted.
     *
     * @param view The view of interest.
     */
    private static void checkHighlightOff(View view) {
        Assert.assertTrue(ViewHighlighterTestUtils.checkHighlightOff(view));
    }
}
