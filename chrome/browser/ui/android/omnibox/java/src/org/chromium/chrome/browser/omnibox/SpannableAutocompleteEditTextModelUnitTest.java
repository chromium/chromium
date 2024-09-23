// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.text.SpannableStringBuilder;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;

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

import java.util.Optional;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link SpannableAutocompleteEditTextModel}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SpannableAutocompleteEditTextModelUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock AutocompleteInputConnection mConnection;
    private @Mock AutocompleteEditTextModelBase.Delegate mDelegate;
    private SpannableAutocompleteEditTextModel mModel;
    private AutocompleteState mCurrentState;
    private AtomicInteger mImeCommandNestLevel;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        doReturn(new SpannableStringBuilder("text")).when(mDelegate).getText();
        mModel = new SpannableAutocompleteEditTextModel(mDelegate, context);
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

    // Dispatch the key code and check that it committed the autocomplete suggestion without
    // dispatching the key event to the delegate.
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

    // Dispatch the key code and check that it committed the autocomplete suggestion but also
    // forwarded the key event to the delegate.
    private void confirmAutocompletionAppliedWithKey(int keyCode) {
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, keyCode);

        clearInvocations(mConnection, mDelegate);
        mModel.dispatchKeyEvent(event);
        verify(mDelegate).super_dispatchKeyEvent(event);
        verify(mConnection).commitAutocomplete();

        // Allow the handler to implement the action possibly by setting the selection or not.
        verify(mDelegate, atLeast(0)).setSelection(anyInt(), anyInt());

        // Secondary, not directly linked to the test.
        verify(mConnection, atLeastOnce()).onBeginImeCommand();
        verify(mConnection, atLeastOnce()).onEndImeCommand();
        assertEquals(0, mImeCommandNestLevel.get());
        verifyNoMoreInteractions(mConnection, mDelegate);
    }

    // Dispatch the key code and check that the even was forwarded to the delegate without
    // committing the suggestion.
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
    public void dispatchKeyEvent_processAutocompleteKeysWhenAutocompletionIsAvailable() {
        mCurrentState.setAutocompleteText(Optional.of("google.com"));

        confirmAutocompletionAppliedWithKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        // Enter is forwarded to the delegate for handling which is what "bypassed" checks.
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_ENTER);
        confirmAutocompletionApplied(KeyEvent.KEYCODE_TAB);
        confirmAutocompletionAppliedWithKey(KeyEvent.KEYCODE_DPAD_LEFT);
    }

    @Test
    public void dispatchKeyEvent_passAutocompleteKeysWhenAutocompletionIsNotAvailable() {
        mCurrentState.setAutocompleteText(Optional.empty());

        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_RIGHT);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_ENTER);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_TAB);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_LEFT);
    }

    @Test
    public void dispatchKeyEvent_handleForwardDel() {
        mCurrentState.setUserText("goo");
        mCurrentState.setAutocompleteText(Optional.of("gle.com"));
        assertEquals(mCurrentState.getText(), "google.com"); // Verify full state constructed.

        // The delete key doesn't get sent to our delegate when in autocomplete mode so
        // confirmAutocompletionBypassed() doesn't work. Manually dispatch.
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_FORWARD_DEL);
        clearInvocations(mConnection, mDelegate);
        mModel.dispatchKeyEvent(event);

        // Inline autocompleted text should be deleted.
        assertEquals(mCurrentState.getText(), "goo");

        // Go left and then forward delete the last user-char. The forward delete should still
        // get dispatched.
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_LEFT);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_FORWARD_DEL);
    }
}
