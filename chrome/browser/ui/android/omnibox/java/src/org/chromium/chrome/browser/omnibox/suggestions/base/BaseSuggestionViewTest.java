// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.lessThanOrEqualTo;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionLayout.LayoutParams;
import org.chromium.chrome.browser.omnibox.test.R;

/** Tests for {@link BaseSuggestionView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseSuggestionViewTest {
    private static final int CONTENT_VIEW_REPORTED_HEIGHT_PX = 10;
    // Used as a (fixed) width of a refine icon.
    private int mActionIconWidthPx;
    private int mSemicompactSuggestionViewHeight;
    private int mCompactSuggestionViewHeight;
    private int mDecorationIconWidthPx;
    private int mLargeDecorationIconWidthPx;

    private BaseSuggestionViewForTest mView;
    private Activity mActivity;
    private View mContentView;

    @Mock private Runnable mOnFocusListener;

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

        /** Test method to force layout update based on specified view dimensions. */
        void performLayoutForTest(int width) {
            onMeasure(
                    MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
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
        mContentView.setMinimumHeight(CONTENT_VIEW_REPORTED_HEIGHT_PX);
        mView = new BaseSuggestionViewForTest(mContentView);
        mView.setOnFocusViaSelectionListener(mOnFocusListener);

        mActionIconWidthPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_action_button_width);

        mSemicompactSuggestionViewHeight =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_content_height);

        mCompactSuggestionViewHeight =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_compact_content_height);

        mDecorationIconWidthPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_icon_area_size);
        mLargeDecorationIconWidthPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_icon_area_size_large);
    }

    /**
     * Perform the measure and layout pass on the BaseSuggestionView. This method sets up the basic
     * properties of the Suggestion container, specifies height of the content view and executes the
     * measure and layout pass.
     */
    private void executeLayoutTest(int containerWidth, int contentHeight, int layoutDirection) {
        mView.setLayoutDirection(layoutDirection);
        Assert.assertEquals(
                "layout direction not supported", layoutDirection, mView.getLayoutDirection());

        mView.performLayoutForTest(containerWidth);
    }

    /** Confirm that specified view is positioned at specific coordinates. */
    private void verifyViewLayout(View v, int left, int top, int right, int bottom) {
        Assert.assertEquals("left view edge", left, v.getLeft());
        Assert.assertEquals("top view edge", top, v.getTop());
        Assert.assertEquals("right view edge", right, v.getRight());
        // Assert.assertEquals("bottom view edge", bottom, v.getBottom());
        Assert.assertEquals("view width", right - left, v.getMeasuredWidth());
        assertThat("view height", v.getMeasuredHeight(), lessThanOrEqualTo(bottom - top));
    }

    @Test
    public void layout_LtrMultipleActionButtonsVisible() {
        final int useContentWidth = 320;
        final int paddingStart = 12;
        final int paddingEnd = 34;

        final int giveSuggestionWidth =
                mDecorationIconWidthPx
                        + useContentWidth
                        + 3 * mActionIconWidthPx
                        + paddingStart
                        + paddingEnd;
        final int giveContentHeight = 15;

        final int expectedContentLeft = paddingStart + mDecorationIconWidthPx;
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
                actionButton1,
                expectedRefine1Left,
                0,
                expectedRefine1Right,
                mSemicompactSuggestionViewHeight);
        verifyViewLayout(
                actionButton2,
                expectedRefine2Left,
                0,
                expectedRefine2Right,
                mSemicompactSuggestionViewHeight);
        verifyViewLayout(
                actionButton3,
                expectedRefine3Left,
                0,
                expectedRefine3Right,
                mSemicompactSuggestionViewHeight);
        verifyViewLayout(
                mContentView,
                expectedContentLeft,
                0,
                expectedContentRight,
                mSemicompactSuggestionViewHeight);
    }

    @Test
    public void layout_RtlMultipleActionButtonsVisible() {
        final int useContentWidth = 220;
        final int paddingStart = 13;
        final int paddingEnd = 57;

        final int giveSuggestionWidth =
                mDecorationIconWidthPx
                        + useContentWidth
                        + 3 * mActionIconWidthPx
                        + paddingStart
                        + paddingEnd;
        final int giveContentHeight = 25;

        final int expectedRefine1Left = paddingEnd;
        final int expectedRefine1Right = expectedRefine1Left + mActionIconWidthPx;
        final int expectedRefine2Left = expectedRefine1Right;
        final int expectedRefine2Right = expectedRefine2Left + mActionIconWidthPx;
        final int expectedRefine3Left = expectedRefine2Right;
        final int expectedRefine3Right = expectedRefine3Left + mActionIconWidthPx;
        final int expectedContentLeft = expectedRefine3Right;
        final int expectedContentRight =
                giveSuggestionWidth - paddingStart - mDecorationIconWidthPx;

        mView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        mView.setActionButtonsCount(3);
        // Note: reverse order, because we also want to show these buttons in reverse order.
        final View actionButton1 = (View) mView.getActionButtons().get(2);
        final View actionButton2 = (View) mView.getActionButtons().get(1);
        final View actionButton3 = (View) mView.getActionButtons().get(0);

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);

        verifyViewLayout(
                mContentView,
                expectedContentLeft,
                0,
                expectedContentRight,
                mSemicompactSuggestionViewHeight);
        verifyViewLayout(
                actionButton1,
                expectedRefine1Left,
                0,
                expectedRefine1Right,
                mSemicompactSuggestionViewHeight);
        verifyViewLayout(
                actionButton2,
                expectedRefine2Left,
                0,
                expectedRefine2Right,
                mSemicompactSuggestionViewHeight);
        verifyViewLayout(
                actionButton3,
                expectedRefine3Left,
                0,
                expectedRefine3Right,
                mSemicompactSuggestionViewHeight);
    }

    @Test
    public void layout_LtrRefineVisible() {
        final int useContentWidth = 120;
        final int paddingStart = 12;
        final int paddingEnd = 34;

        // Expectations (edge to edge):
        //
        // +---+--------------+----+
        // | % | CONTENT      |ACT1|
        // +---+--------------+----+
        // <- giveSuggestionWidth ->
        //
        // where ACT is action button and % is the suggestion icon.

        final int giveSuggestionWidth =
                mDecorationIconWidthPx
                        + useContentWidth
                        + mActionIconWidthPx
                        + paddingStart
                        + paddingEnd;
        final int giveContentHeight = 15;

        final int expectedContentLeft = paddingStart + mDecorationIconWidthPx;
        final int expectedContentRight = expectedContentLeft + useContentWidth;
        final int expectedRefineLeft = expectedContentRight;
        final int expectedRefineRight = giveSuggestionWidth - paddingEnd;

        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        mView.setActionButtonsCount(1);
        final View actionButton = (View) mView.getActionButtons().get(0);

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);

        verifyViewLayout(
                actionButton,
                expectedRefineLeft,
                0,
                expectedRefineRight,
                mSemicompactSuggestionViewHeight);
        verifyViewLayout(
                mContentView,
                expectedContentLeft,
                0,
                expectedContentRight,
                mSemicompactSuggestionViewHeight);
    }

    @Test
    public void layout_RtlRefineVisible() {
        final int useContentWidth = 120;
        final int paddingStart = 13;
        final int paddingEnd = 57;

        // Expectations (edge to edge):
        //
        // +----+--------------+---+
        // |ACT1| CONTENT      | % |
        // +----+--------------+---+
        // <- giveSuggestionWidth ->
        //
        // where ACT is action button and % is the suggestion icon.

        final int giveSuggestionWidth =
                mDecorationIconWidthPx
                        + useContentWidth
                        + mActionIconWidthPx
                        + paddingStart
                        + paddingEnd;
        final int giveContentHeight = 25;

        final int expectedRefineLeft = paddingEnd;
        final int expectedRefineRight = expectedRefineLeft + mActionIconWidthPx;
        final int expectedContentLeft = expectedRefineRight;
        final int expectedContentRight =
                giveSuggestionWidth - paddingStart - mDecorationIconWidthPx;

        mView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        mView.setActionButtonsCount(1);
        final View actionButton = (View) mView.getActionButtons().get(0);

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);

        verifyViewLayout(
                actionButton,
                expectedRefineLeft,
                0,
                expectedRefineRight,
                mSemicompactSuggestionViewHeight);
        verifyViewLayout(
                mContentView,
                expectedContentLeft,
                0,
                expectedContentRight,
                mSemicompactSuggestionViewHeight);
    }

    @Test
    public void layout_LtrRefineInvisible() {
        // Expectations (edge to edge):
        //
        // +---+-------------------+
        // | % |CONTENT            |
        // +---+-------------------+
        // <- giveSuggestionWidth ->
        //
        // The reason for this is that we want content to align correctly with the end of the
        // omnibox field. Otherwise, content would end at the right screen edge.

        final int giveSuggestionWidth = 250;
        final int giveContentHeight = 15;
        final int paddingStart = 11;
        final int paddingEnd = 22;

        final int expectedContentLeft = paddingStart + mDecorationIconWidthPx;
        final int expectedContentRight = giveSuggestionWidth - paddingEnd;

        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);
        verifyViewLayout(
                mContentView,
                expectedContentLeft,
                0,
                expectedContentRight,
                mSemicompactSuggestionViewHeight);
    }

    @Test
    public void layout_RtlRefineInvisible() {
        // Expectations (edge to edge):
        //
        // +-------------------+---+
        // |CONTENT            | % |
        // +-------------------+---+
        // <- giveSuggestionWidth ->
        //
        // The reason for this is that we want content to align correctly with the end of the
        // omnibox field. Otherwise, content would end (RTL) at the left screen edge.
        final int giveSuggestionWidth = 250;
        final int giveContentHeight = 15;
        final int paddingStart = 57;
        final int paddingEnd = 31;

        final int expectedContentLeft = paddingEnd;
        final int expectedContentRight =
                giveSuggestionWidth - paddingStart - mDecorationIconWidthPx;

        mView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);
        verifyViewLayout(
                mContentView,
                expectedContentLeft,
                0,
                expectedContentRight,
                mSemicompactSuggestionViewHeight);
    }

    @Test
    public void layout_LtrWithFooterAndActionButton() {
        // Expectations (edge to edge):
        //
        // +---+--------------+----+
        // | % | CONTENT      |ACT1|
        // +---+--------------+----+
        // | FOOTER                |
        // +-----------------------+
        // <- giveSuggestionWidth ->
        //
        // where ACT is action button and % is the suggestion icon.
        final int useContentWidth = 120;
        final int paddingStart = 12;
        final int paddingEnd = 34;

        final int giveSuggestionWidth =
                mDecorationIconWidthPx
                        + useContentWidth
                        + mActionIconWidthPx
                        + paddingStart
                        + paddingEnd;

        final int expectedContentLeft = paddingStart + mDecorationIconWidthPx;
        final int expectedContentRight = expectedContentLeft + useContentWidth;
        final int expectedRefineLeft = expectedContentRight;
        final int expectedRefineRight = giveSuggestionWidth - paddingEnd;

        final int footerHeightPx = 10;

        var footer = new View(mActivity);
        footer.setMinimumHeight(footerHeightPx);
        mView.addView(footer, LayoutParams.forViewType(LayoutParams.SuggestionViewType.FOOTER));
        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        mView.setActionButtonsCount(1);

        final View actionButton = (View) mView.getActionButtons().get(0);

        executeLayoutTest(giveSuggestionWidth, 0, View.LAYOUT_DIRECTION_LTR);

        verifyViewLayout(
                actionButton,
                expectedRefineLeft,
                0,
                expectedRefineRight,
                mCompactSuggestionViewHeight);
        verifyViewLayout(
                mContentView,
                expectedContentLeft,
                0,
                expectedContentRight,
                mCompactSuggestionViewHeight);
        verifyViewLayout(
                footer,
                paddingStart,
                mCompactSuggestionViewHeight,
                giveSuggestionWidth - paddingEnd,
                mCompactSuggestionViewHeight + footerHeightPx);
    }

    @Test
    public void layout_RtlWithFooterAndActionButton() {
        final int useContentWidth = 120;
        final int paddingStart = 13;
        final int paddingEnd = 57;

        // Expectations (edge to edge):
        //
        // +----+--------------+---+
        // |ACT1| CONTENT      | % |
        // +----+--------------+---+
        // | FOOTER                |
        // +-----------------------+
        // <- giveSuggestionWidth ->
        //
        // where ACT is action button and % is the suggestion icon.

        final int giveSuggestionWidth =
                mDecorationIconWidthPx
                        + useContentWidth
                        + mActionIconWidthPx
                        + paddingStart
                        + paddingEnd;
        final int giveContentHeight = 25;

        final int expectedRefineLeft = paddingEnd;
        final int expectedRefineRight = expectedRefineLeft + mActionIconWidthPx;
        final int expectedContentLeft = expectedRefineRight;
        final int expectedContentRight =
                giveSuggestionWidth - paddingStart - mDecorationIconWidthPx;

        final int footerHeightPx = 10;

        var footer = new View(mActivity);
        footer.setMinimumHeight(footerHeightPx);
        mView.addView(footer, LayoutParams.forViewType(LayoutParams.SuggestionViewType.FOOTER));
        mView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);
        mView.setActionButtonsCount(1);
        final View actionButton = (View) mView.getActionButtons().get(0);

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);

        verifyViewLayout(
                actionButton,
                expectedRefineLeft,
                0,
                expectedRefineRight,
                mCompactSuggestionViewHeight);
        verifyViewLayout(
                mContentView,
                expectedContentLeft,
                0,
                expectedContentRight,
                mCompactSuggestionViewHeight);
        // Note: in RTL layouts, paddingEnd is equivalent to left-side padding.
        // and paddingStart is equivalent to right-side padding.
        verifyViewLayout(
                footer,
                paddingEnd,
                mCompactSuggestionViewHeight,
                giveSuggestionWidth - paddingStart,
                mCompactSuggestionViewHeight + footerHeightPx);
    }

    @Test(expected = AssertionError.class)
    public void layout_missingContentView() {
        // Make sure there's no content views.
        mView.removeView(mContentView);
        executeLayoutTest(100, 10, View.LAYOUT_DIRECTION_LTR);
    }

    @Test(expected = AssertionError.class)
    public void layout_multipleContentViews() {
        var content = new View(mActivity);
        mView.addView(content, LayoutParams.forViewType(LayoutParams.SuggestionViewType.CONTENT));
        executeLayoutTest(100, 10, View.LAYOUT_DIRECTION_LTR);
    }

    @Test(expected = AssertionError.class)
    public void layout_hiddenContentViews() {
        mContentView.setVisibility(View.GONE);
        executeLayoutTest(100, 10, View.LAYOUT_DIRECTION_LTR);
    }

    @Test
    public void layout_minimumHeightWithNoFooterIsSemicompact() {
        mView.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        executeLayoutTest(100, 10, View.LAYOUT_DIRECTION_LTR);
        Assert.assertEquals(mSemicompactSuggestionViewHeight, mView.getMeasuredHeight());
    }

    @Test
    public void layout_minimumHeightWithFooterIsCompact() {
        var content = new View(mActivity);
        mView.addView(content, LayoutParams.forViewType(LayoutParams.SuggestionViewType.FOOTER));
        mView.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        executeLayoutTest(100, 10, View.LAYOUT_DIRECTION_LTR);
        Assert.assertEquals(mCompactSuggestionViewHeight, mView.getMeasuredHeight());
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

    @Test
    public void layout_dimensions() {
        Assert.assertEquals(mDecorationIconWidthPx, mView.mDecorationIconWidthPx);
        Assert.assertEquals(mSemicompactSuggestionViewHeight, mView.mContentHeightPx);
        Assert.assertEquals(mCompactSuggestionViewHeight, mView.mCompactContentHeightPx);
    }

    @Test
    public void layout_LtrLargeDecoration() {
        // Expectations (edge to edge):
        //
        // +---+-------------------+
        // | %%% |CONTENT          |
        // +---+-------------------+
        // <- giveSuggestionWidth ->
        //

        final int giveSuggestionWidth = 250;
        final int giveContentHeight = 15;
        final int paddingStart = 11;

        mView.setPaddingRelative(paddingStart, 0, 0, 0);
        mView.setUseLargeDecorationIcon(true);
        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);
        verifyViewLayout(
                mView.getChildAt(0),
                paddingStart + mLargeDecorationIconWidthPx / 2,
                mSemicompactSuggestionViewHeight / 2,
                paddingStart + mLargeDecorationIconWidthPx / 2,
                mSemicompactSuggestionViewHeight);

        mView.decorationIcon.getLayoutParams().width = 66;
        mView.decorationIcon.getLayoutParams().height = ViewGroup.LayoutParams.WRAP_CONTENT;
        mView.setUseLargeDecorationIcon(false);
        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);
        // Calling setUseLargeDecorationIcon should preserve its layout params' width and height.
        // Updating the width and height for a larger intrinsic image size is the responsibility of
        // BaseSuggestionViewBinder#updateSuggestionIcon.
        Assert.assertEquals(66, mView.decorationIcon.getLayoutParams().width);
        Assert.assertEquals(
                ViewGroup.LayoutParams.WRAP_CONTENT, mView.decorationIcon.getLayoutParams().height);
    }
}
