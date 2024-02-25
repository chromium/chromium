// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link SimpleVerticalLayoutView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SpacingRecyclerViewItemDecorationUnitTest {
    private static final int LEAD_IN_SPACE = 10;
    private static final int ELEMENT_SPACE = 17;
    private static final int ITEM_FIRST = 0;
    private static final int ITEM_MIDDLE = 1;
    private static final int ITEM_LAST = 2;
    private static final int ITEM_COUNT = ITEM_LAST + 1;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock RecyclerView mRecyclerView;
    private @Mock RecyclerView.Adapter mAdapter;
    private @Mock View mChildView;
    private SpacingRecyclerViewItemDecoration mDecoration;
    private Rect mOffsets;

    @Before
    public void setUp() {
        mDecoration = new SpacingRecyclerViewItemDecoration(LEAD_IN_SPACE, ELEMENT_SPACE);
        doReturn(mAdapter).when(mRecyclerView).getAdapter();
        doReturn(ITEM_COUNT).when(mAdapter).getItemCount();
        mOffsets = new Rect();
    }

    @Test
    public void notifyViewSizeChanged_neverUpdates() {
        assertFalse(mDecoration.notifyViewSizeChanged(true, 0, 0));
        assertFalse(mDecoration.notifyViewSizeChanged(false, 10, 10));
        assertFalse(mDecoration.notifyViewSizeChanged(true, 20, 20));
    }

    @Test
    public void testSpacing_firstElementLTR() {
        doReturn(ITEM_FIRST).when(mRecyclerView).getChildAdapterPosition(mChildView);
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mRecyclerView).getLayoutDirection();
        mDecoration.getItemOffsets(mOffsets, mChildView, mRecyclerView, /* state= */ null);
        assertEquals(LEAD_IN_SPACE, mOffsets.left);
        assertEquals(ELEMENT_SPACE / 2, mOffsets.right);
        assertEquals(0, mOffsets.top);
        assertEquals(0, mOffsets.bottom);
    }

    @Test
    public void testSpacing_firstElementRTL() {
        doReturn(ITEM_FIRST).when(mRecyclerView).getChildAdapterPosition(mChildView);
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mRecyclerView).getLayoutDirection();
        mDecoration.getItemOffsets(mOffsets, mChildView, mRecyclerView, /* state= */ null);
        assertEquals(ELEMENT_SPACE / 2, mOffsets.left);
        assertEquals(LEAD_IN_SPACE, mOffsets.right);
        assertEquals(0, mOffsets.top);
        assertEquals(0, mOffsets.bottom);
    }

    @Test
    public void testSpacing_lastElementLTR() {
        doReturn(ITEM_LAST).when(mRecyclerView).getChildAdapterPosition(mChildView);
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mRecyclerView).getLayoutDirection();
        mDecoration.getItemOffsets(mOffsets, mChildView, mRecyclerView, /* state= */ null);
        assertEquals(ELEMENT_SPACE / 2, mOffsets.left);
        assertEquals(LEAD_IN_SPACE, mOffsets.right);
        assertEquals(0, mOffsets.top);
        assertEquals(0, mOffsets.bottom);
    }

    @Test
    public void testSpacing_lastElementRTL() {
        doReturn(ITEM_LAST).when(mRecyclerView).getChildAdapterPosition(mChildView);
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mRecyclerView).getLayoutDirection();
        mDecoration.getItemOffsets(mOffsets, mChildView, mRecyclerView, /* state= */ null);
        assertEquals(LEAD_IN_SPACE, mOffsets.left);
        assertEquals(ELEMENT_SPACE / 2, mOffsets.right);
        assertEquals(0, mOffsets.top);
        assertEquals(0, mOffsets.bottom);
    }

    @Test
    public void testSpacing_middleElement() {
        doReturn(ITEM_MIDDLE).when(mRecyclerView).getChildAdapterPosition(mChildView);
        mDecoration.getItemOffsets(mOffsets, mChildView, mRecyclerView, /* state= */ null);
        assertEquals(ELEMENT_SPACE / 2, mOffsets.left);
        assertEquals(ELEMENT_SPACE / 2, mOffsets.right);
        assertEquals(0, mOffsets.top);
        assertEquals(0, mOffsets.bottom);
    }

    @Test
    public void setElementSpace_noUpdate() {
        assertFalse(mDecoration.setElementSpace(ELEMENT_SPACE));
        mDecoration.getItemOffsets(mOffsets, mChildView, mRecyclerView, /* state= */ null);
        assertEquals(LEAD_IN_SPACE, mOffsets.left);
        assertEquals(ELEMENT_SPACE / 2, mOffsets.right);
        assertEquals(0, mOffsets.top);
        assertEquals(0, mOffsets.bottom);
        assertEquals(ELEMENT_SPACE, mDecoration.getElementSpace());
    }

    @Test
    public void setElementSpace_changeTriggersInvalidation() {
        assertTrue(mDecoration.setElementSpace(2 * ELEMENT_SPACE));
        mDecoration.getItemOffsets(mOffsets, mChildView, mRecyclerView, /* state= */ null);
        assertEquals(LEAD_IN_SPACE, mOffsets.left);
        assertEquals(ELEMENT_SPACE, mOffsets.right);
        assertEquals(0, mOffsets.top);
        assertEquals(0, mOffsets.bottom);
        assertEquals(2 * ELEMENT_SPACE, mDecoration.getElementSpace());
    }
}
