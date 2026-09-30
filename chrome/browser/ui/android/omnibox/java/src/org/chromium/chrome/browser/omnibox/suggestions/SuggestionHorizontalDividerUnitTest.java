// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Region.Op;
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
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests for {@link SuggestionHorizontalDivider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionHorizontalDividerUnitTest {
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
    private View mChildViewWithDivider;
    private View mChildViewWithNoDivider;
    private SimpleRecyclerViewAdapter.ViewHolder mShowDividerViewHolder;
    private SimpleRecyclerViewAdapter.ViewHolder mNoDividerViewHolder;

    private final PropertyModel mShowDividerModel =
            new PropertyModel.Builder(SuggestionCommonProperties.ALL_KEYS)
                    .with(SuggestionCommonProperties.SHOW_DIVIDER, true)
                    .build();
    private final PropertyModel mNoDividerModel =
            new PropertyModel.Builder(SuggestionCommonProperties.ALL_KEYS)
                    .with(SuggestionCommonProperties.SHOW_DIVIDER, false)
                    .build();

    private Activity mActivity;
    private SuggestionHorizontalDivider mDecoration;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mDecoration = new SuggestionHorizontalDivider(mActivity);
        mRecyclerView = new TestRecyclerView(mActivity);
        mChildViewWithDivider = new View(mActivity);
        mChildViewWithNoDivider = new View(mActivity);
        mShowDividerViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildViewWithDivider, null);
        mNoDividerViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildViewWithNoDivider, null);
        mShowDividerViewHolder.model = mShowDividerModel;
        mNoDividerViewHolder.model = mNoDividerModel;

        mRecyclerView.registerViewHolder(mChildViewWithDivider, mShowDividerViewHolder);
        mRecyclerView.registerViewHolder(mChildViewWithNoDivider, mNoDividerViewHolder);
        mRecyclerView.addView(mChildViewWithDivider);
        mRecyclerView.addView(mChildViewWithNoDivider);
    }

    @Test
    public void testShouldDraw() {
        assertTrue(mDecoration.shouldDrawDivider(mChildViewWithDivider, mRecyclerView));
        assertFalse(mDecoration.shouldDrawDivider(mChildViewWithNoDivider, mRecyclerView));
    }

    @Test
    public void testDraw() {
        mChildViewWithDivider.layout(8, 10, 8 + 92, 10 + 30);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);
        verify(mCanvas).clipRect(8, 40 - 1, 100, 40, Op.DIFFERENCE);
    }
}
