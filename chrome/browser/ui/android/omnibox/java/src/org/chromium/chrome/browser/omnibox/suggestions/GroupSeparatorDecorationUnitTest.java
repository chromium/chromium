// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.GroupSeparatorType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Tests for {@link GroupSeparatorDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupSeparatorDecorationUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private RecyclerView mRecyclerView;
    @Mock private View mChildViewWithLineSeparator;
    @Mock private View mChildViewWithGapSeparator;
    @Mock private View mChildViewWithNoSeparator;
    @Mock private RecyclerView.State mState;
    @Mock private Canvas mCanvas;

    private SimpleRecyclerViewAdapter.ViewHolder mLineSeparatorViewHolder;
    private SimpleRecyclerViewAdapter.ViewHolder mGapSeparatorViewHolder;
    private SimpleRecyclerViewAdapter.ViewHolder mNoSeparatorViewHolder;

    private PropertyModel mLineSeparatorModel;
    private PropertyModel mGapSeparatorModel;
    private PropertyModel mNoSeparatorModel;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private GroupSeparatorDecoration mDecoration;
    private int mExpectedHeight;

    @Before
    public void setUp() {
        mLineSeparatorModel =
                new PropertyModel.Builder(SuggestionCommonProperties.ALL_KEYS)
                        .with(
                                SuggestionCommonProperties.GROUP_SEPARATOR_TYPE,
                                GroupSeparatorType.LINE)
                        .build();
        mGapSeparatorModel =
                new PropertyModel.Builder(SuggestionCommonProperties.ALL_KEYS)
                        .with(
                                SuggestionCommonProperties.GROUP_SEPARATOR_TYPE,
                                GroupSeparatorType.GAP)
                        .build();
        mNoSeparatorModel =
                new PropertyModel.Builder(SuggestionCommonProperties.ALL_KEYS)
                        .with(
                                SuggestionCommonProperties.GROUP_SEPARATOR_TYPE,
                                GroupSeparatorType.NONE)
                        .build();

        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mDecoration = new GroupSeparatorDecoration(mActivity);

        Resources res = mActivity.getResources();
        mExpectedHeight =
                res.getDimensionPixelSize(R.dimen.divider_height)
                        + res.getDimensionPixelSize(
                                R.dimen.omnibox_suggestion_list_divider_line_vertical_padding);

        mLineSeparatorViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildViewWithLineSeparator, null);
        mGapSeparatorViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildViewWithGapSeparator, null);
        mNoSeparatorViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildViewWithNoSeparator, null);
        mLineSeparatorViewHolder.model = mLineSeparatorModel;
        mGapSeparatorViewHolder.model = mGapSeparatorModel;
        mNoSeparatorViewHolder.model = mNoSeparatorModel;

        lenient()
                .doReturn(mLineSeparatorViewHolder)
                .when(mRecyclerView)
                .getChildViewHolder(mChildViewWithLineSeparator);
        lenient()
                .doReturn(mGapSeparatorViewHolder)
                .when(mRecyclerView)
                .getChildViewHolder(mChildViewWithGapSeparator);
        lenient()
                .doReturn(mNoSeparatorViewHolder)
                .when(mRecyclerView)
                .getChildViewHolder(mChildViewWithNoSeparator);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    public void testGetItemOffsets_withLineSeparator() {
        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, mChildViewWithLineSeparator, mRecyclerView, mState);
        assertEquals(mExpectedHeight, outRect.top);
        assertEquals(0, outRect.bottom);
        assertEquals(0, outRect.left);
        assertEquals(0, outRect.right);
    }

    @Test
    public void testGetItemOffsets_withGapSeparator() {
        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, mChildViewWithGapSeparator, mRecyclerView, mState);
        assertEquals(mExpectedHeight, outRect.top);
        assertEquals(0, outRect.bottom);
        assertEquals(0, outRect.left);
        assertEquals(0, outRect.right);
    }

    @Test
    public void testGetItemOffsets_noSeparator() {
        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, mChildViewWithNoSeparator, mRecyclerView, mState);
        assertEquals(0, outRect.top);
        assertEquals(0, outRect.bottom);
        assertEquals(0, outRect.left);
        assertEquals(0, outRect.right);
    }

    @Test
    public void testOnDraw_withLineSeparator() {
        doReturn(1).when(mRecyclerView).getChildCount();
        doReturn(mChildViewWithLineSeparator).when(mRecyclerView).getChildAt(0);
        RecyclerView.LayoutParams lp =
                new RecyclerView.LayoutParams(
                        RecyclerView.LayoutParams.WRAP_CONTENT,
                        RecyclerView.LayoutParams.WRAP_CONTENT);
        lp.topMargin = 10;
        doReturn(lp).when(mChildViewWithLineSeparator).getLayoutParams();
        doReturn(100).when(mChildViewWithLineSeparator).getTop();

        doReturn(10).when(mRecyclerView).getPaddingLeft();
        doReturn(200).when(mRecyclerView).getWidth();
        doReturn(20).when(mRecyclerView).getPaddingRight();

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        int expectedPadding =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.omnibox_suggestion_list_divider_line_horizontal_padding);
        int expectedLeft = 10 + expectedPadding;
        int expectedRight = 200 - 20 - expectedPadding;
        int expectedCenterY = 90 - mExpectedHeight / 2;

        verify(mCanvas)
                .drawRect(
                        eq((float) expectedLeft),
                        eq((float) expectedCenterY),
                        eq((float) expectedRight),
                        eq((float) (expectedCenterY + 1)),
                        any(Paint.class));
    }

    @Test
    public void testOnDraw_withGapSeparator() {
        doReturn(1).when(mRecyclerView).getChildCount();
        doReturn(mChildViewWithGapSeparator).when(mRecyclerView).getChildAt(0);
        RecyclerView.LayoutParams lp =
                new RecyclerView.LayoutParams(
                        RecyclerView.LayoutParams.WRAP_CONTENT,
                        RecyclerView.LayoutParams.WRAP_CONTENT);
        lp.topMargin = 10;

        doReturn(10).when(mRecyclerView).getPaddingLeft();
        doReturn(200).when(mRecyclerView).getWidth();
        doReturn(20).when(mRecyclerView).getPaddingRight();

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        // Should not draw anything for gap
        verify(mCanvas, never())
                .drawRect(anyFloat(), anyFloat(), anyFloat(), anyFloat(), any(Paint.class));
    }

    @Test
    public void testOnDraw_noSeparator() {
        doReturn(1).when(mRecyclerView).getChildCount();
        doReturn(mChildViewWithNoSeparator).when(mRecyclerView).getChildAt(0);
        RecyclerView.LayoutParams lp =
                new RecyclerView.LayoutParams(
                        RecyclerView.LayoutParams.WRAP_CONTENT,
                        RecyclerView.LayoutParams.WRAP_CONTENT);
        lp.topMargin = 10;

        doReturn(10).when(mRecyclerView).getPaddingLeft();
        doReturn(200).when(mRecyclerView).getWidth();
        doReturn(20).when(mRecyclerView).getPaddingRight();

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        // Should not draw anything
        verify(mCanvas, never())
                .drawRect(anyFloat(), anyFloat(), anyFloat(), anyFloat(), any(Paint.class));
    }
}
