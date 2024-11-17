// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link SimpleVerticalLayoutView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SimpleVerticalLayoutViewTest {
    private static final int SMALL_VIEW_WIDTH = 50;
    private static final int LARGE_VIEW_WIDTH = 120;
    private static final int SMALL_VIEW_HEIGHT = 20;
    private static final int LARGE_VIEW_HEIGHT = 30;

    private SimpleVerticalLayoutViewForTest mView;
    private Activity mActivity;

    private View mSmallView;
    private View mLargeView;

    static class SimpleVerticalLayoutViewForTest extends SimpleVerticalLayoutView {
        SimpleVerticalLayoutViewForTest(Context context) {
            super(context);
        }

        /** Test method to force layout update based on specified view dimensions. */
        void performLayoutForTest(int width) {
            onMeasure(
                    MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

            // Note: height is computed by onMeasure call.
            final int height = getMeasuredHeight();
            onLayout(true, 0, 0, width, height);
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mView = new SimpleVerticalLayoutViewForTest(mActivity);

        mSmallView = new View(mActivity);
        mSmallView.setLayoutParams(new LayoutParams(SMALL_VIEW_WIDTH, SMALL_VIEW_HEIGHT));
        mSmallView.setMinimumHeight(SMALL_VIEW_HEIGHT);
        mLargeView = new View(mActivity);
        mLargeView.setLayoutParams(new LayoutParams(LARGE_VIEW_WIDTH, LARGE_VIEW_HEIGHT));
        mLargeView.setMinimumHeight(LARGE_VIEW_HEIGHT);
    }

    /**
     * Perform the measure and layout pass on the SimpleVerticalLayoutView. This method sets up the
     * basic properties of the Suggestion container, specifies height of the content view and
     * executes the measure and layout pass.
     */
    private void executeLayoutTest(int containerWidth) {
        mView.performLayoutForTest(containerWidth);
    }

    /** Confirm that specified view is positioned at specific coordinates. */
    private void verifyViewLayout(View v, int left, int top, int right, int bottom) {
        Assert.assertEquals("left view edge", left, v.getLeft());
        Assert.assertEquals("top view edge", top, v.getTop());
        Assert.assertEquals("right view edge", right, v.getRight());
        Assert.assertEquals("bottom view edge", bottom, v.getBottom());
        Assert.assertEquals("view height", bottom - top, v.getMeasuredHeight());
    }

    /** Verify that padding are respected during layout. */
    @Test
    public void layout_padding() {
        final int leftPaddingWidth = 17;
        final int rightPaddingWidth = 23;
        final int topPaddingHeight = 13;
        final int bottomPaddingHeight = 31;

        final int overallSuggestionWidth = leftPaddingWidth + LARGE_VIEW_WIDTH + rightPaddingWidth;
        final int largeSuggestionTop = topPaddingHeight + SMALL_VIEW_HEIGHT;

        mView.setPaddingRelative(
                leftPaddingWidth, topPaddingHeight, rightPaddingWidth, bottomPaddingHeight);

        mView.addView(mSmallView);
        mView.addView(mLargeView);

        executeLayoutTest(overallSuggestionWidth);

        verifyViewLayout(
                mSmallView,
                leftPaddingWidth,
                topPaddingHeight,
                leftPaddingWidth + LARGE_VIEW_WIDTH,
                topPaddingHeight + SMALL_VIEW_HEIGHT);
        verifyViewLayout(
                mLargeView,
                leftPaddingWidth,
                largeSuggestionTop,
                leftPaddingWidth + LARGE_VIEW_WIDTH,
                largeSuggestionTop + LARGE_VIEW_HEIGHT);
    }
}
