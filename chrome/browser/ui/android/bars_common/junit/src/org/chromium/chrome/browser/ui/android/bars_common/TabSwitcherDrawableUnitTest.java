// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.bars_common;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.View.MeasureSpec;
import android.widget.ImageView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.GraphicsMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.android.bars_common.TabSwitcherDrawable.TabSwitcherDrawableLocation;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

import java.util.function.Consumer;

/** Unit tests for optical text centering in {@link TabSwitcherDrawable}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(qualifiers = "xxhdpi")
public class TabSwitcherDrawableUnitTest {
    // Covers anti-aliasing and the part of the center-of-mass offset that is left uncorrected.
    private static final float TOLERANCE_PX = 1.0f;
    private static final int BUTTON_SIZE_PX = 144;

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testLabelsCenteredInButtonView() {
        TabSwitcherButtonView button =
                new TabSwitcherButtonView(mActivity, Robolectric.buildAttributeSet().build());
        button.setBackground(null);
        button.setPadding(0, 0, 0, 0);
        button.setScaleType(ImageView.ScaleType.CENTER);
        button.onFinishInflate();
        int measureSpec = MeasureSpec.makeMeasureSpec(BUTTON_SIZE_PX, MeasureSpec.EXACTLY);
        button.measure(measureSpec, measureSpec);
        button.layout(0, 0, BUTTON_SIZE_PX, BUTTON_SIZE_PX);
        Bitmap iconOnly = render(button::draw, BUTTON_SIZE_PX, BUTTON_SIZE_PX);
        float center = BUTTON_SIZE_PX / 2f;

        // Covers one and two digit sizes, asymmetric glyphs such as "1", and the ":D" overflow.
        for (int tabCount : new int[] {1, 3, 4, 7, 8, 11, 47, 71, 88, 100}) {
            button.setTabCount(tabCount, /* isIncognito= */ false);
            assertInkCentered(
                    render(button::draw, BUTTON_SIZE_PX, BUTTON_SIZE_PX),
                    iconOnly,
                    center,
                    center,
                    "count " + tabCount);
        }
        button.setTabCount(100, /* isIncognito= */ true);
        assertInkCentered(
                render(button::draw, BUTTON_SIZE_PX, BUTTON_SIZE_PX),
                iconOnly,
                center,
                center,
                "incognito overflow");
    }

    @Test
    public void testLabelFollowsDrawableBounds() {
        TabSwitcherDrawable drawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
                        mActivity,
                        BrandedColorScheme.APP_DEFAULT,
                        TabSwitcherDrawableLocation.TAB_TOOLBAR);
        // The second pass draws the same label again, so it uses the cached measurement.
        for (Rect bounds : new Rect[] {new Rect(10, 20, 82, 92), new Rect(14, 24, 86, 96)}) {
            drawable.setBounds(bounds);
            drawable.updateForTabCount(0, /* incognito= */ false);
            Bitmap iconOnly = render(drawable::draw, 100, 110);
            drawable.updateForTabCount(11, /* incognito= */ false);
            assertInkCentered(
                    render(drawable::draw, 100, 110),
                    iconOnly,
                    bounds.exactCenterX(),
                    bounds.exactCenterY(),
                    "bounds " + bounds);
        }
    }

    @Test
    public void testCountsShareBaseline() {
        TabSwitcherDrawable drawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
                        mActivity,
                        BrandedColorScheme.APP_DEFAULT,
                        TabSwitcherDrawableLocation.TAB_TOOLBAR);
        // An odd height puts the center between two pixels, where centering each count on its
        // own ink would round "11" and "81" to different baselines.
        drawable.setBounds(0, 0, 72, 73);
        Bitmap iconOnly = render(drawable::draw, 72, 73);
        drawable.updateForTabCount(11, /* incognito= */ false);
        int baseline = trailingStemBottom(render(drawable::draw, 72, 73), iconOnly);
        drawable.updateForTabCount(81, /* incognito= */ false);
        assertEquals(baseline, trailingStemBottom(render(drawable::draw, 72, 73), iconOnly));
    }

    private static Bitmap render(Consumer<Canvas> draw, int width, int height) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        draw.accept(new Canvas(bitmap));
        return bitmap;
    }

    /** Returns the lowest label row in the rightmost label column: the foot of a trailing "1". */
    private static int trailingStemBottom(Bitmap bitmap, Bitmap iconOnly) {
        for (int x = bitmap.getWidth() - 1; x >= 0; x--) {
            for (int y = bitmap.getHeight() - 1; y >= 0; y--) {
                if (bitmap.getPixel(x, y) != iconOnly.getPixel(x, y)) return y;
            }
        }
        throw new AssertionError("No label drawn");
    }

    /**
     * Asserts that the label pixels, i.e. those that differ from {@code iconOnly}, have their
     * horizontal center of mass and vertical bounding-box center at the given point.
     */
    private static void assertInkCentered(
            Bitmap bitmap, Bitmap iconOnly, float centerX, float centerY, String message) {
        long totalAlpha = 0;
        double weightedX = 0;
        int minY = bitmap.getHeight();
        int maxY = -1;
        for (int y = 0; y < bitmap.getHeight(); y++) {
            for (int x = 0; x < bitmap.getWidth(); x++) {
                int pixel = bitmap.getPixel(x, y);
                if (pixel == iconOnly.getPixel(x, y)) continue;
                int alpha = Color.alpha(pixel);
                totalAlpha += alpha;
                weightedX += alpha * (x + 0.5);
                minY = Math.min(minY, y);
                maxY = Math.max(maxY, y);
            }
        }
        assertTrue(message + ": no label drawn", totalAlpha > 0);
        assertEquals(
                message + ": ink center of mass X",
                centerX,
                (float) (weightedX / totalAlpha),
                TOLERANCE_PX);
        assertEquals(
                message + ": ink bounding-box center Y",
                centerY,
                (minY + maxY + 1) / 2f,
                TOLERANCE_PX);
    }
}
