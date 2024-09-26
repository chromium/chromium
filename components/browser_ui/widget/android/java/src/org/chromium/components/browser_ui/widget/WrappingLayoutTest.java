// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;

import java.util.Arrays;
import java.util.List;

/** Tests for the {@link WrappingLayout} class. */
@RunWith(ParameterizedRunner.class)
@Batch(Batch.UNIT_TESTS)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public class WrappingLayoutTest {
    /** A class providing input parameters for the test below. */
    public static class WrappingLayoutTestParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    // Test function expects: measureSpec, leftTopPadding, bottomRightPadding,
                    // margin, spacing.
                    new ParameterSet()
                            .value(MeasureSpec.UNSPECIFIED, 0, 0, 0, 0)
                            .name("UnboundedCompact"),
                    new ParameterSet()
                            .value(MeasureSpec.UNSPECIFIED, 15, 15, 0, 0)
                            .name("UnboundedWithPadding"),
                    new ParameterSet()
                            .value(MeasureSpec.UNSPECIFIED, 0, 0, 10, 0)
                            .name("UnboundedWithMargin"),
                    new ParameterSet()
                            .value(MeasureSpec.UNSPECIFIED, 15, 15, 10, 0)
                            .name("UnboundedWithPaddingAndMargin"),
                    new ParameterSet()
                            .value(MeasureSpec.UNSPECIFIED, 0, 0, 0, 3)
                            .name("UnboundedWithSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.UNSPECIFIED, 0, 0, 10, 3)
                            .name("UnboundedWithMarginAndSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.UNSPECIFIED, 15, 15, 0, 3)
                            .name("UnboundedWithPaddingAndSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.UNSPECIFIED, 15, 15, 10, 3)
                            .name("UnboundedWithPaddingMarginAndSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.UNSPECIFIED, 10, 15, 10, 3)
                            .name("UnboundedWithMismatchedPaddingMarginAndSpacing"),
                    new ParameterSet().value(MeasureSpec.EXACTLY, 0, 0, 0, 0).name("ExactCompact"),
                    new ParameterSet()
                            .value(MeasureSpec.EXACTLY, 15, 15, 0, 0)
                            .name("ExactWithPadding"),
                    new ParameterSet()
                            .value(MeasureSpec.EXACTLY, 0, 0, 10, 0)
                            .name("ExactWithMargin"),
                    new ParameterSet()
                            .value(MeasureSpec.EXACTLY, 15, 15, 10, 0)
                            .name("ExactWithPaddingAndMargin"),
                    new ParameterSet()
                            .value(MeasureSpec.EXACTLY, 0, 0, 0, 3)
                            .name("ExactWithSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.EXACTLY, 0, 0, 10, 3)
                            .name("ExactWithMarginAndSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.EXACTLY, 15, 15, 0, 3)
                            .name("ExactWithPaddingAndSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.EXACTLY, 15, 15, 10, 3)
                            .name("ExactWithPaddingMarginAndSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.EXACTLY, 10, 15, 10, 3)
                            .name("ExactWithMismatchedPaddingMarginAndSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.AT_MOST, 0, 0, 0, 0)
                            .name("BoundedCompact"),
                    new ParameterSet()
                            .value(MeasureSpec.AT_MOST, 15, 15, 0, 0)
                            .name("BoundedWithPadding"),
                    new ParameterSet()
                            .value(MeasureSpec.AT_MOST, 0, 0, 10, 0)
                            .name("BoundedWithMargin"),
                    new ParameterSet()
                            .value(MeasureSpec.AT_MOST, 15, 15, 10, 0)
                            .name("BoundedWithPaddingAndMargin"),
                    new ParameterSet()
                            .value(MeasureSpec.AT_MOST, 0, 0, 0, 3)
                            .name("BoundedWithspacing"),
                    new ParameterSet()
                            .value(MeasureSpec.AT_MOST, 0, 0, 10, 3)
                            .name("BoundedWithMarginAndSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.AT_MOST, 15, 15, 0, 3)
                            .name("BoundedWithPaddingAndSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.AT_MOST, 15, 15, 10, 3)
                            .name("BoundedWithPaddingMarginAndSpacing"),
                    new ParameterSet()
                            .value(MeasureSpec.AT_MOST, 10, 15, 10, 3)
                            .name("BoundedWithMismatchedPaddingMarginAndSpacing"));
        }
    }

    private static class WrappingLayoutSubclass extends WrappingLayout {
        Context mContext;
        int mRequestedWidth;
        int mRequestedHeight;
        View mViewA;
        View mViewB;
        View mViewC;

        public static WrappingLayoutSubclass create(
                Context context, int leftTopPadding, int bottomRightPadding, int spacing) {
            WrappingLayoutSubclass layout = new WrappingLayoutSubclass(context);
            layout.setPadding(
                    leftTopPadding, leftTopPadding, bottomRightPadding, bottomRightPadding);
            if (spacing > 0) layout.setSpacingBetweenViews(spacing, spacing);
            return layout;
        }

        private WrappingLayoutSubclass(Context context) {
            super(context);
            mContext = context;
        }

        public View getView(String id) {
            View view = null;
            if (VIEW_A_ID.equals(id)) {
                view = mViewA;
            } else if (VIEW_B_ID.equals(id)) {
                view = mViewB;
            } else if (VIEW_C_ID.equals(id)) {
                view = mViewC;
            }
            Assert.assertTrue(view != null);
            return view;
        }

        public void addTestViews(int margin) {
            addView("hidden", 200, 100, margin, View.GONE);
            mViewA = addView(VIEW_A_ID, VIEW_A_WIDTH, VIEW_A_HEIGHT, margin, View.VISIBLE);
            mViewB = addView(VIEW_B_ID, VIEW_B_WIDTH, VIEW_B_HEIGHT, margin, View.VISIBLE);
            addView("hidden", 1, 1, margin, View.GONE);
            mViewC = addView(VIEW_C_ID, VIEW_C_WIDTH, VIEW_C_HEIGHT, margin, View.VISIBLE);
            addView("hidden", 1, 1, margin, View.GONE);
        }

        public void addTestViewsReverseOrder(int margin) {
            addView("hidden", 200, 100, margin, View.GONE);
            mViewC = addView(VIEW_C_ID, VIEW_C_WIDTH, VIEW_C_HEIGHT, margin, View.VISIBLE);
            mViewB = addView(VIEW_B_ID, VIEW_B_WIDTH, VIEW_B_HEIGHT, margin, View.VISIBLE);
            addView("hidden", 1, 1, margin, View.GONE);
            mViewA = addView(VIEW_A_ID, VIEW_A_WIDTH, VIEW_A_HEIGHT, margin, View.VISIBLE);
            addView("hidden", 1, 1, margin, View.GONE);
        }

        public View addView(String tag, int width, int height, int margin, int visibility) {
            View view = new View(mContext);
            view.setTag(tag);
            if (visibility == View.VISIBLE) {
                MarginLayoutParams params = new MarginLayoutParams(width, height);
                params.setMargins(margin, margin, margin, margin);
                view.setLayoutParams(params);
            } else {
                view.setVisibility(View.GONE);
            }
            super.addView(view);
            return view;
        }

        public void layoutAtSize(
                int width,
                int height,
                int measureSpecWidth,
                int measureSpecHeight,
                int leftTopPadding,
                int bottomRightPadding) {
            mRequestedWidth = width + leftTopPadding + bottomRightPadding;
            mRequestedHeight = height + leftTopPadding + bottomRightPadding;

            int specWidth = MeasureSpec.makeMeasureSpec(mRequestedWidth, measureSpecWidth);
            int specHeight = MeasureSpec.makeMeasureSpec(mRequestedHeight, measureSpecHeight);
            measure(specWidth, specHeight);
            layout(0, 0, mRequestedWidth, mRequestedHeight);
        }

        /**
         * Validates the calculated width and height for the view is correct.
         *
         * @param expectedWidth The expected width of the view, not accounting for layout padding
         *     and view margins.
         * @param expectedHeight The expected height of the view, not accounting for layout padding
         *     and view margins.
         * @param expectedMaxCols The max number of fully visible (not truncated) columns to expect
         *     the views to line up in, once laid out.
         * @param expectedMaxRows The max number of fully visible (not truncated) rows to expect the
         *     views to line up in, once laid out.
         * @param margin The amount of margin (in pixels) to account for during calculations.
         * @param leftTopPadding The amount of padding (in pixels) to account for during
         *     calculations on the left and at the top.
         * @param bottomRightPadding The amount of padding (in pixels) to account for during
         *     calculations on the right and at the bottom.
         * @param spacing The amount of spacing (in pixels) to account for (between views) during
         *     calculations.
         * @param measureSpecWidth The measureSpec to use for width (EXACT, AT_MOST, UNSPECIFIED).
         * @param measureSpecHeight The measureSpec to use for height (EXACT, AT_MOST, UNSPECIFIED).
         */
        public void validateCalculatedSize(
                int expectedWidth,
                int expectedHeight,
                int expectedMaxRows,
                int expectedMaxCols,
                int margin,
                int leftTopPadding,
                int bottomRightPadding,
                int spacing,
                int measureSpecWidth,
                int measureSpecHeight) {
            int measuredWidth = getMeasuredWidth();
            int measuredHeight = getMeasuredHeight();
            String message =
                    "Layout is incorrectly sized. Note: Requested margin was: "
                            + margin
                            + ", padding: ("
                            + leftTopPadding
                            + ","
                            + bottomRightPadding
                            + "), and spacing in-between: "
                            + spacing
                            + ". Failure checking Layout.";
            if (measureSpecWidth == MeasureSpec.EXACTLY
                    && measureSpecHeight == MeasureSpec.EXACTLY) {
                Assert.assertEquals(message + "width:", mRequestedWidth, measuredWidth);
                Assert.assertEquals(message + "height:", mRequestedHeight, measuredHeight);
            } else {
                int expectedWidthWithBuffers =
                        expectedWidth
                                + leftTopPadding
                                + bottomRightPadding
                                + (expectedMaxCols * 2 * margin)
                                + (expectedMaxCols > 0 ? (expectedMaxCols - 1) * spacing : 0);
                int expectedHeightWithBuffers =
                        expectedHeight
                                + leftTopPadding
                                + bottomRightPadding
                                + (expectedMaxRows * 2 * margin)
                                + (expectedMaxRows > 0 ? (expectedMaxRows - 1) * spacing : 0);

                Assert.assertEquals(message + "width:", expectedWidthWithBuffers, measuredWidth);
                Assert.assertEquals(message + "height:", expectedHeightWithBuffers, measuredHeight);
            }
        }

        /**
         * Validates that a |view| is correctly positioned.
         *
         * @param viewId The ID of the view to validate.
         * @param column The column we expect the view to be in (zero-based).
         * @param row The row we expect the view to be in (zero-based).
         * @param expectedLeft The expected left position of the view, before accounting for layout
         *     padding and view margins.
         * @param expectedTop The expected left position of the view, before accounting for layout
         *     padding and view margins.
         * @param expectedWidth The expected width of the view, before accounting for layout padding
         *     and view margins.
         * @param expectedHeight The expected height of the view, before accounting for layout
         *     padding and view margins.
         * @param margin The amount of margin (in pixels) to account for during calculations.
         * @param leftTopPadding The amount of padding (in pixels) to account for during
         *     calculations on the left and at the top.
         * @param spacing The amount of spacing (in pixels) to account for (between views) during
         *     calculations.
         */
        public void validateView(
                String viewId,
                int column,
                int row,
                int expectedLeft,
                int expectedTop,
                int expectedWidth,
                int expectedHeight,
                int margin,
                int leftTopPadding,
                int spacing) {
            View view = getView(viewId);

            int left = view.getLeft();
            int right = view.getRight();
            int top = view.getTop();
            int bottom = view.getBottom();
            int expectedLeftWithBuffers =
                    expectedLeft
                            + leftTopPadding
                            + ((1 + 2 * column) * margin)
                            + (column * spacing);
            int expectedTopWithBuffers =
                    expectedTop + leftTopPadding + ((1 + 2 * row) * margin) + (row * spacing);
            String message =
                    "View '"
                            + view.getTag()
                            + "', expected to be in (zero-based) col "
                            + column
                            + ", row "
                            + row
                            + ", is incorrectly positioned. Note: Requested margin was: "
                            + margin
                            + ", padding: "
                            + leftTopPadding
                            + ", and spacing: "
                            + spacing
                            + ". Failure checking View.";
            Assert.assertEquals(message + "left:", expectedLeftWithBuffers, left);
            Assert.assertEquals(message + "top:", expectedTopWithBuffers, top);
            Assert.assertEquals(message + "width:", expectedWidth, right - left);
            Assert.assertEquals(message + "height:", expectedHeight, bottom - top);
        }

        public void assertViewExpectations(
                ViewExpectation expectationA,
                ViewExpectation expectationB,
                ViewExpectation expectationC) {
            expectationA.assertMatchExpectation(mViewA);
            expectationB.assertMatchExpectation(mViewB);
            expectationC.assertMatchExpectation(mViewC);
        }
    }

    private static class ViewExpectation {
        private int mExpectedLeft;
        private int mExpectedTop;
        private int mExpectedWidth;
        private int mExpectedHeight;

        public ViewExpectation(
                int expectedLeft, int expectedTop, int expectedWidth, int expectedHeight) {
            mExpectedLeft = expectedLeft;
            mExpectedTop = expectedTop;
            mExpectedWidth = expectedWidth;
            mExpectedHeight = expectedHeight;
        }

        public void assertMatchExpectation(View view) {
            Assert.assertEquals(mExpectedLeft, view.getLeft());
            Assert.assertEquals(mExpectedTop, view.getTop());
            Assert.assertEquals(mExpectedWidth, view.getWidth());
            Assert.assertEquals(mExpectedHeight, view.getHeight());
        }
    }

    private Context mContext;

    // Constants that improve readability of the tests.
    private static final int FIRST_COL = 0;
    private static final int SECOND_COL = 1;
    private static final int THIRD_COL = 2;
    private static final int FIRST_ROW = 0;
    private static final int SECOND_ROW = 1;
    private static final int THIRD_ROW = 2;

    private static final int TRUNCATED_COLS = 0;
    private static final int ONE_COL = 1;
    private static final int TWO_COLS = 2;
    private static final int THREE_COLS = 3;
    private static final int ONE_ROW = 1;
    private static final int TWO_ROWS = 2;
    private static final int THREE_ROWS = 3;

    private static final String VIEW_A_ID = "a";
    private static final int VIEW_A_WIDTH = 180;
    private static final int VIEW_A_HEIGHT = 90;

    private static final String VIEW_B_ID = "b";
    private static final int VIEW_B_WIDTH = 190;
    private static final int VIEW_B_HEIGHT = 100;

    private static final String VIEW_C_ID = "c";
    private static final int VIEW_C_WIDTH = 200;
    private static final int VIEW_C_HEIGHT = 110;

    @Before
    public void setUp() throws Exception {
        mContext =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
    }

    public void testNoWrapping(
            int specWidth,
            int specHeight,
            int leftTopPadding,
            int bottomRightPadding,
            int margin,
            int spacing) {
        WrappingLayoutSubclass layout =
                WrappingLayoutSubclass.create(
                        mContext, leftTopPadding, bottomRightPadding, spacing);
        layout.addTestViews(margin);

        // This tests the easy case (no wrapping). It creates a a layout of 1000x400 (with room
        // for all views) and verifies all views fit within the first row.
        layout.layoutAtSize(1000, 400, specWidth, specHeight, leftTopPadding, bottomRightPadding);
        layout.validateCalculatedSize(
                570,
                110,
                ONE_ROW,
                THREE_COLS,
                margin,
                leftTopPadding,
                bottomRightPadding,
                spacing,
                specWidth,
                specHeight);
        layout.validateView(
                VIEW_A_ID,
                FIRST_COL,
                FIRST_ROW,
                0,
                0,
                VIEW_A_WIDTH,
                VIEW_A_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_B_ID,
                SECOND_COL,
                FIRST_ROW,
                180,
                0,
                VIEW_B_WIDTH,
                VIEW_B_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_C_ID,
                THIRD_COL,
                FIRST_ROW,
                370,
                0,
                VIEW_C_WIDTH,
                VIEW_C_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
    }

    public void testAllTruncated(
            int specWidth,
            int specHeight,
            int leftTopPadding,
            int bottomRightPadding,
            int margin,
            int spacing) {
        WrappingLayoutSubclass layout =
                WrappingLayoutSubclass.create(
                        mContext, leftTopPadding, bottomRightPadding, spacing);
        layout.addTestViews(margin);

        // This is the worst-case scenario: Not enough horizontal room for any view because the
        // layout is only 100 in width, but the views are between 180 and 200 each.
        layout.layoutAtSize(100, 400, specWidth, specHeight, leftTopPadding, bottomRightPadding);
        layout.validateCalculatedSize(
                100,
                300,
                THREE_ROWS,
                TRUNCATED_COLS,
                margin,
                leftTopPadding,
                bottomRightPadding,
                spacing,
                specWidth,
                specHeight);
        layout.validateView(
                VIEW_A_ID,
                FIRST_COL,
                FIRST_ROW,
                0,
                0,
                VIEW_A_WIDTH,
                VIEW_A_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_B_ID,
                FIRST_COL,
                SECOND_ROW,
                0,
                90,
                VIEW_B_WIDTH,
                VIEW_B_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_C_ID,
                FIRST_COL,
                THIRD_ROW,
                0,
                190,
                VIEW_C_WIDTH,
                VIEW_C_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
    }

    public void testOnePerLineWithSpace(
            int specWidth,
            int specHeight,
            int leftTopPadding,
            int bottomRightPadding,
            int margin,
            int spacing) {
        WrappingLayoutSubclass layout =
                WrappingLayoutSubclass.create(
                        mContext, leftTopPadding, bottomRightPadding, spacing);
        layout.addTestViews(margin);

        // This tests that wrapping happens correctly when there is only enough space for one
        // view per line (essentially the same test as above, except no truncation occurs).
        layout.layoutAtSize(
                200 + 2 * margin, 400, specWidth, specHeight, leftTopPadding, bottomRightPadding);
        layout.validateCalculatedSize(
                200,
                300,
                THREE_ROWS,
                ONE_COL,
                margin,
                leftTopPadding,
                bottomRightPadding,
                spacing,
                specWidth,
                specHeight);
        layout.validateView(
                VIEW_A_ID,
                FIRST_COL,
                FIRST_ROW,
                0,
                0,
                VIEW_A_WIDTH,
                VIEW_A_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_B_ID,
                FIRST_COL,
                SECOND_ROW,
                0,
                90,
                VIEW_B_WIDTH,
                VIEW_B_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_C_ID,
                FIRST_COL,
                THIRD_ROW,
                0,
                190,
                VIEW_C_WIDTH,
                VIEW_C_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
    }

    public void testTwoOnFirstOneOnNext(
            int specWidth,
            int specHeight,
            int leftTopPadding,
            int bottomRightPadding,
            int margin,
            int spacing) {
        WrappingLayoutSubclass layout =
                WrappingLayoutSubclass.create(
                        mContext, leftTopPadding, bottomRightPadding, spacing);
        layout.addTestViews(margin);

        // Test what happens if there is room for two views on the first row, and one on the
        // second.
        layout.layoutAtSize(
                400 + 2 * TWO_COLS * margin,
                400,
                specWidth,
                specHeight,
                leftTopPadding,
                bottomRightPadding);
        layout.validateCalculatedSize(
                370,
                210,
                TWO_ROWS,
                TWO_COLS,
                margin,
                leftTopPadding,
                bottomRightPadding,
                spacing,
                specWidth,
                specHeight);
        layout.validateView(
                VIEW_A_ID,
                FIRST_COL,
                FIRST_ROW,
                0,
                0,
                VIEW_A_WIDTH,
                VIEW_A_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_B_ID,
                SECOND_COL,
                FIRST_ROW,
                180,
                0,
                VIEW_B_WIDTH,
                VIEW_B_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_C_ID,
                FIRST_COL,
                SECOND_ROW,
                0,
                100,
                VIEW_C_WIDTH,
                VIEW_C_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
    }

    public void testUnbounded(
            int specHeight, int leftTopPadding, int bottomRightPadding, int margin, int spacing) {
        if (specHeight != MeasureSpec.UNSPECIFIED) return;

        WrappingLayoutSubclass layout =
                WrappingLayoutSubclass.create(
                        mContext, leftTopPadding, bottomRightPadding, spacing);
        layout.addTestViews(margin);

        // Special-case: width set to 200 pixels exactly, height unbounded.
        layout.layoutAtSize(
                200 + 2 * margin,
                300,
                MeasureSpec.EXACTLY,
                specHeight,
                leftTopPadding,
                bottomRightPadding);
        layout.validateCalculatedSize(
                200,
                300,
                THREE_ROWS,
                ONE_COL,
                margin,
                leftTopPadding,
                bottomRightPadding,
                spacing,
                MeasureSpec.EXACTLY,
                specHeight);
        layout.validateView(
                VIEW_A_ID,
                FIRST_COL,
                FIRST_ROW,
                0,
                0,
                VIEW_A_WIDTH,
                VIEW_A_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_B_ID,
                FIRST_COL,
                SECOND_ROW,
                0,
                90,
                VIEW_B_WIDTH,
                VIEW_B_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_C_ID,
                FIRST_COL,
                THIRD_ROW,
                0,
                190,
                VIEW_C_WIDTH,
                VIEW_C_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
    }

    public void testTwoOnFirstOneOnNextReversed(
            int specWidth,
            int specHeight,
            int leftTopPadding,
            int bottomRightPadding,
            int margin,
            int spacing) {
        WrappingLayoutSubclass layout =
                WrappingLayoutSubclass.create(
                        mContext, leftTopPadding, bottomRightPadding, spacing);
        layout.addTestViewsReverseOrder(margin);

        // Same test as testTwoOnFirstOneOnNext, but with first and last last view swapped.
        // Remember: Views are: c, b, a = { 200, 110 }, { 190, 100 }, { 180, 90 }.
        layout.layoutAtSize(
                400 + 2 * TWO_COLS * margin,
                400,
                specWidth,
                specHeight,
                leftTopPadding,
                bottomRightPadding);
        layout.validateCalculatedSize(
                390,
                200,
                TWO_ROWS,
                TWO_COLS,
                margin,
                leftTopPadding,
                bottomRightPadding,
                spacing,
                specWidth,
                specHeight);
        layout.validateView(
                VIEW_C_ID,
                FIRST_COL,
                FIRST_ROW,
                0,
                0,
                VIEW_C_WIDTH,
                VIEW_C_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_B_ID,
                SECOND_COL,
                FIRST_ROW,
                200,
                0,
                VIEW_B_WIDTH,
                VIEW_B_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_A_ID,
                FIRST_COL,
                SECOND_ROW,
                0,
                110,
                VIEW_A_WIDTH,
                VIEW_A_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
    }

    public void testOneOnFirstTwoOnNextReversed(
            int specWidth,
            int specHeight,
            int leftTopPadding,
            int bottomRightPadding,
            int margin,
            int spacing) {
        WrappingLayoutSubclass layout =
                WrappingLayoutSubclass.create(
                        mContext, leftTopPadding, bottomRightPadding, spacing);
        layout.addTestViewsReverseOrder(margin);

        // Test what happens if there is room for one view on the first row, and two on the
        // second.
        layout.layoutAtSize(
                370 + (2 * TWO_COLS * margin) + spacing,
                400,
                specWidth,
                specHeight,
                leftTopPadding,
                bottomRightPadding);
        // Remember: Views are: c, b, a = { 200, 110 }, { 190, 100 }, { 180, 90 }.
        layout.validateCalculatedSize(
                370,
                210,
                TWO_ROWS,
                TWO_COLS,
                margin,
                leftTopPadding,
                bottomRightPadding,
                spacing,
                specWidth,
                specHeight);
        layout.validateView(
                VIEW_C_ID,
                FIRST_COL,
                FIRST_ROW,
                0,
                0,
                VIEW_C_WIDTH,
                VIEW_C_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_B_ID,
                FIRST_COL,
                SECOND_ROW,
                0,
                110,
                VIEW_B_WIDTH,
                VIEW_B_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
        layout.validateView(
                VIEW_A_ID,
                SECOND_COL,
                SECOND_ROW,
                190,
                110,
                VIEW_A_WIDTH,
                VIEW_A_HEIGHT,
                margin,
                leftTopPadding,
                spacing);
    }

    @Test
    @SmallTest
    @UseMethodParameter(WrappingLayoutTestParams.class)
    public void testAllLTR(
            int measureSpec, int leftTopPadding, int bottomRightPadding, int margin, int spacing) {
        // Unbounded width doesn't make sense for a wrapping layout. Assume exact measurement so
        // test can still test unbounded height.
        int specWidth = measureSpec == MeasureSpec.UNSPECIFIED ? MeasureSpec.AT_MOST : measureSpec;
        int specHeight = measureSpec;

        testNoWrapping(specWidth, specHeight, leftTopPadding, bottomRightPadding, margin, spacing);
        testAllTruncated(
                specWidth, specHeight, leftTopPadding, bottomRightPadding, margin, spacing);
        testOnePerLineWithSpace(
                specWidth, specHeight, leftTopPadding, bottomRightPadding, margin, spacing);
        testTwoOnFirstOneOnNext(
                specWidth, specHeight, leftTopPadding, bottomRightPadding, margin, spacing);
        testUnbounded(specHeight, leftTopPadding, bottomRightPadding, margin, spacing);
        testTwoOnFirstOneOnNextReversed(
                specWidth, specHeight, leftTopPadding, bottomRightPadding, margin, spacing);
        testOneOnFirstTwoOnNextReversed(
                specWidth, specHeight, leftTopPadding, bottomRightPadding, margin, spacing);
    }

    public void testRtl(
            int specWidth,
            int width,
            int specHeight,
            int height,
            int leftTopPadding,
            int bottomRightPadding,
            int margin,
            int spacing,
            ViewExpectation expectationA,
            ViewExpectation expectationB,
            ViewExpectation expectationC) {
        WrappingLayoutSubclass layout =
                WrappingLayoutSubclass.create(
                        mContext, leftTopPadding, bottomRightPadding, spacing);
        layout.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        layout.addTestViews(margin);

        layout.layoutAtSize(width, height, specWidth, specHeight, 0, 0);
        layout.assertViewExpectations(expectationA, expectationB, expectationC);
    }

    @Test
    @SmallTest
    public void testWrappingLayoutRtl() {
        // Test case: Plenty of space, all views in one line.
        testRtl(
                MeasureSpec.EXACTLY,
                1000,
                MeasureSpec.EXACTLY,
                400,
                10,
                15,
                0,
                0,
                new ViewExpectation(805, 10, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(615, 10, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(415, 10, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                1000,
                MeasureSpec.EXACTLY,
                400,
                0,
                0,
                0,
                0,
                new ViewExpectation(820, 0, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(630, 0, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(430, 0, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                1000,
                MeasureSpec.EXACTLY,
                400,
                15,
                15,
                0,
                0,
                new ViewExpectation(805, 15, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(615, 15, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(415, 15, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                1000,
                MeasureSpec.EXACTLY,
                400,
                0,
                0,
                10,
                0,
                new ViewExpectation(810, 10, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(600, 10, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(380, 10, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                1000,
                MeasureSpec.EXACTLY,
                400,
                15,
                15,
                10,
                0,
                new ViewExpectation(795, 25, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(585, 25, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(365, 25, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                1000,
                MeasureSpec.EXACTLY,
                400,
                0,
                0,
                0,
                3,
                new ViewExpectation(820, 0, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(627, 0, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(424, 0, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                1000,
                MeasureSpec.EXACTLY,
                400,
                0,
                0,
                10,
                3,
                new ViewExpectation(810, 10, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(597, 10, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(374, 10, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                1000,
                MeasureSpec.EXACTLY,
                400,
                15,
                15,
                0,
                3,
                new ViewExpectation(805, 15, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(612, 15, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(409, 15, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                1000,
                MeasureSpec.EXACTLY,
                400,
                15,
                15,
                10,
                3,
                new ViewExpectation(795, 25, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(582, 25, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(359, 25, VIEW_C_WIDTH, VIEW_C_HEIGHT));

        // Test case: Exact match for two views in first row, one overflows to next line.
        testRtl(
                MeasureSpec.EXACTLY,
                370 + 10 + 15,
                MeasureSpec.EXACTLY,
                400,
                10,
                15,
                0,
                0,
                new ViewExpectation(200, 10, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(10, 10, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(180, 110, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                370,
                MeasureSpec.EXACTLY,
                400,
                0,
                0,
                0,
                0,
                new ViewExpectation(190, 0, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(0, 0, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(170, 100, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                370 + 15 + 15,
                MeasureSpec.EXACTLY,
                400,
                15,
                15,
                0,
                0,
                new ViewExpectation(205, 15, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(15, 15, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(185, 115, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                370 + 4 * 10,
                MeasureSpec.EXACTLY,
                400,
                0,
                0,
                10,
                0,
                new ViewExpectation(220, 10, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(10, 10, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(200, 130, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                370 + 4 * 10 + 15 + 15,
                MeasureSpec.EXACTLY,
                400,
                15,
                15,
                10,
                0,
                new ViewExpectation(235, 25, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(25, 25, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(215, 145, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                370 + 3,
                MeasureSpec.EXACTLY,
                400,
                0,
                0,
                0,
                3,
                new ViewExpectation(193, 0, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(0, 0, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(173, 103, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                370 + 4 * 10 + 3,
                MeasureSpec.EXACTLY,
                400,
                0,
                0,
                10,
                3,
                new ViewExpectation(223, 10, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(10, 10, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(203, 133, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                370 + 15 + 3 + 15,
                MeasureSpec.EXACTLY,
                400,
                15,
                15,
                0,
                3,
                new ViewExpectation(208, 15, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(15, 15, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(188, 118, VIEW_C_WIDTH, VIEW_C_HEIGHT));
        testRtl(
                MeasureSpec.EXACTLY,
                370 + 15 + 4 * 10 + 3 + 15,
                MeasureSpec.EXACTLY,
                400,
                15,
                15,
                10,
                3,
                new ViewExpectation(238, 25, VIEW_A_WIDTH, VIEW_A_HEIGHT),
                new ViewExpectation(25, 25, VIEW_B_WIDTH, VIEW_B_HEIGHT),
                new ViewExpectation(218, 148, VIEW_C_WIDTH, VIEW_C_HEIGHT));
    }
}
