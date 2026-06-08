// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link RecyclerViewSelectionController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RecyclerViewSelectionControllerUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock LayoutManager mLayoutManager;
    private @Mock View mChildView1;
    private @Mock View mChildView2;
    private @Mock View mChildView3;
    private @Mock View mChildView4;
    private @Mock View mChildView5;
    private @Mock Callback<Boolean> mVirtualCallback;
    RecyclerViewSelectionController mSelectionController;
    RecyclerViewSelectionController mSelectionControllerWithSentinel;

    @Before
    public void setUp() {
        lenient().when(mLayoutManager.getItemCount()).thenReturn(5);
        lenient().when(mLayoutManager.findViewByPosition(0)).thenReturn(mChildView1);
        lenient().when(mLayoutManager.findViewByPosition(1)).thenReturn(mChildView2);
        lenient().when(mLayoutManager.findViewByPosition(2)).thenReturn(mChildView3);
        lenient().when(mLayoutManager.findViewByPosition(3)).thenReturn(mChildView4);
        lenient().when(mLayoutManager.findViewByPosition(4)).thenReturn(mChildView5);

        lenient().doReturn(true).when(mChildView1).isFocusable();
        lenient().doReturn(true).when(mChildView2).isFocusable();
        lenient().doReturn(true).when(mChildView3).isFocusable();
        lenient().doReturn(true).when(mChildView4).isFocusable();
        lenient().doReturn(true).when(mChildView5).isFocusable();

        mSelectionController =
                new RecyclerViewSelectionController(
                        mLayoutManager, RecyclerViewSelectionController.Mode.SATURATING);
        mSelectionControllerWithSentinel =
                new RecyclerViewSelectionController(
                        mLayoutManager,
                        RecyclerViewSelectionController.Mode.SATURATING_WITH_SENTINEL);

        // Saturating controller will initialize selection, impacting tests. Reset this right away.
        clearInvocations(mChildView1);
    }

    @Test
    public void selectNextItem_fromNone() {
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
        mSelectionController.selectNextItem();
        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());
        assertEquals(mChildView2, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_fromNone_withSentinel() {
        assertTrue(mSelectionControllerWithSentinel.isParkedAtSentinel());
        mSelectionControllerWithSentinel.selectNextItem();
        assertEquals(Integer.valueOf(0), mSelectionControllerWithSentinel.getPosition());
        assertEquals(mChildView1, mSelectionControllerWithSentinel.getSelectedView());
    }

    @Test
    public void selectNextItem_fromPrevious() {
        mSelectionController.setPosition(1);
        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());
        assertEquals(mChildView2, mSelectionController.getSelectedView());
        verify(mLayoutManager).scrollToPosition(3);

        mSelectionController.selectNextItem();
        assertEquals(Integer.valueOf(2), mSelectionController.getPosition());
        assertEquals(mChildView3, mSelectionController.getSelectedView());
        verify(mLayoutManager).scrollToPosition(4);
    }

    @Test
    public void selectNextItem_fromLast() {
        mSelectionController.setPosition(4);
        verify(mLayoutManager).scrollToPosition(4);
        assertEquals(Integer.valueOf(4), mSelectionController.getPosition());
        assertEquals(mChildView5, mSelectionController.getSelectedView());

        // Selecting next item should result in item being highlighted.
        assertFalse(mSelectionController.selectNextItem());
        assertEquals(Integer.valueOf(4), mSelectionController.getPosition());
        assertEquals(mChildView5, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_fromLast_withSentinel() {
        mSelectionControllerWithSentinel.setPosition(4);
        verify(mLayoutManager).scrollToPosition(4);
        assertEquals(Integer.valueOf(4), mSelectionControllerWithSentinel.getPosition());
        assertEquals(mChildView5, mSelectionControllerWithSentinel.getSelectedView());

        // Selecting next item should result in leaving valid range.
        assertFalse(mSelectionControllerWithSentinel.selectNextItem());

        assertEquals(null, mSelectionControllerWithSentinel.getPosition());
        assertEquals(null, mSelectionControllerWithSentinel.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromNone_withSentinel() {
        assertEquals(null, mSelectionControllerWithSentinel.getPosition());
        mSelectionControllerWithSentinel.selectPreviousItem();
        assertEquals(null, mSelectionControllerWithSentinel.getPosition());
        assertEquals(null, mSelectionControllerWithSentinel.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromPrevious() {
        mSelectionController.setPosition(1);
        verify(mLayoutManager).scrollToPosition(3);
        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());
        assertEquals(mChildView2, mSelectionController.getSelectedView());

        mSelectionController.selectPreviousItem();
        verify(mLayoutManager).scrollToPosition(0);
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
        assertEquals(mChildView1, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromFirst() {
        mSelectionController.setPosition(0);
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
        assertEquals(mChildView1, mSelectionController.getSelectedView());

        // Selecting previous item should result in item being highlighted.
        assertFalse(mSelectionController.selectPreviousItem());
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
        assertEquals(mChildView1, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromFirst_withSentinel() {
        mSelectionControllerWithSentinel.setPosition(0);
        assertEquals(Integer.valueOf(0), mSelectionControllerWithSentinel.getPosition());
        assertEquals(mChildView1, mSelectionControllerWithSentinel.getSelectedView());
        assertFalse(mSelectionControllerWithSentinel.selectPreviousItem());
        assertEquals(null, mSelectionControllerWithSentinel.getPosition());
        assertEquals(null, mSelectionControllerWithSentinel.getSelectedView());
    }

    @Test
    public void selectPreviousItem_skipNonFocusableItems_noCycling() {
        mSelectionController.setPosition(2);
        assertEquals(Integer.valueOf(2), mSelectionController.getPosition());
        assertEquals(mChildView3, mSelectionController.getSelectedView());

        // View at position 1 is not focusable:
        doReturn(false).when(mChildView2).isFocusable();

        // Focus skips position 1.
        mSelectionController.selectPreviousItem();
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
        assertEquals(mChildView1, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_skipNonFocusableItems_noCycling() {
        mSelectionController.setPosition(0);
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
        assertEquals(mChildView1, mSelectionController.getSelectedView());

        // View at position 1 is not focusable:
        doReturn(false).when(mChildView2).isFocusable();

        // Focus skips position 1.
        mSelectionController.selectNextItem();
        assertEquals(Integer.valueOf(2), mSelectionController.getPosition());
        assertEquals(mChildView3, mSelectionController.getSelectedView());
    }

    @Test
    public void setSelectedItem_moveSelectionFromNone_withSentinel() {
        assertEquals(null, mSelectionControllerWithSentinel.getPosition());
        mSelectionControllerWithSentinel.setPosition(1);
        assertEquals(Integer.valueOf(1), mSelectionControllerWithSentinel.getPosition());

        verify(mChildView2, atLeastOnce()).isFocusable();
        verify(mChildView2, times(1)).setSelected(true);
        verify(mChildView2, times(1)).setSelected(anyBoolean());
        verifyNoMoreInteractions(mChildView1, mChildView2, mChildView3);

        // Reset selection back to none.

        mSelectionControllerWithSentinel.reset();
        verify(mChildView2, atLeastOnce()).isFocusable();
        verify(mChildView2, times(1)).setSelected(false);
        verify(mChildView2, times(2)).setSelected(anyBoolean());
        verifyNoMoreInteractions(mChildView1, mChildView2, mChildView3);

        assertEquals(null, mSelectionControllerWithSentinel.getPosition());
    }

    @Test
    public void setSelectedItem_moveSelectionFromAnotherItem_withSentinel() {
        assertEquals(null, mSelectionControllerWithSentinel.getPosition());
        mSelectionControllerWithSentinel.setPosition(1);
        clearInvocations(mChildView2);

        mSelectionControllerWithSentinel.setPosition(2);
        assertEquals(Integer.valueOf(2), mSelectionControllerWithSentinel.getPosition());

        verify(mChildView1, times(0)).setSelected(anyBoolean());
        verify(mChildView2, times(0)).setSelected(true);
        verify(mChildView2, times(1)).setSelected(false);
        verify(mChildView3, times(1)).setSelected(true);
        verify(mChildView3, times(0)).setSelected(false);
    }

    @Test
    public void setSelectedItem_moveSelectionToNone_withSentinel() {
        assertTrue(mSelectionControllerWithSentinel.isParkedAtSentinel());
        mSelectionControllerWithSentinel.setPosition(1);
        clearInvocations(mChildView2);

        mSelectionControllerWithSentinel.reset();
        assertTrue(mSelectionControllerWithSentinel.isParkedAtSentinel());

        verify(mChildView1, times(0)).setSelected(anyBoolean());
        verify(mChildView3, times(0)).setSelected(anyBoolean());
        verify(mChildView2, times(0)).setSelected(true);
        verify(mChildView2, times(1)).setSelected(false);
    }

    @Test
    public void setSelectedItem_indexNegative() {
        mSelectionController.setPosition(1);

        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());

        // Clamped to valid range.
        mSelectionController.setPosition(-2);
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
    }

    @Test
    public void setSelectedItem_indexNegative_withSentinel() {
        mSelectionControllerWithSentinel.setPosition(1);

        assertEquals(Integer.valueOf(1), mSelectionControllerWithSentinel.getPosition());

        // Parked at sentinel.
        mSelectionControllerWithSentinel.setPosition(-2);
        assertEquals(null, mSelectionControllerWithSentinel.getPosition());
    }

    @Test
    public void setSelectedItem_indexTooLarge() {
        mSelectionController.setPosition(1);
        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());

        // Clamped to valid range.
        mSelectionController.setPosition(30);
        assertEquals(Integer.valueOf(4), mSelectionController.getPosition());
    }

    @Test
    public void setSelectedItem_indexTooLarge_withSentinel() {
        mSelectionControllerWithSentinel.setPosition(1);
        assertEquals(Integer.valueOf(1), mSelectionControllerWithSentinel.getPosition());

        // Parked at sentinel.
        mSelectionControllerWithSentinel.setPosition(30);
        assertEquals(null, mSelectionControllerWithSentinel.getPosition());
    }

    @Test
    public void onChildViewAttached_viewIsReused_withSentinel() {
        // Simulates the case where View at position 1 is used as a View at position 3.
        when(mLayoutManager.getItemCount()).thenReturn(4);

        // Select View at position 1.
        mSelectionControllerWithSentinel.setPosition(1);
        verify(mChildView2, atLeastOnce()).isFocusable();
        verify(mChildView2).setSelected(true);
        verifyNoMoreInteractions(mChildView2);
        clearInvocations(mChildView2);

        // Pretend that the view is out of screen.
        // This should not result in view selection being cleared.
        when(mLayoutManager.findViewByPosition(1)).thenReturn(null);
        mSelectionControllerWithSentinel.onChildViewDetachedFromWindow(mChildView2);
        verify(mChildView2).setSelected(false);
        verifyNoMoreInteractions(mChildView2);
        clearInvocations(mChildView2);

        // Pretend that the View 1 is now reused as View 3.
        // We should see that the Selected state is cleared.
        when(mLayoutManager.findViewByPosition(3)).thenReturn(mChildView2);
        mSelectionControllerWithSentinel.onChildViewAttachedToWindow(mChildView2);
        verifyNoMoreInteractions(mChildView2);

        // Finally, pretend that the view 1 is back on screen.
        // This happens in 2 steps:
        // - 1. the view is removed from last position
        when(mLayoutManager.findViewByPosition(3)).thenReturn(null);
        mSelectionControllerWithSentinel.onChildViewDetachedFromWindow(mChildView2);
        // - 2. the view is inserted at position 1.
        when(mLayoutManager.findViewByPosition(1)).thenReturn(mChildView2);
        mSelectionControllerWithSentinel.onChildViewAttachedToWindow(mChildView2);
        // This will result in the setSelected(false) being called once, when we signal the view
        // is detached.
        verify(mChildView2).setSelected(false);
        verify(mChildView2).setSelected(true);
        verifyNoMoreInteractions(mChildView2);
    }

    @Test
    public void virtualViews_navigationAndCallbacks() {
        when(mLayoutManager.getItemCount()).thenReturn(4);
        mSelectionController.addVirtualView(1, mVirtualCallback);
        assertEquals(5, mSelectionController.getItemCount());

        mSelectionController.setPosition(0);
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
        verify(mChildView1).setSelected(true);

        clearInvocations(mChildView1);
        mSelectionController.selectNextItem();

        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());
        verify(mVirtualCallback, times(1)).onResult(true);
        verify(mChildView1).setSelected(false);
        verify(mChildView2, times(0)).setSelected(anyBoolean());

        mSelectionController.selectNextItem();

        assertEquals(Integer.valueOf(2), mSelectionController.getPosition());
        verify(mVirtualCallback, times(1)).onResult(false);
        verify(mChildView2).setSelected(true);

        clearInvocations(mChildView2);
        mSelectionController.selectPreviousItem();

        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());
        verify(mVirtualCallback, times(2)).onResult(true);
        verify(mChildView2).setSelected(false);
    }

    @Test
    public void virtualViews_removeVirtualView() {
        mSelectionController.addVirtualView(1, mVirtualCallback);
        mSelectionController.removeVirtualView(1);

        mSelectionController.setPosition(0);
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
        verify(mChildView1).setSelected(true);

        mSelectionController.selectNextItem();

        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());
        verify(mChildView2).setSelected(true);

        verifyNoInteractions(mVirtualCallback);
    }
}
