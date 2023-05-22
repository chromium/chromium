// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.Region.Op;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Tests for {@link SuggestionHorizontalDivider}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SuggestionHorizontalDividerTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock
    private RecyclerView mRecyclerView;
    @Mock
    private View mChildViewWithDivider;
    @Mock
    private View mChildViewWithNoDivider;
    @Mock
    private RecyclerView.State mState;
    @Mock
    private SimpleRecyclerViewAdapter.ViewHolder mShowDividerViewHolder;
    @Mock
    private SimpleRecyclerViewAdapter.ViewHolder mNoDividerViewHolder;
    @Mock
    private Canvas mCanvas;

    private PropertyModel mShowDividerModel =
            new PropertyModel.Builder(DropdownCommonProperties.ALL_KEYS)
                    .with(DropdownCommonProperties.SHOW_DIVIDER, true)
                    .build();
    private PropertyModel mNoDividerModel =
            new PropertyModel.Builder(DropdownCommonProperties.ALL_KEYS)
                    .with(DropdownCommonProperties.SHOW_DIVIDER, false)
                    .build();

    private Activity mActivity;
    private SuggestionHorizontalDivider mDecoration;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI);
        mDecoration = new SuggestionHorizontalDivider(mActivity);
        mShowDividerViewHolder.model = mShowDividerModel;
        mNoDividerViewHolder.model = mNoDividerModel;

        doReturn(mShowDividerViewHolder)
                .when(mRecyclerView)
                .getChildViewHolder(mChildViewWithDivider);
        doReturn(mNoDividerViewHolder)
                .when(mRecyclerView)
                .getChildViewHolder(mChildViewWithNoDivider);
        doReturn(2).when(mRecyclerView).getChildCount();
        doReturn(mChildViewWithDivider).when(mRecyclerView).getChildAt(0);
        doReturn(mChildViewWithNoDivider).when(mRecyclerView).getChildAt(1);
    }

    @Test
    @SmallTest
    public void testShouldDraw() {
        Assert.assertTrue(mDecoration.shouldDrawDivider(mChildViewWithDivider, mRecyclerView));
        Assert.assertFalse(mDecoration.shouldDrawDivider(mChildViewWithNoDivider, mRecyclerView));
    }

    @Test
    @SmallTest
    public void testDraw() {
        doAnswer((invocation -> {
            ((Rect) invocation.getArgument(1)).set(0, 0, 100, 30);
            return null;
        }))
                .when(mRecyclerView)
                .getDecoratedBoundsWithMargins(any(View.class), any(Rect.class));

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);
        verify(mCanvas).clipRect(0, 29, 100, 30, Op.DIFFERENCE);
    }
}
