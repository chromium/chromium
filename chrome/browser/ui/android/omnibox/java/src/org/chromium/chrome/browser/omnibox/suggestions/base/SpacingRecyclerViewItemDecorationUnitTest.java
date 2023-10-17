// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;

import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link SimpleVerticalLayoutView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SpacingRecyclerViewItemDecorationUnitTest {
    private static final int LEAD_IN_SPACE = 10;
    private static final int ELEMENT_SPACE = 17;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock RecyclerView mRecyclerView;
    private @Mock View mChildView;
    private SpacingRecyclerViewItemDecoration mDecoration;
    private Rect mOffsets;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mDecoration = new SpacingRecyclerViewItemDecoration(LEAD_IN_SPACE, ELEMENT_SPACE);
        mOffsets = new Rect();
    }

    @Test
    public void testSpacing_firstElementLTR() {
        doReturn(0).when(mRecyclerView).getChildAdapterPosition(mChildView);
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mRecyclerView).getLayoutDirection();
        mDecoration.getItemOffsets(mOffsets, mChildView, mRecyclerView, /* state= */ null);
        assertEquals(LEAD_IN_SPACE, mOffsets.left);
        assertEquals(ELEMENT_SPACE, mOffsets.right);
        assertEquals(0, mOffsets.top);
        assertEquals(0, mOffsets.bottom);
    }

    @Test
    public void testSpacing_firstElementRTL() {
        doReturn(0).when(mRecyclerView).getChildAdapterPosition(mChildView);
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mRecyclerView).getLayoutDirection();
        mDecoration.getItemOffsets(mOffsets, mChildView, mRecyclerView, /* state= */ null);
        assertEquals(ELEMENT_SPACE, mOffsets.left);
        assertEquals(LEAD_IN_SPACE, mOffsets.right);
        assertEquals(0, mOffsets.top);
        assertEquals(0, mOffsets.bottom);
    }

    @Test
    public void testSpacing_nonFirstElement() {
        doReturn(1).when(mRecyclerView).getChildAdapterPosition(mChildView);
        mDecoration.getItemOffsets(mOffsets, mChildView, mRecyclerView, /* state= */ null);
        assertEquals(ELEMENT_SPACE, mOffsets.left);
        assertEquals(ELEMENT_SPACE, mOffsets.right);
        assertEquals(0, mOffsets.top);
        assertEquals(0, mOffsets.bottom);
    }
}
