// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
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

import java.util.OptionalInt;

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
        Assert.assertEquals(OptionalInt.of(0), mSelectionController.getPosition());
        mSelectionController.selectNextItem();
        Assert.assertEquals(OptionalInt.of(1), mSelectionController.getPosition());
        Assert.assertEquals(mChildView2, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_fromNone_withSentinel() {
        Assert.assertTrue(mSelectionControllerWithSentinel.isParkedAtSentinel());
        mSelectionControllerWithSentinel.selectNextItem();
        Assert.assertEquals(OptionalInt.of(0), mSelectionControllerWithSentinel.getPosition());
        Assert.assertEquals(mChildView1, mSelectionControllerWithSentinel.getSelectedView());
    }

    @Test
    public void selectNextItem_fromPrevious() {
        mSelectionController.setPosition(1);
        Assert.assertEquals(OptionalInt.of(1), mSelectionController.getPosition());
        Assert.assertEquals(mChildView2, mSelectionController.getSelectedView());
        verify(mLayoutManager).scrollToPosition(3);

        mSelectionController.selectNextItem();
        Assert.assertEquals(OptionalInt.of(2), mSelectionController.getPosition());
        Assert.assertEquals(mChildView3, mSelectionController.getSelectedView());
        verify(mLayoutManager).scrollToPosition(4);
    }

    @Test
    public void selectNextItem_fromLast() {
        mSelectionController.setPosition(4);
        verify(mLayoutManager).scrollToPosition(4);
        Assert.assertEquals(OptionalInt.of(4), mSelectionController.getPosition());
        Assert.assertEquals(mChildView5, mSelectionController.getSelectedView());

        // Selecting next item should result in item being highlighted.
        Assert.assertFalse(mSelectionController.selectNextItem());
        Assert.assertEquals(OptionalInt.of(4), mSelectionController.getPosition());
        Assert.assertEquals(mChildView5, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_fromLast_withSentinel() {
        mSelectionControllerWithSentinel.setPosition(4);
        verify(mLayoutManager).scrollToPosition(4);
        Assert.assertEquals(OptionalInt.of(4), mSelectionControllerWithSentinel.getPosition());
        Assert.assertEquals(mChildView5, mSelectionControllerWithSentinel.getSelectedView());

        // Selecting next item should result in leaving valid range.
        Assert.assertFalse(mSelectionControllerWithSentinel.selectNextItem());

        Assert.assertEquals(OptionalInt.empty(), mSelectionControllerWithSentinel.getPosition());
        Assert.assertEquals(null, mSelectionControllerWithSentinel.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromNone_withSentinel() {
        Assert.assertEquals(OptionalInt.empty(), mSelectionControllerWithSentinel.getPosition());
        mSelectionControllerWithSentinel.selectPreviousItem();
        Assert.assertEquals(OptionalInt.empty(), mSelectionControllerWithSentinel.getPosition());
        Assert.assertEquals(null, mSelectionControllerWithSentinel.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromPrevious() {
        mSelectionController.setPosition(1);
        verify(mLayoutManager).scrollToPosition(3);
        Assert.assertEquals(OptionalInt.of(1), mSelectionController.getPosition());
        Assert.assertEquals(mChildView2, mSelectionController.getSelectedView());

        mSelectionController.selectPreviousItem();
        verify(mLayoutManager).scrollToPosition(0);
        Assert.assertEquals(OptionalInt.of(0), mSelectionController.getPosition());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromFirst() {
        mSelectionController.setPosition(0);
        Assert.assertEquals(OptionalInt.of(0), mSelectionController.getPosition());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());

        // Selecting previous item should result in item being highlighted.
        Assert.assertFalse(mSelectionController.selectPreviousItem());
        Assert.assertEquals(OptionalInt.of(0), mSelectionController.getPosition());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_fromFirst_withSentinel() {
        mSelectionControllerWithSentinel.setPosition(0);
        Assert.assertEquals(OptionalInt.of(0), mSelectionControllerWithSentinel.getPosition());
        Assert.assertEquals(mChildView1, mSelectionControllerWithSentinel.getSelectedView());
        Assert.assertFalse(mSelectionControllerWithSentinel.selectPreviousItem());
        Assert.assertEquals(OptionalInt.empty(), mSelectionControllerWithSentinel.getPosition());
        Assert.assertEquals(null, mSelectionControllerWithSentinel.getSelectedView());
    }

    @Test
    public void selectPreviousItem_skipNonFocusableItems_noCycling() {
        mSelectionController.setPosition(2);
        Assert.assertEquals(OptionalInt.of(2), mSelectionController.getPosition());
        Assert.assertEquals(mChildView3, mSelectionController.getSelectedView());

        // View at position 1 is not focusable:
        doReturn(false).when(mChildView2).isFocusable();

        // Focus skips position 1.
        mSelectionController.selectPreviousItem();
        Assert.assertEquals(OptionalInt.of(0), mSelectionController.getPosition());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_ignoreHeadNonFocusableViews() {
        mSelectionController.setPosition(2);
        Assert.assertEquals(OptionalInt.of(2), mSelectionController.getPosition());
        Assert.assertEquals(mChildView3, mSelectionController.getSelectedView());

        // Views at position 0 and 1 are not focusable:
        doReturn(false).when(mChildView1).isFocusable();
        doReturn(false).when(mChildView2).isFocusable();

        // Focus must not move.
        mSelectionController.selectPreviousItem();
        Assert.assertEquals(OptionalInt.of(2), mSelectionController.getPosition());
        Assert.assertEquals(mChildView3, mSelectionController.getSelectedView());
    }

    @Test
    public void selectPreviousItem_skipNonFocusableItems_withSentinel() {
        mSelectionControllerWithSentinel.setPosition(1);
        Assert.assertEquals(OptionalInt.of(1), mSelectionControllerWithSentinel.getPosition());
        Assert.assertEquals(mChildView2, mSelectionControllerWithSentinel.getSelectedView());

        // View at position 0 is not focusable:
        doReturn(false).when(mChildView1).isFocusable();

        // We wrap around ignoring view at position 2.
        mSelectionControllerWithSentinel.selectPreviousItem();
        Assert.assertEquals(OptionalInt.empty(), mSelectionControllerWithSentinel.getPosition());
        Assert.assertEquals(null, mSelectionControllerWithSentinel.getSelectedView());
    }

    @Test
    public void selectPreviousItem_noFocusableViews() {
        doReturn(false).when(mChildView1).isFocusable();
        doReturn(false).when(mChildView2).isFocusable();
        doReturn(false).when(mChildView3).isFocusable();
        doReturn(false).when(mChildView4).isFocusable();
        doReturn(false).when(mChildView5).isFocusable();

        // Re-create controller (because when it was created, all these views were focusable).
        mSelectionController =
                new RecyclerViewSelectionController(
                        mLayoutManager, RecyclerViewSelectionController.Mode.SATURATING);

        Assert.assertEquals(OptionalInt.empty(), mSelectionController.getPosition());
        Assert.assertEquals(null, mSelectionController.getSelectedView());

        mSelectionController.selectPreviousItem();
        Assert.assertEquals(OptionalInt.empty(), mSelectionController.getPosition());
        Assert.assertEquals(null, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_skipNonFocusableItems_noCycling() {
        mSelectionController.setPosition(0);
        Assert.assertEquals(OptionalInt.of(0), mSelectionController.getPosition());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());

        // View at position 1 is not focusable:
        doReturn(false).when(mChildView2).isFocusable();

        // Focus skips position 1.
        mSelectionController.selectNextItem();
        Assert.assertEquals(OptionalInt.of(2), mSelectionController.getPosition());
        Assert.assertEquals(mChildView3, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_ignoreTailNonFocusableViews() {
        // This controller initializes at 0th position by default.
        verify(mLayoutManager).scrollToPosition(2);
        Assert.assertEquals(OptionalInt.of(0), mSelectionController.getPosition());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());

        // Views at positions 1-4 are not focusable:
        doReturn(false).when(mChildView2).isFocusable();
        doReturn(false).when(mChildView3).isFocusable();
        doReturn(false).when(mChildView4).isFocusable();
        doReturn(false).when(mChildView5).isFocusable();

        // Focus must not move.
        mSelectionController.selectNextItem();
        Assert.assertEquals(OptionalInt.of(0), mSelectionController.getPosition());
        Assert.assertEquals(mChildView1, mSelectionController.getSelectedView());
    }

    @Test
    public void selectNextItem_skipNonFocusableItems_withSentinel() {
        mSelectionControllerWithSentinel.setPosition(1);
        verify(mLayoutManager).scrollToPosition(3);
        Assert.assertEquals(OptionalInt.of(1), mSelectionControllerWithSentinel.getPosition());
        Assert.assertEquals(mChildView2, mSelectionControllerWithSentinel.getSelectedView());

        // Views at positions 2-4 are not focusable:
        doReturn(false).when(mChildView3).isFocusable();
        doReturn(false).when(mChildView4).isFocusable();
        doReturn(false).when(mChildView5).isFocusable();

        mSelectionControllerWithSentinel.selectNextItem();
        Assert.assertEquals(OptionalInt.empty(), mSelectionControllerWithSentinel.getPosition());
        Assert.assertEquals(null, mSelectionControllerWithSentinel.getSelectedView());
    }

    @Test
    public void selectNextItem_noFocusableViews() {
        doReturn(false).when(mChildView1).isFocusable();
        doReturn(false).when(mChildView2).isFocusable();
        doReturn(false).when(mChildView3).isFocusable();
        doReturn(false).when(mChildView4).isFocusable();
        doReturn(false).when(mChildView5).isFocusable();

        // Re-create controller (because when it was created, all these views were focusable).
        mSelectionController =
                new RecyclerViewSelectionController(
                        mLayoutManager, RecyclerViewSelectionController.Mode.SATURATING);

        Assert.assertEquals(OptionalInt.empty(), mSelectionController.getPosition());
        Assert.assertEquals(null, mSelectionController.getSelectedView());

        mSelectionController.selectNextItem();
        Assert.assertEquals(OptionalInt.empty(), mSelectionController.getPosition());
        Assert.assertEquals(null, mSelectionController.getSelectedView());
    }

    @Test
    public void setSelectedItem_moveSelectionFromNone_withSentinel() {
        Assert.assertEquals(OptionalInt.empty(), mSelectionControllerWithSentinel.getPosition());
        mSelectionControllerWithSentinel.setPosition(1);
        Assert.assertEquals(OptionalInt.of(1), mSelectionControllerWithSentinel.getPosition());

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

        Assert.assertEquals(OptionalInt.empty(), mSelectionControllerWithSentinel.getPosition());
    }

    @Test
    public void setSelectedItem_moveSelectionFromAnotherItem_withSentinel() {
        Assert.assertEquals(OptionalInt.empty(), mSelectionControllerWithSentinel.getPosition());
        mSelectionControllerWithSentinel.setPosition(1);
        clearInvocations(mChildView2);

        mSelectionControllerWithSentinel.setPosition(2);
        Assert.assertEquals(OptionalInt.of(2), mSelectionControllerWithSentinel.getPosition());

        verify(mChildView1, times(0)).setSelected(anyBoolean());
        verify(mChildView2, times(0)).setSelected(true);
        verify(mChildView2, times(1)).setSelected(false);
        verify(mChildView3, times(1)).setSelected(true);
        verify(mChildView3, times(0)).setSelected(false);
    }

    @Test
    public void setSelectedItem_moveSelectionToNone_withSentinel() {
        Assert.assertTrue(mSelectionControllerWithSentinel.isParkedAtSentinel());
        mSelectionControllerWithSentinel.setPosition(1);
        clearInvocations(mChildView2);

        mSelectionControllerWithSentinel.reset();
        Assert.assertTrue(mSelectionControllerWithSentinel.isParkedAtSentinel());

        verify(mChildView1, times(0)).setSelected(anyBoolean());
        verify(mChildView3, times(0)).setSelected(anyBoolean());
        verify(mChildView2, times(0)).setSelected(true);
        verify(mChildView2, times(1)).setSelected(false);
    }

    @Test
    public void setSelectedItem_indexNegative() {
        mSelectionController.setPosition(1);

        Assert.assertEquals(OptionalInt.of(1), mSelectionController.getPosition());

        // Clamped to valid range.
        mSelectionController.setPosition(-2);
        Assert.assertEquals(OptionalInt.of(0), mSelectionController.getPosition());
    }

    @Test
    public void setSelectedItem_indexNegative_withSentinel() {
        mSelectionControllerWithSentinel.setPosition(1);

        Assert.assertEquals(OptionalInt.of(1), mSelectionControllerWithSentinel.getPosition());

        // Parked at sentinel.
        mSelectionControllerWithSentinel.setPosition(-2);
        Assert.assertEquals(OptionalInt.empty(), mSelectionControllerWithSentinel.getPosition());
    }

    @Test
    public void setSelectedItem_indexTooLarge() {
        mSelectionController.setPosition(1);
        Assert.assertEquals(OptionalInt.of(1), mSelectionController.getPosition());

        // Clamped to valid range.
        mSelectionController.setPosition(30);
        Assert.assertEquals(OptionalInt.of(4), mSelectionController.getPosition());
    }

    @Test
    public void setSelectedItem_indexTooLarge_withSentinel() {
        mSelectionControllerWithSentinel.setPosition(1);
        Assert.assertEquals(OptionalInt.of(1), mSelectionControllerWithSentinel.getPosition());

        // Parked at sentinel.
        mSelectionControllerWithSentinel.setPosition(30);
        Assert.assertEquals(OptionalInt.empty(), mSelectionControllerWithSentinel.getPosition());
    }

    @Test
    public void onChildViewAttached_viewIsReused_withSentinel() {
        // Simulates the case where View at position 0 is used as a View at position 3.
        when(mLayoutManager.getItemCount()).thenReturn(4);

        // Select View at position 0.
        mSelectionControllerWithSentinel.setPosition(0);
        verify(mChildView1, atLeastOnce()).isFocusable();
        verify(mChildView1).setSelected(true);
        verifyNoMoreInteractions(mChildView1);
        verifyNoMoreInteractions(mChildView2);
        verifyNoMoreInteractions(mChildView3);
        clearInvocations(mChildView1);

        // Pretend that the view is out of screen.
        // This should not result in view selection being cleared.
        when(mLayoutManager.findViewByPosition(0)).thenReturn(null);
        mSelectionControllerWithSentinel.onChildViewDetachedFromWindow(mChildView1);
        verify(mChildView1).setSelected(false);
        verifyNoMoreInteractions(mChildView1);
        verifyNoMoreInteractions(mChildView2);
        verifyNoMoreInteractions(mChildView3);
        clearInvocations(mChildView1);

        // Pretend that the View 0 is now re-used as View 3.
        // We should see that the Selected state is cleared.
        when(mLayoutManager.findViewByPosition(3)).thenReturn(mChildView1);
        mSelectionControllerWithSentinel.onChildViewAttachedToWindow(mChildView1);
        verifyNoMoreInteractions(mChildView1);
        verifyNoMoreInteractions(mChildView2);
        verifyNoMoreInteractions(mChildView3);

        // Finally, pretend that the view 0 is back on screen.
        // This happens in 2 steps:
        // - 1. the view is removed from last position
        when(mLayoutManager.findViewByPosition(3)).thenReturn(null);
        mSelectionControllerWithSentinel.onChildViewDetachedFromWindow(mChildView1);
        // - 2. the view is inserted at first position.
        when(mLayoutManager.findViewByPosition(0)).thenReturn(mChildView1);
        mSelectionControllerWithSentinel.onChildViewAttachedToWindow(mChildView1);
        // This will result in the setSelected(false) being called once, when we signal the view
        // is detached.
        verify(mChildView1).setSelected(false);
        verify(mChildView1).setSelected(true);
        verifyNoMoreInteractions(mChildView1);
        verifyNoMoreInteractions(mChildView2);
        verifyNoMoreInteractions(mChildView3);
    }
}
