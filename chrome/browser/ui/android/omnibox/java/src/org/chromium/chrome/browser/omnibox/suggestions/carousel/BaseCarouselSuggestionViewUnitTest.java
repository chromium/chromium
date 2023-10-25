// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
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
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Tests for {@link BaseCarouselSuggestionView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BaseCarouselSuggestionViewUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock SimpleRecyclerViewAdapter mAdapter;
    private @Mock BaseCarouselSuggestionSelectionManager mManager;
    private @Mock View mChild;
    private @Spy BaseCarouselSuggestionView mView =
            new BaseCarouselSuggestionView(ContextUtils.getApplicationContext(), mAdapter);

    @Before
    public void setUp() {
        mView.setSelectionManagerForTesting(mManager);
        clearInvocations(mView, mAdapter, mManager, mChild);
    }

    @Test
    public void onKeyDown_selectNextItem_ltr() {
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mView).getLayoutDirection();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_RIGHT);
        assertTrue(event.dispatch(mView));

        verify(mView).onKeyDown(event.getKeyCode(), event);
        verify(mManager).selectNextItem();

        verifyNoMoreInteractions(mManager);
    }

    @Test
    public void onKeyDown_selectNextItem_rtl() {
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mView).getLayoutDirection();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_LEFT);
        assertTrue(event.dispatch(mView));

        verify(mView).onKeyDown(event.getKeyCode(), event);
        verify(mManager).selectNextItem();

        verifyNoMoreInteractions(mManager);
    }

    @Test
    public void onKeyDown_selectPrevItem_ltr() {
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mView).getLayoutDirection();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_LEFT);
        assertTrue(event.dispatch(mView));

        verify(mView).onKeyDown(event.getKeyCode(), event);
        verify(mManager).selectPreviousItem();

        verifyNoMoreInteractions(mManager);
    }

    @Test
    public void onKeyDown_selectPrevItem_rtl() {
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mView).getLayoutDirection();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_RIGHT);
        assertTrue(event.dispatch(mView));

        verify(mView).onKeyDown(event.getKeyCode(), event);
        verify(mManager).selectPreviousItem();

        verifyNoMoreInteractions(mManager);
    }

    @Test
    public void onKeyDown_enterKeyPassedThroughWhenNoItemSelected() {
        doReturn(null).when(mManager).getSelectedView();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
        assertFalse(event.dispatch(mView));

        verify(mView).onKeyDown(event.getKeyCode(), event);
        verify(mManager).getSelectedView();
        verify(mView).superOnKeyDown(event.getKeyCode(), event);

        verifyNoMoreInteractions(mChild, mManager);
    }

    @Test
    public void onKeyDown_enterKeyAcceptsSelectedItem() {
        doReturn(mChild).when(mManager).getSelectedView();
        doReturn(true).when(mChild).performClick();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
        assertTrue(event.dispatch(mView));

        verify(mView).onKeyDown(event.getKeyCode(), event);
        verify(mManager).getSelectedView();
        verify(mChild).performClick();

        verifyNoMoreInteractions(mChild, mManager);
    }

    @Test
    public void onKeyDown_unhandledKeysArePassedToSuper() {
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_T);
        assertFalse(event.dispatch(mView));

        verify(mView).onKeyDown(KeyEvent.KEYCODE_T, event);
        verify(mView).superOnKeyDown(KeyEvent.KEYCODE_T, event);

        verifyNoMoreInteractions(mManager);
    }

    @Test
    public void setSelected_resetsCarouselSelectionWhenSelected() {
        mView.setSelected(true);
        verify(mManager, times(1)).setSelectedItem(0, /* force= */ true);
        verifyNoMoreInteractions(mManager);
    }

    @Test
    public void setSelected_resetsCarouselSelectionWhenDeselected() {
        mView.setSelected(false);
        verify(mManager, times(1)).setSelectedItem(RecyclerView.NO_POSITION, /* force= */ false);
        verifyNoMoreInteractions(mManager);
    }
}
