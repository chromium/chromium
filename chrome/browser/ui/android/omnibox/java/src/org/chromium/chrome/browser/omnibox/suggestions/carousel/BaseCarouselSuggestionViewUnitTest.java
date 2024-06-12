// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.view.KeyEvent;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.RecyclerViewSelectionController;
import org.chromium.chrome.browser.omnibox.suggestions.base.DynamicSpacingRecyclerViewItemDecoration;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Tests for {@link BaseCarouselSuggestionView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BaseCarouselSuggestionViewUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock SimpleRecyclerViewAdapter mAdapter;
    private @Mock RecyclerViewSelectionController mController;
    private @Mock DynamicSpacingRecyclerViewItemDecoration mDecoration;
    private @Mock View mChild;
    private @Spy BaseCarouselSuggestionView mView =
            new BaseCarouselSuggestionView(ContextUtils.getApplicationContext(), mAdapter);

    @Before
    public void setUp() {
        mView.setSelectionControllerForTesting(mController);
        mView.setItemDecoration(mDecoration);
        clearInvocations(mView, mAdapter, mController, mChild);
    }

    @Test
    public void onKeyDown_selectNextItem() {
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB);

        doReturn(true).when(mController).selectNextItem();
        assertTrue(event.dispatch(mView, null, null));

        doReturn(false).when(mController).selectNextItem();
        assertFalse(event.dispatch(mView, null, null));
    }

    @Test
    public void setItemDecoration_nullDecorations() {
        // One decoration installed by the setUp routine.
        assertEquals(1, mView.getItemDecorationCount());

        // Reset current decoration to null.
        mView.setItemDecoration(null);
        assertEquals(0, mView.getItemDecorationCount());

        // One decoration re-installed.
        mView.setItemDecoration(mDecoration);
        assertEquals(1, mView.getItemDecorationCount());
    }

    @Test
    public void onKeyDown_selectPrevItem() {
        var event =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_TAB,
                        0,
                        KeyEvent.META_SHIFT_ON);

        doReturn(true).when(mController).selectPreviousItem();
        assertTrue(event.dispatch(mView, null, null));

        doReturn(false).when(mController).selectPreviousItem();
        assertFalse(event.dispatch(mView, null, null));
    }

    @Test
    public void onKeyDown_enterKeyPassedThroughWhenNoItemSelected() {
        doReturn(null).when(mController).getSelectedView();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
        assertFalse(event.dispatch(mView));

        verify(mView).onKeyDown(event.getKeyCode(), event);
        verify(mController).getSelectedView();
        verify(mView).superOnKeyDown(event.getKeyCode(), event);

        verifyNoMoreInteractions(mChild, mController);
    }

    @Test
    public void onKeyDown_enterKeyAcceptsSelectedItem() {
        doReturn(mChild).when(mController).getSelectedView();
        doReturn(true).when(mChild).performClick();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
        assertTrue(event.dispatch(mView));

        verify(mView).onKeyDown(event.getKeyCode(), event);
        verify(mController).getSelectedView();
        verify(mChild).performClick();

        verifyNoMoreInteractions(mChild, mController);
    }

    @Test
    public void onKeyDown_unhandledKeysArePassedToSuper() {
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_T);
        assertFalse(event.dispatch(mView));

        verify(mView).onKeyDown(KeyEvent.KEYCODE_T, event);
        verify(mView).superOnKeyDown(KeyEvent.KEYCODE_T, event);

        verifyNoMoreInteractions(mController);
    }

    @Test
    public void setSelected_resetsCarouselSelectionWhenSelected() {
        mView.setSelected(true);
        verify(mController, times(1)).setSelectedItem(0);
        verifyNoMoreInteractions(mController);
    }

    @Test
    public void setSelected_resetsCarouselSelectionWhenDeselected() {
        mView.setSelected(false);
        verify(mController, times(1)).setSelectedItem(RecyclerView.NO_POSITION);
        verifyNoMoreInteractions(mController);
    }

    @Test
    public void onMeasure_invalidateItemDecorationsWhenTheyChange() {
        doReturn(true).when(mDecoration).notifyViewSizeChanged(anyBoolean(), anyInt(), anyInt());
        mView.onMeasure(0, 0);
        // Must be called if the decorations report changes.
        verify(mView).invalidateItemDecorations();
    }

    @Test
    public void onMeasure_dontInvalidateItemDecorationsWhenTheyDontChange() {
        doReturn(false).when(mDecoration).notifyViewSizeChanged(anyBoolean(), anyInt(), anyInt());
        mView.onMeasure(0, 0);
        // Must be called if the decorations report changes.
        verify(mView, never()).invalidateItemDecorations();
    }
}
