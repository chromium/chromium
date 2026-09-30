// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.clearInvocations;

import android.content.Context;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.SelectionController.TraversalMode;

/** Tests for {@link RecyclerViewSelectionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RecyclerViewSelectionControllerUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private LayoutManager mLayoutManager;
    @Mock private Callback<Boolean> mVirtualCallback;
    private View mChildView1;
    private View mChildView2;
    private View mChildView3;
    private View mChildView4;
    private View mChildView5;
    RecyclerViewSelectionController mSelectionController;
    RecyclerViewSelectionController mSelectionControllerWithSentinel;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        mChildView1 = new View(context);
        mChildView2 = new View(context);
        mChildView3 = new View(context);
        mChildView4 = new View(context);
        mChildView5 = new View(context);

        mChildView1.setFocusable(true);
        mChildView2.setFocusable(true);
        mChildView3.setFocusable(true);
        mChildView4.setFocusable(true);
        mChildView5.setFocusable(true);

        lenient().when(mLayoutManager.getItemCount()).thenReturn(5);
        lenient().when(mLayoutManager.findViewByPosition(0)).thenReturn(mChildView1);
        lenient().when(mLayoutManager.findViewByPosition(1)).thenReturn(mChildView2);
        lenient().when(mLayoutManager.findViewByPosition(2)).thenReturn(mChildView3);
        lenient().when(mLayoutManager.findViewByPosition(3)).thenReturn(mChildView4);
        lenient().when(mLayoutManager.findViewByPosition(4)).thenReturn(mChildView5);

        mSelectionController =
                new RecyclerViewSelectionController(mLayoutManager, TraversalMode.SATURATING);
        mSelectionControllerWithSentinel =
                new RecyclerViewSelectionController(
                        mLayoutManager, TraversalMode.SATURATING_WITH_SENTINEL);

        // Saturating controller will initialize selection, impacting tests. Reset this right away.
        mChildView1.setSelected(false);
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
        mChildView2.setFocusable(false);

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
        mChildView2.setFocusable(false);

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

        assertFalse(mChildView1.isSelected());
        assertTrue(mChildView2.isSelected());
        assertFalse(mChildView3.isSelected());

        // Reset selection back to none.

        mSelectionControllerWithSentinel.reset();
        assertFalse(mChildView1.isSelected());
        assertFalse(mChildView2.isSelected());
        assertFalse(mChildView3.isSelected());

        assertEquals(null, mSelectionControllerWithSentinel.getPosition());
    }

    @Test
    public void setSelectedItem_moveSelectionFromAnotherItem_withSentinel() {
        assertEquals(null, mSelectionControllerWithSentinel.getPosition());
        mSelectionControllerWithSentinel.setPosition(1);
        assertTrue(mChildView2.isSelected());

        mSelectionControllerWithSentinel.setPosition(2);
        assertEquals(Integer.valueOf(2), mSelectionControllerWithSentinel.getPosition());

        assertFalse(mChildView1.isSelected());
        assertFalse(mChildView2.isSelected());
        assertTrue(mChildView3.isSelected());
    }

    @Test
    public void setSelectedItem_moveSelectionToNone_withSentinel() {
        assertTrue(mSelectionControllerWithSentinel.isParkedAtSentinel());
        mSelectionControllerWithSentinel.setPosition(1);
        assertTrue(mChildView2.isSelected());

        mSelectionControllerWithSentinel.reset();
        assertTrue(mSelectionControllerWithSentinel.isParkedAtSentinel());

        assertFalse(mChildView1.isSelected());
        assertFalse(mChildView2.isSelected());
        assertFalse(mChildView3.isSelected());
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
        assertTrue(mChildView2.isSelected());

        // Pretend that the view is out of screen.
        // This should not result in view selection being cleared.
        when(mLayoutManager.findViewByPosition(1)).thenReturn(null);
        mSelectionControllerWithSentinel.onChildViewDetachedFromWindow(mChildView2);
        assertFalse(mChildView2.isSelected());

        // Pretend that the View 1 is now reused as View 3.
        // We should see that the Selected state is cleared.
        mSelectionControllerWithSentinel.onChildViewAttachedToWindow(mChildView2);
        assertFalse(mChildView2.isSelected());

        // Finally, pretend that the view 1 is back on screen.
        // This happens in 2 steps:
        // - 1. the view is removed from last position
        mSelectionControllerWithSentinel.onChildViewDetachedFromWindow(mChildView2);
        assertFalse(mChildView2.isSelected());
        // - 2. the view is inserted at position 1.
        when(mLayoutManager.findViewByPosition(1)).thenReturn(mChildView2);
        mSelectionControllerWithSentinel.onChildViewAttachedToWindow(mChildView2);
        assertTrue(mChildView2.isSelected());
    }

    @Test
    public void virtualViews_navigationAndCallbacks() {
        when(mLayoutManager.getItemCount()).thenReturn(4);
        mSelectionController.addVirtualView(1, mVirtualCallback);
        assertEquals(5, mSelectionController.getItemCount());

        mSelectionController.setPosition(0);
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
        assertTrue(mChildView1.isSelected());

        mSelectionController.selectNextItem();

        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());
        verify(mVirtualCallback).onResult(/* result= */ true);
        assertFalse(mChildView1.isSelected());
        assertFalse(mChildView2.isSelected());

        mSelectionController.selectNextItem();

        assertEquals(Integer.valueOf(2), mSelectionController.getPosition());
        verify(mVirtualCallback).onResult(/* result= */ false);
        assertTrue(mChildView2.isSelected());

        mSelectionController.selectPreviousItem();

        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());
        verify(mVirtualCallback, times(2)).onResult(/* result= */ true);
        assertFalse(mChildView2.isSelected());
    }

    @Test
    public void virtualViews_removeVirtualView() {
        mSelectionController.addVirtualView(1, mVirtualCallback);
        mSelectionController.removeVirtualView(1);

        mSelectionController.setPosition(0);
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
        assertTrue(mChildView1.isSelected());

        mSelectionController.selectNextItem();

        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());
        assertTrue(mChildView2.isSelected());

        verifyNoInteractions(mVirtualCallback);
    }

    @Test
    public void virtualViews_removeCurrentlySelectedVirtualView() {
        mSelectionController.addVirtualView(1, mVirtualCallback);
        mSelectionController.setPosition(1);
        assertEquals(Integer.valueOf(1), mSelectionController.getPosition());
        verify(mVirtualCallback).onResult(/* result= */ true);

        clearInvocations(mVirtualCallback);
        mSelectionController.removeVirtualView(1);
        assertEquals(Integer.valueOf(0), mSelectionController.getPosition());
        verify(mVirtualCallback).onResult(/* result= */ false);
        assertTrue(mChildView1.isSelected());
    }

    @Test
    public void virtualViews_removeCurrentlySelectedVirtualView_withSentinel() {
        mSelectionControllerWithSentinel.addVirtualView(1, mVirtualCallback);
        mSelectionControllerWithSentinel.setPosition(1);
        assertEquals(Integer.valueOf(1), mSelectionControllerWithSentinel.getPosition());
        verify(mVirtualCallback).onResult(/* result= */ true);

        clearInvocations(mVirtualCallback);
        mSelectionControllerWithSentinel.removeVirtualView(1);
        assertNull(mSelectionControllerWithSentinel.getPosition());
        verify(mVirtualCallback).onResult(/* result= */ false);
    }
}
