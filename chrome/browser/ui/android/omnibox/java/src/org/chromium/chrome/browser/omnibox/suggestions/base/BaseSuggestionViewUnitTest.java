// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.RecyclerViewSelectionController;
import org.chromium.chrome.browser.omnibox.test.R;

/** Tests for {@link BaseSuggestionView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BaseSuggestionViewUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock View.OnClickListener mOnClickListener;
    private @Mock View.OnLongClickListener mOnLongClickListener;

    private Context mContext;
    private View mInnerView;
    private BaseSuggestionView<View> mView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mInnerView = new View(mContext);
        mView = spy(new BaseSuggestionView<>(mInnerView));
        mView.setOnClickListener(mOnClickListener);
        mView.setOnLongClickListener(mOnLongClickListener);
    }

    private boolean sendKey(int keyCode) {
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, keyCode);
        return mView.onKeyDown(keyCode, event);
    }

    @Test
    public void onKeyDown_enterKeyActivatesSuggestion() {
        // View internally installs a mechanism to detect enter long-presses.
        // We currently don't pass onKeyUp event, which incorrectly triggers a long-press event.
        // This test evaluates that <Enter> key triggers the navigation event until we plumb both
        // keyDown and keyUp events.
        assertTrue(sendKey(KeyEvent.KEYCODE_ENTER));
        verify(mOnClickListener).onClick(any());
        verifyNoMoreInteractions(mOnClickListener, mOnLongClickListener);
        verify(mView, never()).super_onKeyDown(anyInt(), any());
    }

    @Test
    public void onKeyDown_actionButtonKeysAreConsumedIfActionsArePresent() {
        var controller = mock(RecyclerViewSelectionController.class);
        mView.actionChipsView.setSelectionControllerForTesting(controller);

        // Simulate Actions consuming key stroke.
        doReturn(true).when(controller).selectNextItem();
        assertTrue(sendKey(KeyEvent.KEYCODE_TAB));
        verify(mView, never()).super_onKeyDown(anyInt(), any());

        // Simulate Actions rejecting key stroke.
        doReturn(false).when(controller).selectNextItem();
        assertFalse(sendKey(KeyEvent.KEYCODE_TAB));
        verify(mView).super_onKeyDown(anyInt(), any());
    }

    @Test
    public void onKeyDown_unrecognizedKeysPassedToSuper() {
        assertFalse(sendKey(KeyEvent.KEYCODE_A));
        verifyNoMoreInteractions(mOnClickListener, mOnLongClickListener);
        verify(mView).super_onKeyDown(eq(KeyEvent.KEYCODE_A), any());
    }

    @Test
    public void setSelected_noFocusListener() {
        // No side effects if the listener is not installed.
        mView.setOnFocusViaSelectionListener(null);
        mView.setSelected(false);
        mView.setSelected(true);
        mView.setSelected(false);
    }

    @Test
    public void setSelected_withFocusListener() {
        Runnable callback = mock(Runnable.class);
        mView.setOnFocusViaSelectionListener(callback);

        mView.setSelected(false);
        verifyNoMoreInteractions(callback);

        mView.setSelected(true);
        verify(callback).run();
        clearInvocations(callback);

        mView.setSelected(false);
        verifyNoMoreInteractions(callback);
    }

    @Test
    public void setActionButtonsCount_addButtons() {
        // Verify that we don't unnecessarily create new buttons / retain already created ones.
        assertEquals(0, mView.getActionButtons().size());

        mView.setActionButtonsCount(1);
        assertEquals(1, mView.getActionButtons().size());
        var btn0 = mView.getActionButtons().get(0);
        assertNotNull(btn0);

        mView.setActionButtonsCount(2);
        assertEquals(2, mView.getActionButtons().size());
        assertEquals(btn0, mView.getActionButtons().get(0));
        var btn1 = mView.getActionButtons().get(1);
        assertNotEquals(btn0, btn1);

        // Retain buttons / no change.
        mView.setActionButtonsCount(2);
        assertEquals(2, mView.getActionButtons().size());
        assertEquals(btn0, mView.getActionButtons().get(0));
        assertEquals(btn1, mView.getActionButtons().get(1));
    }

    @Test
    public void setActionButtonsCount_removeButtons() {
        // Verify that we don't unnecessarily create new buttons / retain already created ones.
        assertEquals(0, mView.getActionButtons().size());

        mView.setActionButtonsCount(2);
        assertEquals(2, mView.getActionButtons().size());
        var btn0 = mView.getActionButtons().get(0);
        assertNotNull(btn0);

        mView.setActionButtonsCount(1);
        assertEquals(1, mView.getActionButtons().size());
        assertEquals(btn0, mView.getActionButtons().get(0));
    }

    @Test
    public void isFocused_replicatesSuperFocus() {
        mView.setSelected(false);

        doReturn(false).when(mView).super_isFocused();
        assertFalse(mView.isFocused());

        doReturn(true).when(mView).super_isFocused();
        assertTrue(mView.isFocused());
    }

    @Test
    public void isFocused_selectionHintsFocusExceptTouchMode() {
        doReturn(false).when(mView).super_isFocused();

        mView.setSelected(true);
        assertTrue(mView.isFocused());

        mView.setSelected(false);
        assertFalse(mView.isFocused());
    }
}
