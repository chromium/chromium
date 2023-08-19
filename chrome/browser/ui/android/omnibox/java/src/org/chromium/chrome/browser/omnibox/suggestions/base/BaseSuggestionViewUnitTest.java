// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
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
import org.chromium.chrome.browser.omnibox.test.R;

/**
 * Tests for {@link BaseSuggestionView}.
 */
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
        mContext = new ContextThemeWrapper(
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
        sendKey(KeyEvent.KEYCODE_ENTER);
        verify(mOnClickListener).onClick(any());
        verifyNoMoreInteractions(mOnClickListener, mOnLongClickListener);
    }

    @Test
    public void onKeyDown_actionButtonActivation_noActionButtons_ltr() {
        mView.setActionButtonsCount(0);
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mView).getLayoutDirection();

        // Observe no crashes or other bad behavior when buttons are not available.
        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
    }

    @Test
    public void onKeyDown_actionButtonActivation_noActionButtons_rtl() {
        mView.setActionButtonsCount(0);
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mView).getLayoutDirection();

        // Observe no crashes or other bad behavior when buttons are not available.
        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
    }

    @Test
    public void onKeyDown_actionButtonActivation_oneActionButton_ltr() {
        mView.setActionButtonsCount(1);
        View v = (View) mView.getActionButtons().get(0);
        v.setOnClickListener(mOnClickListener);
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mView).getLayoutDirection();

        // DPAD_RIGHT activates the only action button.
        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        verify(mOnClickListener).onClick(any());
        verifyNoMoreInteractions(mOnClickListener);

        // DPAD_LEFT is no-op.
        sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
        verifyNoMoreInteractions(mOnClickListener);
    }

    @Test
    public void onKeyDown_actionButtonActivation_oneActionButton_rtl() {
        mView.setActionButtonsCount(1);
        View v = (View) mView.getActionButtons().get(0);
        v.setOnClickListener(mOnClickListener);
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mView).getLayoutDirection();

        // DPAD_LEFT activates the only action button.
        sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
        verify(mOnClickListener).onClick(any());
        verifyNoMoreInteractions(mOnClickListener);

        // DPAD_RIGHT is no-op.
        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        verifyNoMoreInteractions(mOnClickListener);
    }

    @Test
    public void onKeyDown_actionButtonActivation_manyActionButtons_ltr() {
        mView.setActionButtonsCount(2);
        ((View) mView.getActionButtons().get(0)).setOnClickListener(mOnClickListener);
        ((View) mView.getActionButtons().get(1)).setOnClickListener(mOnClickListener);
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mView).getLayoutDirection();

        // Observe no interactions.
        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        verifyNoMoreInteractions(mOnClickListener);
        sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
        verifyNoMoreInteractions(mOnClickListener);
    }

    @Test
    public void onKeyDown_actionButtonActivation_manyActionButtons_rtl() {
        mView.setActionButtonsCount(2);
        ((View) mView.getActionButtons().get(0)).setOnClickListener(mOnClickListener);
        ((View) mView.getActionButtons().get(1)).setOnClickListener(mOnClickListener);
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mView).getLayoutDirection();

        // Observe no interactions.
        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        verifyNoMoreInteractions(mOnClickListener);
        sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
        verifyNoMoreInteractions(mOnClickListener);
    }
}
