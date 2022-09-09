// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.hamcrest.Matchers.lessThanOrEqualTo;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.view.View.MeasureSpec;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;

/**
 * Tests for {@link BaseSuggestionView}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseSuggestionViewTest {
    // Used as a (fixed) width of a refine icon.
    private int mActionIconWidthPx;

    private BaseSuggestionViewForTest mView;
    private Activity mActivity;
    private View mDecoratedView;
    private View mContentView;

    @Mock
    private Runnable mOnFocusListener;

    // IMPORTANT: We need to extend the tested class here to support functionality currently
    // omitted by Robolectric, that is relevant to the tests below (layout direction change).
    //
    // TODO(https://github.com/robolectric/robolectric/issues/3910) Remove the class below once
    // the above issue is resolved and our robolectric version is rolled forward to the version
    // that supports layout direction changes.
    static class BaseSuggestionViewForTest extends BaseSuggestionView {
        private int mCurrentDirection = View.LAYOUT_DIRECTION_LTR;

        BaseSuggestionViewForTest(View childView) {
            super(childView);
        }

        @Override
        public void setLayoutDirection(int newDirection) {
            mCurrentDirection = newDirection;
        }

        @Override
        public int getLayoutDirection() {
            return mCurrentDirection;
        }

        /**
         * Test method to force layout update based on specified view dimensions.
         */
        void performLayoutForTest(int width) {
            onMeasure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                    MeasureSpec.UNSPECIFIED);

            // Note: height is computed by onMeasure call.
            final int height = getMeasuredHeight();
            onLayout(true, 0, 0, width, height);
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mContentView = new View(mActivity);
        mView = new BaseSuggestionViewForTest(mContentView);
        mView.setOnFocusViaSelectionListener(mOnFocusListener);

        mActionIconWidthPx = mActivity.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_action_icon_width);

        mDecoratedView = mView.getDecoratedSuggestionView();
    }

    /**
     * Perform the measure and layout pass on the BaseSuggestionView.
     * This method sets up the basic properties of the Suggestion container, specifies height of the
     * content view and executes the measure and layout pass.
     */
    private void executeLayoutTest(int containerWidth, int contentHeight, int layoutDirection) {
        mView.setLayoutDirection(layoutDirection);
        Assert.assertEquals(
                "layout direction not supported", layoutDirection, mView.getLayoutDirection());

        // Let ContentView drive the height of the Suggestion. The dummy view could shrink, so let's
        // prevent that from happening. We don't technically have any content, so we need to prevent
        // the view from shrinking, too.
        mContentView.setMinimumHeight(contentHeight);

        mView.performLayoutForTest(containerWidth);
    }

    /**
     * Confirm that specified view is positioned at specific coordinates.
     */
    private void verifyViewLayout(View v, int left, int top, int right, int bottom) {
        Assert.assertEquals("left view edge", left, v.getLeft());
        Assert.assertEquals("top view edge", top, v.getTop());
        Assert.assertEquals("right view edge", right, v.getRight());
        Assert.assertEquals("bottom view edge", bottom, v.getBottom());
        Assert.assertEquals("view width", right - left, v.getMeasuredWidth());
        Assert.assertThat("view height", v.getMeasuredHeight(), lessThanOrEqualTo(bottom - top));
    }

    @Test
    public void layout_LtrMultipleActionButtonsVisible() {
        final int useContentWidth = 320;
        final int paddingStart = 12;
        final int paddingEnd = 34;

        final int giveSuggestionWidth =
                useContentWidth + 3 * mActionIconWidthPx + paddingStart + paddingEnd;
        final int giveContentHeight = 15;

        final int expectedContentLeft = paddingStart;
        final int expectedContentRight = expectedContentLeft + useContentWidth;
        final int expectedRefine1Left = expectedContentRight;
        final int expectedRefine1Right = expectedRefine1Left + mActionIconWidthPx;
        final int expectedRefine2Left = expectedRefine1Right;
        final int expectedRefine2Right = expectedRefine2Left + mActionIconWidthPx;
        final int expectedRefine3Left = expectedRefine2Right;
        final int expectedRefine3Right = giveSuggestionWidth - paddingEnd;

        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        mView.setActionButtonsCount(3);
        final View actionButton1 = (View) mView.getActionButtons().get(0);
        final View actionButton2 = (View) mView.getActionButtons().get(1);
        final View actionButton3 = (View) mView.getActionButtons().get(2);

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);

        verifyViewLayout(
                actionButton1, expectedRefine1Left, 0, expectedRefine1Right, giveContentHeight);
        verifyViewLayout(
                actionButton2, expectedRefine2Left, 0, expectedRefine2Right, giveContentHeight);
        verifyViewLayout(
                actionButton3, expectedRefine3Left, 0, expectedRefine3Right, giveContentHeight);
        verifyViewLayout(
                mDecoratedView, expectedContentLeft, 0, expectedContentRight, giveContentHeight);
    }

    @Test
    public void layout_RtlMultipleActionButtonsVisible() {
        final int useContentWidth = 220;
        final int paddingStart = 13;
        final int paddingEnd = 57;

        final int giveSuggestionWidth =
                useContentWidth + 3 * mActionIconWidthPx + paddingStart + paddingEnd;
        final int giveContentHeight = 25;

        final int expectedRefine1Left = paddingEnd;
        final int expectedRefine1Right = expectedRefine1Left + mActionIconWidthPx;
        final int expectedRefine2Left = expectedRefine1Right;
        final int expectedRefine2Right = expectedRefine2Left + mActionIconWidthPx;
        final int expectedRefine3Left = expectedRefine2Right;
        final int expectedRefine3Right = expectedRefine3Left + mActionIconWidthPx;
        final int expectedContentLeft = expectedRefine3Right;
        final int expectedContentRight = giveSuggestionWidth - paddingStart;

        mView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        mView.setActionButtonsCount(3);
        // Note: reverse order, because we also want to show these buttons in reverse order.
        final View actionButton1 = (View) mView.getActionButtons().get(2);
        final View actionButton2 = (View) mView.getActionButtons().get(1);
        final View actionButton3 = (View) mView.getActionButtons().get(0);

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);

        verifyViewLayout(
                actionButton1, expectedRefine1Left, 0, expectedRefine1Right, giveContentHeight);
        verifyViewLayout(
                actionButton2, expectedRefine2Left, 0, expectedRefine2Right, giveContentHeight);
        verifyViewLayout(
                actionButton3, expectedRefine3Left, 0, expectedRefine3Right, giveContentHeight);
        verifyViewLayout(
                mDecoratedView, expectedContentLeft, 0, expectedContentRight, giveContentHeight);
    }

    @Test
    public void layout_LtrRefineVisible() {
        final int useContentWidth = 120;
        final int paddingStart = 12;
        final int paddingEnd = 34;

        // Expectations (edge to edge):
        //
        // +----------------+----+-+  ^
        // | CONTENT        |ACT1|#|  giveContentHeight
        // +----------------+----+-+  v
        //
        // <- giveSuggestionWidth ->
        //
        // where ACT is action button and # is final padding.

        final int giveSuggestionWidth =
                useContentWidth + mActionIconWidthPx + paddingStart + paddingEnd;
        final int giveContentHeight = 15;

        final int expectedContentLeft = paddingStart;
        final int expectedContentRight = expectedContentLeft + useContentWidth;
        final int expectedRefineLeft = expectedContentRight;
        final int expectedRefineRight = giveSuggestionWidth - paddingEnd;

        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        mView.setActionButtonsCount(1);
        final View actionButton = (View) mView.getActionButtons().get(0);

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);

        verifyViewLayout(
                actionButton, expectedRefineLeft, 0, expectedRefineRight, giveContentHeight);
        verifyViewLayout(
                mDecoratedView, expectedContentLeft, 0, expectedContentRight, giveContentHeight);
    }

    @Test
    public void layout_RtlRefineVisible() {
        final int useContentWidth = 120;
        final int paddingStart = 13;
        final int paddingEnd = 57;

        // Expectations (edge to edge):
        //
        // +----+----------------+-+  ^
        // |ACT1| CONTENT        |#|  giveContentHeight
        // +----+----------------+-+  v
        //
        // <- giveSuggestionWidth ->
        //
        // where ACT is action button and # is final padding.

        final int giveSuggestionWidth =
                useContentWidth + mActionIconWidthPx + paddingStart + paddingEnd;
        final int giveContentHeight = 25;

        final int expectedRefineLeft = paddingEnd;
        final int expectedRefineRight = expectedRefineLeft + mActionIconWidthPx;
        final int expectedContentLeft = expectedRefineRight;
        final int expectedContentRight = giveSuggestionWidth - paddingStart;

        mView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        mView.setActionButtonsCount(1);
        final View actionButton = (View) mView.getActionButtons().get(0);

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);

        verifyViewLayout(
                actionButton, expectedRefineLeft, 0, expectedRefineRight, giveContentHeight);
        verifyViewLayout(
                mDecoratedView, expectedContentLeft, 0, expectedContentRight, giveContentHeight);
    }

    @Test
    public void layout_LtrRefineInvisible() {
        // Expectations (edge to edge):
        //
        // +---------------------+-+  ^
        // |CONTENT              |#|  giveContentHeight
        // +---------------------+-+  v
        //
        // <- giveSuggestionWidth ->
        //
        // The reason for this is that we want content to align correctly with the end of the
        // omnibox field. Otherwise, content would end at the right screen edge.

        final int giveSuggestionWidth = 250;
        final int giveContentHeight = 15;
        final int paddingStart = 11;
        final int paddingEnd = 22;

        final int expectedContentLeft = paddingStart;
        final int expectedContentRight = giveSuggestionWidth - paddingEnd;

        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);
        verifyViewLayout(
                mDecoratedView, expectedContentLeft, 0, expectedContentRight, giveContentHeight);
    }

    @Test
    public void layout_RtlRefineInvisible() {
        // Expectations (edge to edge):
        //
        // +---------------------+-+  ^
        // |CONTENT              |#|  giveContentHeight
        // +---------------------+-+  v
        //
        // <- giveSuggestionWidth ->
        //
        // The reason for this is that we want content to align correctly with the end of the
        // omnibox field. Otherwise, content would end (RTL) at the left screen edge.

        final int giveSuggestionWidth = 250;
        final int giveContentHeight = 15;
        final int paddingStart = 57;
        final int paddingEnd = 31;

        final int expectedContentLeft = paddingEnd;
        final int expectedContentRight = giveSuggestionWidth - paddingStart;

        mView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);
        verifyViewLayout(
                mDecoratedView, expectedContentLeft, 0, expectedContentRight, giveContentHeight);
    }

    @Test
    public void setSelected_emitsOmniboxUpdateWhenSelected() {
        mView.setSelected(true);
        verify(mOnFocusListener, times(1)).run();
    }

    @Test
    public void setSelected_noOmniboxUpdateWhenDeselected() {
        mView.setSelected(false);
        verify(mOnFocusListener, never()).run();
    }
}
