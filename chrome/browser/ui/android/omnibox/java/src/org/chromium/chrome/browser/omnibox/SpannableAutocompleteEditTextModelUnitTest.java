// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.text.SpannableStringBuilder;
import android.view.KeyEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link SpannableAutocompleteEditTextModel}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SpannableAutocompleteEditTextModelUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock SpannableAutocompleteEditTextModel.AutocompleteInputConnection mConnection;
    private @Mock AutocompleteEditTextModelBase.Delegate mDelegate;
    private SpannableAutocompleteEditTextModel mModel;
    private AutocompleteState mCurrentState;
    private AtomicInteger mImeCommandNestLevel;

    @Before
    public void setUp() {
        doReturn(new SpannableStringBuilder("text")).when(mDelegate).getText();
        mModel = new SpannableAutocompleteEditTextModel(mDelegate);
        mModel.setInputConnectionForTesting(mConnection);
        mImeCommandNestLevel = new AtomicInteger();
        mCurrentState = mModel.getCurrentAutocompleteState();
        clearInvocations(mDelegate);

        doAnswer(
                        inv -> {
                            return mImeCommandNestLevel.incrementAndGet() != 0;
                        })
                .when(mConnection)
                .onBeginImeCommand();

        doAnswer(
                        inv -> {
                            return mImeCommandNestLevel.decrementAndGet() == 0;
                        })
                .when(mConnection)
                .onEndImeCommand();
    }

    @Test
    public void testNonCompositionalText() {
        assertTrue(SpannableAutocompleteEditTextModel.isNonCompositionalText("http://123.com"));
        assertTrue(SpannableAutocompleteEditTextModel.isNonCompositionalText("goo"));
        assertFalse(SpannableAutocompleteEditTextModel.isNonCompositionalText("네이버"));
        assertFalse(SpannableAutocompleteEditTextModel.isNonCompositionalText("네"));
        assertFalse(SpannableAutocompleteEditTextModel.isNonCompositionalText("123네이버"));
    }

    private void confirmAutocompletionApplied(int keyCode) {
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, keyCode);

        clearInvocations(mConnection, mDelegate);
        mModel.dispatchKeyEvent(event);
        verify(mDelegate, times(0)).super_dispatchKeyEvent(event);
        verify(mConnection).commitAutocomplete();

        // Secondary, not directly linked to the test.
        verify(mConnection, atLeastOnce()).onBeginImeCommand();
        verify(mConnection, atLeastOnce()).onEndImeCommand();
        assertEquals(0, mImeCommandNestLevel.get());
        verifyNoMoreInteractions(mConnection, mDelegate);
    }

    private void confirmAutocompletionBypassed(int keyCode) {
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, keyCode);

        clearInvocations(mConnection, mDelegate);
        mModel.dispatchKeyEvent(event);
        verify(mConnection, times(0)).commitAutocomplete();
        verify(mDelegate).super_dispatchKeyEvent(event);

        // Secondary, not directly linked to the test.
        verify(mConnection, atLeastOnce()).onBeginImeCommand();
        verify(mConnection, atLeastOnce()).onEndImeCommand();
        assertEquals(0, mImeCommandNestLevel.get());
        verifyNoMoreInteractions(mConnection, mDelegate);
    }

    @Test
    public void dispatchKeyEvent_processAutocompleteKeysWhenAutocompletionIsAvailable_ltr() {
        mModel.setLayoutDirectionIsLtr(true);
        mCurrentState.setAutocompleteText("google.com");

        confirmAutocompletionApplied(KeyEvent.KEYCODE_DPAD_RIGHT);
        confirmAutocompletionApplied(KeyEvent.KEYCODE_ENTER);
        confirmAutocompletionApplied(KeyEvent.KEYCODE_TAB);

        // The following keys should not apply autocompletion.
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_LEFT);
    }

    @Test
    public void dispatchKeyEvent_processAutocompleteKeysWhenAutocompletionIsAvailable_rtl() {
        mModel.setLayoutDirectionIsLtr(false);
        mCurrentState.setAutocompleteText("google.com");

        confirmAutocompletionApplied(KeyEvent.KEYCODE_DPAD_LEFT);
        confirmAutocompletionApplied(KeyEvent.KEYCODE_ENTER);
        confirmAutocompletionApplied(KeyEvent.KEYCODE_TAB);

        // The following keys should not apply autocompletion.
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_RIGHT);
    }

    @Test
    public void dispatchKeyEvent_passAutocompleteKeysWhenAutocompletionIsNotAvailable_ltr() {
        mModel.setLayoutDirectionIsLtr(true);
        mCurrentState.setAutocompleteText("");

        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_RIGHT);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_ENTER);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_TAB);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_LEFT);
    }

    @Test
    public void dispatchKeyEvent_passAutocompleteKeysWhenAutocompletionIsNotAvailable_rtl() {
        mModel.setLayoutDirectionIsLtr(false);
        mCurrentState.setAutocompleteText("");

        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_RIGHT);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_ENTER);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_TAB);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_LEFT);
    }
}
