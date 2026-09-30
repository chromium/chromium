// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.GroupSeparatorType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests for {@link GroupSeparatorDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupSeparatorDecorationUnitTest {
    private static class TestRecyclerView extends RecyclerView {
        private final List<View> mChildren = new ArrayList<>();
        private final Map<View, ViewHolder> mViewHolders = new HashMap<>();

        TestRecyclerView(Context context) {
            super(context);
        }

        void registerViewHolder(View view, ViewHolder holder) {
            mViewHolders.put(view, holder);
        }

        @Override
        public void addView(View child) {
            mChildren.add(child);
        }

        @Override
        public int getChildCount() {
            return mChildren.size();
        }

        @Override
        public View getChildAt(int index) {
            return mChildren.get(index);
        }

        @Override
        public ViewHolder getChildViewHolder(View child) {
            return mViewHolders.get(child);
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private RecyclerView.State mState;
    @Mock private Canvas mCanvas;

    private TestRecyclerView mRecyclerView;
    private View mChildViewWithLineSeparator;
    private View mChildViewWithGapSeparator;
    private View mChildViewWithNoSeparator;

    private SimpleRecyclerViewAdapter.ViewHolder mLineSeparatorViewHolder;
    private SimpleRecyclerViewAdapter.ViewHolder mGapSeparatorViewHolder;
    private SimpleRecyclerViewAdapter.ViewHolder mNoSeparatorViewHolder;

    private PropertyModel mLineSeparatorModel;
    private PropertyModel mGapSeparatorModel;
    private PropertyModel mNoSeparatorModel;

    private Context mContext;
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

        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mDecoration = new GroupSeparatorDecoration(mContext);
        mRecyclerView = new TestRecyclerView(mContext);

        mChildViewWithLineSeparator = new View(mContext);
        mChildViewWithGapSeparator = new View(mContext);
        mChildViewWithNoSeparator = new View(mContext);

        Resources res = mContext.getResources();
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

        mRecyclerView.registerViewHolder(mChildViewWithLineSeparator, mLineSeparatorViewHolder);
        mRecyclerView.registerViewHolder(mChildViewWithGapSeparator, mGapSeparatorViewHolder);
        mRecyclerView.registerViewHolder(mChildViewWithNoSeparator, mNoSeparatorViewHolder);
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
        RecyclerView.LayoutParams lp =
                new RecyclerView.LayoutParams(
                        RecyclerView.LayoutParams.WRAP_CONTENT,
                        RecyclerView.LayoutParams.WRAP_CONTENT);
        lp.topMargin = 10;
        mChildViewWithLineSeparator.setLayoutParams(lp);
        mRecyclerView.addView(mChildViewWithLineSeparator);
        mChildViewWithLineSeparator.setTop(100);

        mRecyclerView.setPadding(10, 0, 20, 0);
        mRecyclerView.setRight(200);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        int expectedPadding =
                mContext.getResources()
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
        RecyclerView.LayoutParams lp =
                new RecyclerView.LayoutParams(
                        RecyclerView.LayoutParams.WRAP_CONTENT,
                        RecyclerView.LayoutParams.WRAP_CONTENT);
        lp.topMargin = 10;
        mChildViewWithGapSeparator.setLayoutParams(lp);
        mRecyclerView.addView(mChildViewWithGapSeparator);

        mRecyclerView.setPadding(10, 0, 20, 0);
        mRecyclerView.setRight(200);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        // Should not draw anything for gap
        verify(mCanvas, never())
                .drawRect(anyFloat(), anyFloat(), anyFloat(), anyFloat(), any(Paint.class));
    }

    @Test
    public void testOnDraw_noSeparator() {
        RecyclerView.LayoutParams lp =
                new RecyclerView.LayoutParams(
                        RecyclerView.LayoutParams.WRAP_CONTENT,
                        RecyclerView.LayoutParams.WRAP_CONTENT);
        lp.topMargin = 10;
        mChildViewWithNoSeparator.setLayoutParams(lp);
        mRecyclerView.addView(mChildViewWithNoSeparator);

        mRecyclerView.setPadding(10, 0, 20, 0);
        mRecyclerView.setRight(200);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        // Should not draw anything
        verify(mCanvas, never())
                .drawRect(anyFloat(), anyFloat(), anyFloat(), anyFloat(), any(Paint.class));
    }
}
