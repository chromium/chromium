// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.view.KeyEvent;
import android.view.View;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for {@link ActionChipsView}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionChipsViewUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock ActionChipsAdapter mAdapter;
    private @Mock View mChild;
    private @Spy ActionChipsView mView = new ActionChipsView(ContextUtils.getApplicationContext());

    @Test
    public void keyDispatch_noDispatchWhenAdapterNotSet() {
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB);
        assertFalse(event.dispatch(mView));

        verify(mView, times(1)).onKeyDown(event.getKeyCode(), event);
        verifyNoMoreInteractions(mView);

        verifyNoMoreInteractions(mAdapter);
    }

    private void installAdapter() {
        mView.setAdapter(mAdapter);
        clearInvocations(mAdapter);
        clearInvocations(mView);
    }

    @Test
    public void keyDispatch_tabSelectsNextItem() {
        installAdapter();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB);
        assertTrue(event.dispatch(mView));

        verify(mView, times(1)).onKeyDown(event.getKeyCode(), event);
        verifyNoMoreInteractions(mView);

        verify(mAdapter, times(1)).selectNextItem();
        verifyNoMoreInteractions(mAdapter);
    }

    @Test
    public void keyDispatch_shiftTabSelectsPreviousItem() {
        installAdapter();

        var event = new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB, 0, KeyEvent.META_SHIFT_ON);
        assertTrue(event.dispatch(mView));

        verify(mView, times(1)).onKeyDown(event.getKeyCode(), event);
        verifyNoMoreInteractions(mView);

        verify(mAdapter, times(1)).selectPreviousItem();
        verifyNoMoreInteractions(mAdapter);
    }

    @Test
    public void keyDispatch_enterKeyPassedThroughWhenNoChipsSelected() {
        installAdapter();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
        assertFalse(event.dispatch(mView));

        verify(mView, times(1)).onKeyDown(event.getKeyCode(), event);
        verify(mView, times(1)).superOnKeyDown(event.getKeyCode(), event);
        verifyNoMoreInteractions(mView);

        verify(mAdapter, times(1)).getSelectedView();
        verifyNoMoreInteractions(mAdapter);
    }

    @Test
    public void keyDispatch_enterKeyAcceptsSelectedChip() {
        installAdapter();

        doReturn(mChild).when(mAdapter).getSelectedView();
        doReturn(true).when(mChild).performClick();
        clearInvocations(mView);

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
        assertTrue(event.dispatch(mView));

        verify(mChild, times(1)).performClick();
        verify(mView, times(1)).onKeyDown(event.getKeyCode(), event);
        verifyNoMoreInteractions(mView);

        verify(mAdapter, times(1)).getSelectedView();
        verifyNoMoreInteractions(mAdapter);
    }

    @Test
    public void keyDispatch_unhandledKeysArePassedToSuper() {
        installAdapter();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_T);
        assertFalse(event.dispatch(mView));

        verify(mView, times(1)).onKeyDown(KeyEvent.KEYCODE_T, event);
        verify(mView, times(1)).superOnKeyDown(KeyEvent.KEYCODE_T, event);
        verifyNoMoreInteractions(mView);

        verifyNoMoreInteractions(mAdapter);
    }

    @Test
    public void selection_doesNothingWhenNoAdapter() {
        mView.setSelected(true);
        verifyNoMoreInteractions(mAdapter);

        mView.setSelected(false);
        verifyNoMoreInteractions(mAdapter);
    }

    @Test
    public void selection_resetsCarouselSelectionWhenSelected() {
        installAdapter();

        mView.setSelected(true);
        verify(mAdapter, times(1)).resetSelection();
        verifyNoMoreInteractions(mAdapter);
    }

    @Test
    public void selection_resetsCarouselSelectionWhenDeselected() {
        installAdapter();

        mView.setSelected(false);
        verify(mAdapter, times(1)).resetSelection();
        verifyNoMoreInteractions(mAdapter);
    }
}
