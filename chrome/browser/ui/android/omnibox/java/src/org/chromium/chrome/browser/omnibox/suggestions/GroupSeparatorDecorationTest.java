// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Tests for {@link GroupSeparatorDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupSeparatorDecorationTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock private RecyclerView mRecyclerView;
    @Mock private View mChildViewWithSeparator;
    @Mock private View mChildViewWithNoSeparator;
    @Mock private RecyclerView.State mState;

    private SimpleRecyclerViewAdapter.ViewHolder mShowSeparatorViewHolder;
    private SimpleRecyclerViewAdapter.ViewHolder mNoSeparatorViewHolder;

    private PropertyModel mShowSeparatorModel;
    private PropertyModel mNoSeparatorModel;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private GroupSeparatorDecoration mDecoration;
    private int mExpectedHeight;

    @Before
    public void setUp() {
        mShowSeparatorModel =
                new PropertyModel.Builder(SuggestionCommonProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.SHOW_GROUP_SEPARATOR, true)
                        .build();
        mNoSeparatorModel =
                new PropertyModel.Builder(SuggestionCommonProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.SHOW_GROUP_SEPARATOR, false)
                        .build();

        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mDecoration = new GroupSeparatorDecoration(mActivity);

        Resources res = mActivity.getResources();
        mExpectedHeight =
                res.getDimensionPixelSize(R.dimen.divider_height)
                        + res.getDimensionPixelSize(
                                R.dimen.omnibox_suggestion_list_divider_line_padding);

        mShowSeparatorViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildViewWithSeparator, null);
        mNoSeparatorViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildViewWithNoSeparator, null);
        mShowSeparatorViewHolder.model = mShowSeparatorModel;
        mNoSeparatorViewHolder.model = mNoSeparatorModel;

        doReturn(mShowSeparatorViewHolder)
                .when(mRecyclerView)
                .getChildViewHolder(mChildViewWithSeparator);
        doReturn(mNoSeparatorViewHolder)
                .when(mRecyclerView)
                .getChildViewHolder(mChildViewWithNoSeparator);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    @SmallTest
    public void testGetItemOffsets_withSeparator() {
        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, mChildViewWithSeparator, mRecyclerView, mState);
        assertEquals(mExpectedHeight, outRect.top);
        assertEquals(0, outRect.bottom);
        assertEquals(0, outRect.left);
        assertEquals(0, outRect.right);
    }

    @Test
    @SmallTest
    public void testGetItemOffsets_noSeparator() {
        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, mChildViewWithNoSeparator, mRecyclerView, mState);
        assertEquals(0, outRect.top);
        assertEquals(0, outRect.bottom);
        assertEquals(0, outRect.left);
        assertEquals(0, outRect.right);
    }
}
