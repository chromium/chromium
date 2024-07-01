// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

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
    private @Mock View mChildView4;
    private @Mock View mChildView5;
    RecyclerViewSelectionController mSelectionController;

    @Before
    public void setUp() {
        when(mLayoutManager.getItemCount()).thenReturn(5);
        when(mLayoutManager.findViewByPosition(0)).thenReturn(mChildView1);
        when(mLayoutManager.findViewByPosition(1)).thenReturn(mChildView2);
        when(mLayoutManager.findViewByPosition(2)).thenReturn(mChildView3);
        when(mLayoutManager.findViewByPosition(3)).thenReturn(mChildView4);
        when(mLayoutManager.findViewByPosition(4)).thenReturn(mChildView5);

        doReturn(true).when(mChildView1).isFocusable();
        doReturn(true).when(mChildView2).isFocusable();
        doReturn(true).when(mChildView3).isFocusable();
        doReturn(true).when(mChildView4).isFocusable();
        doReturn(true).when(mChildView5).isFocusable();

        mSelectionController = new RecyclerViewSelectionController(mLayoutManager);
    }

    @Test
    public void selectNextItem_fromNone() {
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        mSelectionController.selectNextItem();
        Assert.assertEquals(0, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_fromPrevious() {
        mSelectionController.setSelectedItem(1);
        Assert.assertEquals(1, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView2, mSelectionController.getSelectedView());
        verify(mLayoutManager).scrollToPosition(3);
        mSelectionController.selectNextItem();
        Assert.assertEquals(2, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView3, mSelectionController.getSelectedView());
        verify(mLayoutManager).scrollToPosition(4);
    }

    @Test
    public void selectNextItem_fromLast() {
        mSelectionController.setSelectedItem(4);
        verify(mLayoutManager).scrollToPosition(4);
        Assert.assertEquals(4, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView5, mSelectionController.getSelectedView());

        // Selecting next item should result in item being highlighted.
        Assert.assertTrue(mSelectionController.selectNextItem());
        Assert.assertEquals(4, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView5, mSelectionController.getSelectedView());

        // Permit cycling through to no selection.
        mSelectionController.setCycleThroughNoSelection(true);

        // Cycling should result in no item being selected.
        Assert.assertFalse(mSelectionController.selectNextItem());
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(null, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromNone() {
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        mSelectionController.selectPreviousItem();
        // Jump to the last element on the list.
        Assert.assertEquals(4, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView5, mSelectionController.getSelectedView());
        verify(mLayoutManager).scrollToPosition(4);
    }

    @Test
    public void selectPreviousItem_fromPrevious() {
        mSelectionController.setSelectedItem(1);
        verify(mLayoutManager).scrollToPosition(3);
        Assert.assertEquals(1, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView2, mSelectionController.getSelectedView());
        mSelectionController.selectPreviousItem();
        verify(mLayoutManager).scrollToPosition(0);
        Assert.assertEquals(0, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromFirst() {
        mSelectionController.setSelectedItem(0);
        Assert.assertEquals(0, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());

        // Selecting previous item should result in item being highlighted.
        Assert.assertTrue(mSelectionController.selectPreviousItem());
        Assert.assertEquals(0, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());

        // Permit cycling through no selection.
        mSelectionController.setCycleThroughNoSelection(true);
        // Cycling should result in no item being selected.
        Assert.assertFalse(mSelectionController.selectPreviousItem());
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(null, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_skipNonFocusableItems_noCycling() {
        mSelectionController.setSelectedItem(2);
        Assert.assertEquals(2, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView3, mSelectionController.getSelectedView());

        // View at position 1 is not focusable:
        doReturn(false).when(mChildView2).isFocusable();

        // Focus skips position 1.
        mSelectionController.selectPreviousItem();
        Assert.assertEquals(0, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_ignoreHeadNonFocusableViews() {
        mSelectionController.setSelectedItem(2);
        Assert.assertEquals(2, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView3, mSelectionController.getSelectedView());

        // Views at position 0 and 1 are not focusable:
        doReturn(false).when(mChildView1).isFocusable();
        doReturn(false).when(mChildView2).isFocusable();

        // Focus must not move.
        mSelectionController.selectPreviousItem();
        Assert.assertEquals(2, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView3, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_skipNonFocusableItems_withCycling() {
        mSelectionController.setCycleThroughNoSelection(true);
        mSelectionController.setSelectedItem(1);
        Assert.assertEquals(1, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView2, mSelectionController.getSelectedView());

        // View at position 0 is not focusable:
        doReturn(false).when(mChildView1).isFocusable();

        // We wrap around ignoring view at position 2.
        mSelectionController.selectPreviousItem();
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(null, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_noFocusableViews() {
        doReturn(false).when(mChildView1).isFocusable();
        doReturn(false).when(mChildView2).isFocusable();
        doReturn(false).when(mChildView3).isFocusable();
        doReturn(false).when(mChildView4).isFocusable();
        doReturn(false).when(mChildView5).isFocusable();

        mSelectionController.selectPreviousItem();
        verify(mLayoutManager).scrollToPosition(0);
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(null, mSelectionController.getSelectedView());

        // Permit cycling through.
        mSelectionController.setCycleThroughNoSelection(true);
        mSelectionController.selectPreviousItem();
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(null, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_skipNonFocusableItems_noCycling() {
        mSelectionController.setSelectedItem(0);
        Assert.assertEquals(0, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());

        // View at position 1 is not focusable:
        doReturn(false).when(mChildView2).isFocusable();

        // Focus skips position 1.
        mSelectionController.selectNextItem();
        Assert.assertEquals(2, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView3, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_ignoreTailNonFocusableViews() {
        mSelectionController.setSelectedItem(0);
        verify(mLayoutManager).scrollToPosition(2);
        Assert.assertEquals(0, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());

        // Views at positions 1-4 are not focusable:
        doReturn(false).when(mChildView2).isFocusable();
        doReturn(false).when(mChildView3).isFocusable();
        doReturn(false).when(mChildView4).isFocusable();
        doReturn(false).when(mChildView5).isFocusable();

        // Focus must not move.
        mSelectionController.selectNextItem();
        Assert.assertEquals(0, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_skipNonFocusableItems_withCycling() {
        mSelectionController.setCycleThroughNoSelection(true);
        mSelectionController.setSelectedItem(1);
        verify(mLayoutManager).scrollToPosition(3);
        Assert.assertEquals(1, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(mChildView2, mSelectionController.getSelectedView());

        // Views at positions 2-4 are not focusable:
        doReturn(false).when(mChildView3).isFocusable();
        doReturn(false).when(mChildView4).isFocusable();
        doReturn(false).when(mChildView5).isFocusable();

        // We wrap around ignoring view at position 2.
        mSelectionController.selectNextItem();
        verify(mLayoutManager).scrollToPosition(0);
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(null, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_noFocusableViews() {
        doReturn(false).when(mChildView1).isFocusable();
        doReturn(false).when(mChildView2).isFocusable();
        doReturn(false).when(mChildView3).isFocusable();
        doReturn(false).when(mChildView4).isFocusable();
        doReturn(false).when(mChildView5).isFocusable();

        mSelectionController.selectNextItem();
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(null, mSelectionController.getSelectedView());

        // Permit cycling through.
        mSelectionController.setCycleThroughNoSelection(true);
        mSelectionController.selectNextItem();
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        Assert.assertEquals(null, mSelectionController.getSelectedView());
    }

    @Test
    public void setSelectedItem_moveSelectionFromNone() {
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        mSelectionController.setSelectedItem(1);
        Assert.assertEquals(1, mSelectionController.getSelectedItemForTest());

        verify(mChildView2, times(1)).setSelected(true);
        verify(mChildView2, times(1)).setSelected(anyBoolean());
        verifyNoMoreInteractions(mChildView1, mChildView2, mChildView3);

        // Reset selection back to none.

        mSelectionController.resetSelection();
        verify(mChildView2, times(1)).setSelected(false);
        verify(mChildView2, times(2)).setSelected(anyBoolean());
        verifyNoMoreInteractions(mChildView1, mChildView2, mChildView3);

        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
    }

    @Test
    public void setSelectedItem_moveSelectionFromAnotherItem() {
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        mSelectionController.setSelectedItem(1);
        reset(mChildView2);

        mSelectionController.setSelectedItem(2);
        Assert.assertEquals(2, mSelectionController.getSelectedItemForTest());

        verify(mChildView1, times(0)).setSelected(anyBoolean());
        verify(mChildView2, times(0)).setSelected(true);
        verify(mChildView2, times(1)).setSelected(false);
        verify(mChildView3, times(1)).setSelected(true);
        verify(mChildView3, times(0)).setSelected(false);
    }

    @Test
    public void setSelectedItem_moveSelectionToNone() {
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());
        mSelectionController.setSelectedItem(1);
        reset(mChildView2);

        mSelectionController.setSelectedItem(RecyclerView.NO_POSITION);
        Assert.assertEquals(
                RecyclerView.NO_POSITION, mSelectionController.getSelectedItemForTest());

        verify(mChildView1, times(0)).setSelected(anyBoolean());
        verify(mChildView3, times(0)).setSelected(anyBoolean());
        verify(mChildView2, times(0)).setSelected(true);
        verify(mChildView2, times(1)).setSelected(false);
    }

    @Test
    public void setSelectedItem_indexNegative() {
        mSelectionController.setSelectedItem(1);
        Assert.assertEquals(1, mSelectionController.getSelectedItemForTest());
        // This call should be rejected, leaving selected item as it was.
        // Note that (-1) is reserved value: RecyclerView.NO_POSITION.
        mSelectionController.setSelectedItem(-2);
        Assert.assertEquals(1, mSelectionController.getSelectedItemForTest());
    }

    @Test
    public void setSelectedItem_indexTooLarge() {
        mSelectionController.setSelectedItem(1);
        Assert.assertEquals(1, mSelectionController.getSelectedItemForTest());
        // This call should be rejected, leaving selected item as it was.
        mSelectionController.setSelectedItem(30);
        Assert.assertEquals(1, mSelectionController.getSelectedItemForTest());
    }

    @Test
    public void onChildViewAttached_viewIsReused() {
        // Simulates the case where View at position 0 is used as a View at position 3.
        when(mLayoutManager.getItemCount()).thenReturn(4);

        // Select View at position 0.
        mSelectionController.setSelectedItem(0);
        verify(mChildView1, times(1)).setSelected(true);
        verifyNoMoreInteractions(mChildView1);
        verifyNoMoreInteractions(mChildView2);
        verifyNoMoreInteractions(mChildView3);
        reset(mChildView1);

        // Pretend that the view is out of screen.
        // This should not result in view selection being cleared.
        when(mLayoutManager.findViewByPosition(0)).thenReturn(null);
        mSelectionController.onChildViewDetachedFromWindow(mChildView1);
        verify(mChildView1, times(1)).setSelected(false);
        verifyNoMoreInteractions(mChildView1);
        verifyNoMoreInteractions(mChildView2);
        verifyNoMoreInteractions(mChildView3);
        reset(mChildView1);

        // Pretend that the View 0 is now re-used as View 3.
        // We should see that the Selected state is cleared.
        when(mLayoutManager.findViewByPosition(3)).thenReturn(mChildView1);
        mSelectionController.onChildViewAttachedToWindow(mChildView1);
        verifyNoMoreInteractions(mChildView1);
        verifyNoMoreInteractions(mChildView2);
        verifyNoMoreInteractions(mChildView3);

        // Finally, pretend that the view 0 is back on screen.
        // This happens in 2 steps:
        // - 1. the view is removed from last position
        when(mLayoutManager.findViewByPosition(3)).thenReturn(null);
        mSelectionController.onChildViewDetachedFromWindow(mChildView1);
        // - 2. the view is inserted at first position.
        when(mLayoutManager.findViewByPosition(0)).thenReturn(mChildView1);
        mSelectionController.onChildViewAttachedToWindow(mChildView1);
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
