// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.Space;

import androidx.test.InstrumentationRegistry;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.widget.DualControlLayout.ButtonType;
import org.chromium.components.browser_ui.widget.DualControlLayout.DualControlLayoutAlignment;
import org.chromium.components.browser_ui.widget.test.R;

/** Tests for DualControlLayout. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DualControlLayoutTest {
    private static final int PRIMARY_HEIGHT = 16;
    private static final int SECONDARY_HEIGHT = 8;
    private static final int STACKED_MARGIN = 4;
    private static final int INFOBAR_WIDTH = 3200;

    private static final int PADDING_LEFT = 8;
    private static final int PADDING_TOP = 16;
    private static final int PADDING_RIGHT = 32;
    private static final int PADDING_BOTTOM = 64;

    private int mTinyControlWidth;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mTinyControlWidth = INFOBAR_WIDTH / 4;
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAlignSideBySide() {
        runLayoutTest(DualControlLayoutAlignment.START, false, false, false);
        runLayoutTest(DualControlLayoutAlignment.START, false, true, false);
        runLayoutTest(DualControlLayoutAlignment.START, true, false, false);
        runLayoutTest(DualControlLayoutAlignment.START, true, true, false);

        runLayoutTest(DualControlLayoutAlignment.APART, false, false, false);
        runLayoutTest(DualControlLayoutAlignment.APART, false, true, false);
        runLayoutTest(DualControlLayoutAlignment.APART, true, false, false);
        runLayoutTest(DualControlLayoutAlignment.APART, true, true, false);

        runLayoutTest(DualControlLayoutAlignment.END, false, false, false);
        runLayoutTest(DualControlLayoutAlignment.END, false, true, false);
        runLayoutTest(DualControlLayoutAlignment.END, true, false, false);
        runLayoutTest(DualControlLayoutAlignment.END, true, true, false);

        // Test the padding.
        runLayoutTest(DualControlLayoutAlignment.START, false, false, true);
        runLayoutTest(DualControlLayoutAlignment.START, false, true, true);
        runLayoutTest(DualControlLayoutAlignment.START, true, false, true);
        runLayoutTest(DualControlLayoutAlignment.START, true, true, true);

        runLayoutTest(DualControlLayoutAlignment.APART, false, false, true);
        runLayoutTest(DualControlLayoutAlignment.APART, false, true, true);
        runLayoutTest(DualControlLayoutAlignment.APART, true, false, true);
        runLayoutTest(DualControlLayoutAlignment.APART, true, true, true);

        runLayoutTest(DualControlLayoutAlignment.END, false, false, true);
        runLayoutTest(DualControlLayoutAlignment.END, false, true, true);
        runLayoutTest(DualControlLayoutAlignment.END, true, false, true);
        runLayoutTest(DualControlLayoutAlignment.END, true, true, true);
    }

    /** Lays out two controls that fit on the same line. */
    private void runLayoutTest(
            int alignment, boolean isRtl, boolean addSecondView, boolean addPadding) {
        DualControlLayout layout = new DualControlLayout(mContext, null);
        if (addPadding) layout.setPadding(PADDING_LEFT, PADDING_TOP, PADDING_RIGHT, PADDING_BOTTOM);
        layout.setAlignment(alignment);
        layout.setLayoutDirection(isRtl ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);
        layout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        View primary = new Space(mContext);
        primary.setMinimumWidth(mTinyControlWidth);
        primary.setMinimumHeight(PRIMARY_HEIGHT);
        layout.addView(primary);

        View secondary = null;
        if (addSecondView) {
            secondary = new Space(mContext);
            secondary.setMinimumWidth(mTinyControlWidth);
            secondary.setMinimumHeight(SECONDARY_HEIGHT);
            layout.addView(secondary);
        }

        // Trigger the measurement & layout algorithms.
        int parentWidthSpec = MeasureSpec.makeMeasureSpec(INFOBAR_WIDTH, MeasureSpec.EXACTLY);
        int parentHeightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        layout.measure(parentWidthSpec, parentHeightSpec);
        layout.layout(0, 0, layout.getMeasuredWidth(), layout.getMeasuredHeight());

        // Confirm that the primary View is in the correct place.
        if ((isRtl && alignment == DualControlLayoutAlignment.START)
                || (!isRtl
                        && (alignment == DualControlLayoutAlignment.APART
                                || alignment
                                        == DualControlLayout.DualControlLayoutAlignment.END))) {
            int expectedRight = INFOBAR_WIDTH - (addPadding ? PADDING_RIGHT : 0);
            Assert.assertEquals(
                    "Primary should be on the right.", expectedRight, primary.getRight());
        } else {
            int expectedLeft = addPadding ? PADDING_LEFT : 0;
            Assert.assertEquals("Primary should be on the left.", expectedLeft, primary.getLeft());
        }
        int expectedTop = addPadding ? PADDING_TOP : 0;
        int expectedBottom = expectedTop + PRIMARY_HEIGHT;
        Assert.assertEquals("Primary top in wrong location", expectedTop, primary.getTop());
        Assert.assertEquals(
                "Primary bottom in wrong location", expectedBottom, primary.getBottom());
        Assert.assertEquals(mTinyControlWidth, primary.getMeasuredWidth());
        Assert.assertEquals(PRIMARY_HEIGHT, primary.getMeasuredHeight());
        Assert.assertNotEquals(primary.getLeft(), primary.getRight());

        // Confirm that the secondary View is in the correct place.
        if (secondary != null) {
            Assert.assertEquals(mTinyControlWidth, secondary.getMeasuredWidth());
            Assert.assertEquals(SECONDARY_HEIGHT, secondary.getMeasuredHeight());
            Assert.assertNotEquals(secondary.getLeft(), secondary.getRight());
            if (alignment == DualControlLayoutAlignment.START) {
                if (isRtl) {
                    // Secondary View is immediately to the left of the parent.
                    Assert.assertTrue(secondary.getRight() < primary.getLeft());
                    Assert.assertNotEquals(0, secondary.getLeft());
                } else {
                    // Secondary View is immediately to the right of the parent.
                    Assert.assertTrue(primary.getRight() < secondary.getLeft());
                    Assert.assertNotEquals(INFOBAR_WIDTH, secondary.getRight());
                }
            } else if (alignment == DualControlLayoutAlignment.APART) {
                if (isRtl) {
                    // Secondary View is on the far right.
                    Assert.assertTrue(primary.getRight() < secondary.getLeft());
                    int expectedRight = INFOBAR_WIDTH - (addPadding ? PADDING_RIGHT : 0);
                    Assert.assertEquals(expectedRight, secondary.getRight());
                } else {
                    // Secondary View is on the far left.
                    Assert.assertTrue(secondary.getRight() < primary.getLeft());
                    int expectedLeft = addPadding ? PADDING_LEFT : 0;
                    Assert.assertEquals(expectedLeft, secondary.getLeft());
                }
            } else {
                Assert.assertEquals(DualControlLayoutAlignment.END, alignment);
                if (isRtl) {
                    // Secondary View is immediately to the right of the parent.
                    Assert.assertTrue(primary.getRight() < secondary.getLeft());
                    Assert.assertNotEquals(INFOBAR_WIDTH, secondary.getRight());
                } else {
                    // Secondary View is immediately to the left of the parent.
                    Assert.assertTrue(secondary.getRight() < primary.getLeft());
                    Assert.assertNotEquals(0, secondary.getLeft());
                }
            }

            // Confirm that the secondary View is centered with respect to the first.
            int primaryCenter = (primary.getTop() + primary.getBottom()) / 2;
            int secondaryCenter = (secondary.getTop() + secondary.getBottom()) / 2;
            Assert.assertEquals(primaryCenter, secondaryCenter);
        }

        int expectedLayoutHeight =
                primary.getMeasuredHeight() + (addPadding ? PADDING_TOP + PADDING_BOTTOM : 0);
        Assert.assertEquals(expectedLayoutHeight, layout.getMeasuredHeight());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testStacked() {
        runStackedLayoutTest(DualControlLayoutAlignment.START, false, false);
        runStackedLayoutTest(DualControlLayoutAlignment.START, true, false);
        runStackedLayoutTest(DualControlLayoutAlignment.APART, false, false);
        runStackedLayoutTest(DualControlLayoutAlignment.APART, true, false);
        runStackedLayoutTest(DualControlLayoutAlignment.END, false, false);
        runStackedLayoutTest(DualControlLayoutAlignment.END, true, false);

        // Test the padding.
        runStackedLayoutTest(DualControlLayoutAlignment.START, false, true);
        runStackedLayoutTest(DualControlLayoutAlignment.START, true, true);
        runStackedLayoutTest(DualControlLayoutAlignment.APART, false, true);
        runStackedLayoutTest(DualControlLayoutAlignment.APART, true, true);
        runStackedLayoutTest(DualControlLayoutAlignment.END, false, true);
        runStackedLayoutTest(DualControlLayoutAlignment.END, true, true);
    }

    /** Runs a test where the controls don't fit on the same line. */
    private void runStackedLayoutTest(int alignment, boolean isRtl, boolean addPadding) {
        DualControlLayout layout = new DualControlLayout(mContext, null);
        if (addPadding) layout.setPadding(PADDING_LEFT, PADDING_TOP, PADDING_RIGHT, PADDING_BOTTOM);
        layout.setAlignment(alignment);
        layout.setStackedMargin(STACKED_MARGIN);
        layout.setLayoutDirection(isRtl ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);
        layout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        View primary = new Space(mContext);
        primary.setMinimumWidth(mTinyControlWidth);
        primary.setMinimumHeight(PRIMARY_HEIGHT);
        layout.addView(primary);

        View secondary = new Space(mContext);
        secondary.setMinimumWidth(INFOBAR_WIDTH);
        secondary.setMinimumHeight(SECONDARY_HEIGHT);
        layout.addView(secondary);

        // Trigger the measurement & layout algorithms.
        int parentWidthSpec = MeasureSpec.makeMeasureSpec(INFOBAR_WIDTH, MeasureSpec.AT_MOST);
        int parentHeightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        layout.measure(parentWidthSpec, parentHeightSpec);
        layout.layout(0, 0, layout.getMeasuredWidth(), layout.getMeasuredHeight());

        Assert.assertEquals(addPadding ? PADDING_LEFT : 0, primary.getLeft());
        Assert.assertEquals(addPadding ? PADDING_LEFT : 0, secondary.getLeft());

        Assert.assertEquals(INFOBAR_WIDTH - (addPadding ? PADDING_RIGHT : 0), primary.getRight());
        Assert.assertEquals(INFOBAR_WIDTH - (addPadding ? PADDING_RIGHT : 0), secondary.getRight());

        int expectedControlWidth = INFOBAR_WIDTH - (addPadding ? PADDING_LEFT + PADDING_RIGHT : 0);
        Assert.assertEquals(expectedControlWidth, primary.getMeasuredWidth());
        Assert.assertEquals(expectedControlWidth, secondary.getMeasuredWidth());
        Assert.assertEquals(INFOBAR_WIDTH, layout.getMeasuredWidth());

        int expectedPrimaryTop = addPadding ? PADDING_TOP : 0;
        int expectedPrimaryBottom = expectedPrimaryTop + primary.getHeight();
        int expectedSecondaryTop = expectedPrimaryBottom + STACKED_MARGIN;
        int expectedSecondaryBottom = expectedSecondaryTop + secondary.getHeight();
        int expectedLayoutHeight = expectedSecondaryBottom + (addPadding ? PADDING_BOTTOM : 0);
        Assert.assertEquals(expectedPrimaryTop, primary.getTop());
        Assert.assertEquals(expectedPrimaryBottom, primary.getBottom());
        Assert.assertEquals(expectedSecondaryTop, secondary.getTop());
        Assert.assertEquals(expectedSecondaryBottom, secondary.getBottom());
        Assert.assertEquals(expectedLayoutHeight, layout.getMeasuredHeight());
        Assert.assertNotEquals(layout.getMeasuredHeight(), primary.getMeasuredHeight());
    }

    /**
     * Runs a test against an inflated DualControlLayout that sets all of its values. Re-uses the
     * AutofillEditor's buttons XML layout because we have no support for test-only layout files.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testInflation() {
        // Check that the basic DualControlLayout has nothing going on.
        DualControlLayout layout = new DualControlLayout(mContext, null);
        Assert.assertEquals(DualControlLayoutAlignment.START, layout.getAlignment());
        Assert.assertEquals(0, layout.getStackedMargin());
        Assert.assertNull(layout.findViewById(R.id.button_primary));
        Assert.assertNull(layout.findViewById(R.id.button_secondary));

        float dpToPx = mContext.getResources().getDisplayMetrics().density;
        // Inflate a DualControlLayout that has all of the attributes set and confirm they're used
        // correctly.
        FrameLayout containerView = new FrameLayout(mContext);
        LayoutInflater.from(mContext)
                .inflate(R.layout.dual_control_test_layout, containerView, true);
        DualControlLayout inflatedLayout = containerView.findViewById(R.id.button_bar);
        Assert.assertEquals(DualControlLayoutAlignment.END, inflatedLayout.getAlignment());
        Assert.assertEquals(
                "Incorrect stacked margin. Should be 24dp",
                24 * dpToPx,
                inflatedLayout.getStackedMargin(),
                0.f);

        Button primaryButton = inflatedLayout.findViewById(R.id.button_primary);
        Assert.assertNotNull(primaryButton);
        Assert.assertEquals("Done", primaryButton.getText());

        Button secondaryButton = inflatedLayout.findViewById(R.id.button_secondary);
        Assert.assertNotNull(secondaryButton);
        Assert.assertEquals("Cancel", secondaryButton.getText());
    }

    /** Runs a test that checks whether the primary and secondary buttons can be replaced. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testReplaceButtons() {
        // Inflate a DualControlLayout that has all of the attributes set and confirm they're used
        // correctly.
        FrameLayout containerView = new FrameLayout(mContext);
        LayoutInflater.from(mContext)
                .inflate(R.layout.dual_control_test_layout, containerView, true);
        DualControlLayout inflatedLayout = containerView.findViewById(R.id.button_bar);

        inflatedLayout.removeAllViews();

        Button primaryButton = inflatedLayout.findViewById(R.id.button_primary);
        Assert.assertNull(primaryButton);
        Button secondaryButton = inflatedLayout.findViewById(R.id.button_secondary);
        Assert.assertNull(secondaryButton);

        inflatedLayout.addView(
                DualControlLayout.createButtonForLayout(
                        mContext, ButtonType.PRIMARY_FILLED, "Done", null));
        inflatedLayout.addView(
                DualControlLayout.createButtonForLayout(
                        mContext, ButtonType.SECONDARY_TEXT, "Cancel", null));

        Button newPrimaryButton = inflatedLayout.findViewById(R.id.button_primary);
        Assert.assertNotNull(newPrimaryButton);
        Assert.assertEquals("Done", newPrimaryButton.getText());

        Button newSecondaryButton = inflatedLayout.findViewById(R.id.button_secondary);
        Assert.assertNotNull(newSecondaryButton);
        Assert.assertEquals("Cancel", newSecondaryButton.getText());
    }
}
