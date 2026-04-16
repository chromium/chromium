// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Paint;
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
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Tests for {@link HeaderDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HeaderDecorationTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock private RecyclerView mRecyclerView;
    @Mock private View mChildViewWithHeader;
    @Mock private View mChildViewWithNoHeader;
    @Mock private RecyclerView.State mState;
    @Mock private Canvas mCanvas;

    private SimpleRecyclerViewAdapter.ViewHolder mShowHeaderViewHolder;
    private SimpleRecyclerViewAdapter.ViewHolder mNoHeaderViewHolder;

    private static final String HEADER_TEXT = "Test Header";

    private PropertyModel mShowHeaderModel;
    private PropertyModel mNoHeaderModel;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private HeaderDecoration mDecoration;
    private int mExpectedHeight;

    @Before
    public void setUp() {
        mShowHeaderModel =
                new PropertyModel.Builder(SuggestionCommonProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.HEADER_TITLE, HEADER_TEXT)
                        .with(
                                SuggestionCommonProperties.COLOR_SCHEME,
                                BrandedColorScheme.APP_DEFAULT)
                        .build();
        mNoHeaderModel =
                new PropertyModel.Builder(SuggestionCommonProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.HEADER_TITLE, null)
                        .build();

        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mDecoration = new HeaderDecoration(mActivity);

        mExpectedHeight =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height);

        mShowHeaderViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildViewWithHeader, null);
        mNoHeaderViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildViewWithNoHeader, null);
        mShowHeaderViewHolder.model = mShowHeaderModel;
        mNoHeaderViewHolder.model = mNoHeaderModel;

        doReturn(mShowHeaderViewHolder)
                .when(mRecyclerView)
                .getChildViewHolder(mChildViewWithHeader);
        doReturn(mNoHeaderViewHolder)
                .when(mRecyclerView)
                .getChildViewHolder(mChildViewWithNoHeader);

        doReturn(2).when(mRecyclerView).getChildCount();
        doReturn(mChildViewWithHeader).when(mRecyclerView).getChildAt(0);
        doReturn(mChildViewWithNoHeader).when(mRecyclerView).getChildAt(1);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    @SmallTest
    public void testGetItemOffsets_withHeader() {
        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, mChildViewWithHeader, mRecyclerView, mState);
        assertEquals(mExpectedHeight, outRect.top);
        assertEquals(0, outRect.bottom);
        assertEquals(0, outRect.left);
        assertEquals(0, outRect.right);
    }

    @Test
    @SmallTest
    public void testGetItemOffsets_noHeader() {
        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, mChildViewWithNoHeader, mRecyclerView, mState);
        assertEquals(0, outRect.top);
        assertEquals(0, outRect.bottom);
        assertEquals(0, outRect.left);
        assertEquals(0, outRect.right);
    }

    @Test
    @SmallTest
    public void testDraw_withHeader() {
        doReturn(0).when(mChildViewWithHeader).getTop();
        doReturn(0.0f).when(mChildViewWithHeader).getTranslationY();
        doReturn(100).when(mRecyclerView).getWidth();
        doReturn(0).when(mRecyclerView).getPaddingLeft();
        doReturn(0).when(mRecyclerView).getPaddingRight();
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mRecyclerView).getLayoutDirection();

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);
        verify(mCanvas).drawText(eq(HEADER_TEXT), anyFloat(), anyFloat(), any(Paint.class));
    }

    @Test
    @SmallTest
    public void testDraw_noHeader() {
        doReturn(1).when(mRecyclerView).getChildCount();
        doReturn(mChildViewWithNoHeader).when(mRecyclerView).getChildAt(0);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);
        verifyNoInteractions(mCanvas);
    }
}
