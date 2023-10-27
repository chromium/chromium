// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link RecyclerViewSelectionController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RecyclerViewSelectionControllerUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock RecyclerView mRecyclerView;
    private @Mock LayoutManager mLayoutManager;
    private @Mock View mChildView1;
    private @Mock View mChildView2;
    private @Mock View mChildView3;
    RecyclerViewSelectionController mSelectionManager;

    @Before
    public void setUp() {
        when(mLayoutManager.getItemCount()).thenReturn(3);
        when(mLayoutManager.findViewByPosition(0)).thenReturn(mChildView1);
        when(mLayoutManager.findViewByPosition(1)).thenReturn(mChildView2);
        when(mLayoutManager.findViewByPosition(2)).thenReturn(mChildView3);

        mSelectionManager = new RecyclerViewSelectionController(mLayoutManager);
    }

    @Test
    public void selectNextItem_fromNone() {
        Assert.assertEquals(RecyclerView.NO_POSITION, mSelectionManager.getSelectedItemForTest());
        mSelectionManager.selectNextItem();
        Assert.assertEquals(0, mSelectionManager.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionManager.getSelectedView());
    }

    @Test
    public void selectNextItem_fromPrevious() {
        mSelectionManager.setSelectedItem(1, false);
        Assert.assertEquals(1, mSelectionManager.getSelectedItemForTest());
        Assert.assertEquals(mChildView2, mSelectionManager.getSelectedView());
        mSelectionManager.selectNextItem();
        Assert.assertEquals(2, mSelectionManager.getSelectedItemForTest());
        Assert.assertEquals(mChildView3, mSelectionManager.getSelectedView());
    }

    @Test
    public void selectNextItem_fromLast() {
        mSelectionManager.setSelectedItem(2, false);
        Assert.assertEquals(2, mSelectionManager.getSelectedItemForTest());
        Assert.assertEquals(mChildView3, mSelectionManager.getSelectedView());
        mSelectionManager.selectNextItem();
        Assert.assertEquals(2, mSelectionManager.getSelectedItemForTest());
        Assert.assertEquals(mChildView3, mSelectionManager.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromNone() {
        Assert.assertEquals(RecyclerView.NO_POSITION, mSelectionManager.getSelectedItemForTest());
        mSelectionManager.selectPreviousItem();
        // Jump to the last element on the list.
        Assert.assertEquals(2, mSelectionManager.getSelectedItemForTest());
        Assert.assertEquals(mChildView3, mSelectionManager.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromPrevious() {
        mSelectionManager.setSelectedItem(1, false);
        Assert.assertEquals(1, mSelectionManager.getSelectedItemForTest());
        Assert.assertEquals(mChildView2, mSelectionManager.getSelectedView());
        mSelectionManager.selectPreviousItem();
        Assert.assertEquals(0, mSelectionManager.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionManager.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromFirst() {
        mSelectionManager.setSelectedItem(0, false);
        Assert.assertEquals(0, mSelectionManager.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionManager.getSelectedView());
        mSelectionManager.selectPreviousItem();
        Assert.assertEquals(0, mSelectionManager.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionManager.getSelectedView());
    }

    @Test
    public void setSelectedItem_moveSelectionFromNone() {
        Assert.assertEquals(RecyclerView.NO_POSITION, mSelectionManager.getSelectedItemForTest());
        mSelectionManager.setSelectedItem(1, false);

        verify(mChildView1, times(0)).setSelected(anyBoolean());
        verify(mChildView3, times(0)).setSelected(anyBoolean());
        verify(mChildView2, times(1)).setSelected(true);
        verify(mChildView2, times(0)).setSelected(false);
    }

    @Test
    public void setSelectedItem_moveSelectionFromAnotherItem() {
        Assert.assertEquals(RecyclerView.NO_POSITION, mSelectionManager.getSelectedItemForTest());
        mSelectionManager.setSelectedItem(1, false);
        reset(mChildView2);

        mSelectionManager.setSelectedItem(2, false);
        Assert.assertEquals(2, mSelectionManager.getSelectedItemForTest());

        verify(mChildView1, times(0)).setSelected(anyBoolean());
        verify(mChildView2, times(0)).setSelected(true);
        verify(mChildView2, times(1)).setSelected(false);
        verify(mChildView3, times(1)).setSelected(true);
        verify(mChildView3, times(0)).setSelected(false);
    }

    @Test
    public void setSelectedItem_moveSelectionToNone() {
        Assert.assertEquals(RecyclerView.NO_POSITION, mSelectionManager.getSelectedItemForTest());
        mSelectionManager.setSelectedItem(1, false);
        reset(mChildView2);

        mSelectionManager.setSelectedItem(RecyclerView.NO_POSITION, false);
        Assert.assertEquals(RecyclerView.NO_POSITION, mSelectionManager.getSelectedItemForTest());

        verify(mChildView1, times(0)).setSelected(anyBoolean());
        verify(mChildView3, times(0)).setSelected(anyBoolean());
        verify(mChildView2, times(0)).setSelected(true);
        verify(mChildView2, times(1)).setSelected(false);
    }

    @Test
    public void setSelectedItem_moveFromSameIndexIsNoop() {
        Assert.assertEquals(RecyclerView.NO_POSITION, mSelectionManager.getSelectedItemForTest());
        mSelectionManager.setSelectedItem(1, false);
        Assert.assertEquals(1, mSelectionManager.getSelectedItemForTest());
        reset(mChildView2);

        mSelectionManager.setSelectedItem(1, false);
        Assert.assertEquals(1, mSelectionManager.getSelectedItemForTest());

        verify(mChildView1, times(0)).setSelected(anyBoolean());
        verify(mChildView2, times(0)).setSelected(anyBoolean());
        verify(mChildView3, times(0)).setSelected(anyBoolean());
    }

    @Test
    public void setSelectedItem_indexNegative() {
        mSelectionManager.setSelectedItem(1, false);
        Assert.assertEquals(1, mSelectionManager.getSelectedItemForTest());
        // This call should be rejected, leaving selected item as it was.
        // Note that (-1) is reserved value: RecyclerView.NO_POSITION.
        mSelectionManager.setSelectedItem(-2, false);
        Assert.assertEquals(1, mSelectionManager.getSelectedItemForTest());
    }

    @Test
    public void setSelectedItem_indexTooLarge() {
        mSelectionManager.setSelectedItem(1, false);
        Assert.assertEquals(1, mSelectionManager.getSelectedItemForTest());
        // This call should be rejected, leaving selected item as it was.
        mSelectionManager.setSelectedItem(3, false);
        Assert.assertEquals(1, mSelectionManager.getSelectedItemForTest());
    }

    @Test
    public void onChildViewAttached_viewIsReused() {
        // Simulates the case where View at position 0 is used as a View at position 3.
        when(mLayoutManager.getItemCount()).thenReturn(4);

        // Select View at position 0.
        mSelectionManager.setSelectedItem(0, false);
        verify(mChildView1, times(1)).setSelected(true);
        verifyNoMoreInteractions(mChildView1);
        verifyNoMoreInteractions(mChildView2);
        verifyNoMoreInteractions(mChildView3);
        reset(mChildView1);

        // Pretend that the view is out of screen.
        // This should not result in view selection being cleared.
        when(mLayoutManager.findViewByPosition(0)).thenReturn(null);
        mSelectionManager.onChildViewDetachedFromWindow(mChildView1);
        verify(mChildView1, times(1)).setSelected(false);
        verifyNoMoreInteractions(mChildView1);
        verifyNoMoreInteractions(mChildView2);
        verifyNoMoreInteractions(mChildView3);
        reset(mChildView1);

        // Pretend that the View 0 is now re-used as View 3.
        // We should see that the Selected state is cleared.
        when(mLayoutManager.findViewByPosition(3)).thenReturn(mChildView1);
        mSelectionManager.onChildViewAttachedToWindow(mChildView1);
        verifyNoMoreInteractions(mChildView1);
        verifyNoMoreInteractions(mChildView2);
        verifyNoMoreInteractions(mChildView3);

        // Finally, pretend that the view 0 is back on screen.
        // This happens in 2 steps:
        // - 1. the view is removed from last position
        when(mLayoutManager.findViewByPosition(3)).thenReturn(null);
        mSelectionManager.onChildViewDetachedFromWindow(mChildView1);
        // - 2. the view is inserted at first position.
        when(mLayoutManager.findViewByPosition(0)).thenReturn(mChildView1);
        mSelectionManager.onChildViewAttachedToWindow(mChildView1);
        // This will result in the setSelected(false) being called 2 times:
        // - once, when we signal the view is detached, and
        // - once, when we force re-set the selected view state.
        verify(mChildView1, times(2)).setSelected(false);
        verify(mChildView1, times(1)).setSelected(true);
        verifyNoMoreInteractions(mChildView1);
        verifyNoMoreInteractions(mChildView2);
        verifyNoMoreInteractions(mChildView3);
    }
}
