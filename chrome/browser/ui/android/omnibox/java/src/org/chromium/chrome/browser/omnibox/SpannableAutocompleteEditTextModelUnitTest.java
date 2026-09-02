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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.text.Selection;
import android.text.SpannableStringBuilder;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.OmniboxWordBoundary;
import org.chromium.components.omnibox.OmniboxWordBoundaryJni;
import org.chromium.components.omnibox.TextSelection;

import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link SpannableAutocompleteEditTextModel}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SpannableAutocompleteEditTextModelUnitTest {
    @Rule public final MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock private AutocompleteInputConnection mConnection;
    @Mock private AutocompleteEditTextModelBase.Delegate mDelegate;
    @Mock private OmniboxWordBoundary.Natives mWordBoundaryNatives;
    private SpannableAutocompleteEditTextModel mModel;
    private AutocompleteState mCurrentState;
    private AtomicInteger mImeCommandNestLevel;
    @Captor private ArgumentCaptor<KeyEvent> mKeyEventCaptor;

    @Before
    public void setUp() {
        OmniboxWordBoundaryJni.setInstanceForTesting(mWordBoundaryNatives);
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
        verify(mDelegate, never()).super_dispatchKeyEvent(event);
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
        verify(mConnection, never()).commitAutocomplete();
        verify(mDelegate).super_dispatchKeyEvent(event);

        // Secondary, not directly linked to the test.
        verify(mConnection, atLeastOnce()).onBeginImeCommand();
        verify(mConnection, atLeastOnce()).onEndImeCommand();
        assertEquals(0, mImeCommandNestLevel.get());
        verifyNoMoreInteractions(mConnection, mDelegate);
    }

    @Test
    public void dispatchKeyEvent_processAutocompleteKeysWhenAutocompletionIsAvailable() {
        mCurrentState.setAutocompleteText("google.com");

        confirmAutocompletionAppliedWithKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        // Enter is forwarded to the delegate for handling which is what "bypassed" checks.
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_ENTER);
        confirmAutocompletionApplied(KeyEvent.KEYCODE_TAB);
        confirmAutocompletionAppliedWithKey(KeyEvent.KEYCODE_DPAD_LEFT);
    }

    @Test
    public void dispatchKeyEvent_passAutocompleteKeysWhenAutocompletionIsNotAvailable() {
        mCurrentState.setAutocompleteText(null);

        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_RIGHT);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_ENTER);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_TAB);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_LEFT);
    }

    @Test
    public void dispatchKeyEvent_ctrlTabBypassed() {
        mCurrentState.setAutocompleteText("google.com");
        var event =
                new KeyEvent(
                        0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB, 0, KeyEvent.META_CTRL_ON);

        clearInvocations(mConnection, mDelegate);
        mModel.dispatchKeyEvent(event);
        verify(mConnection, never()).commitAutocomplete();
        verify(mDelegate).super_dispatchKeyEvent(event);
    }

    @Test
    public void dispatchKeyEvent_handleForwardDel() {
        mCurrentState.setUserText("goo");
        mCurrentState.setAutocompleteText("gle.com");
        assertEquals("google.com", mCurrentState.getText()); // Verify full state constructed.

        // The delete key doesn't get sent to our delegate when in autocomplete mode so
        // confirmAutocompletionBypassed() doesn't work. Manually dispatch.
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_FORWARD_DEL);
        clearInvocations(mConnection, mDelegate);
        mModel.dispatchKeyEvent(event);

        // Inline autocompleted text should be deleted.
        assertEquals("goo", mCurrentState.getText());

        // Go left and then forward delete the last user-char. The forward delete should still
        // get dispatched.
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_DPAD_LEFT);
        confirmAutocompletionBypassed(KeyEvent.KEYCODE_FORWARD_DEL);
    }

    @Test
    public void dispatchKeyEvent_handleDel() {
        mCurrentState.setUserText("goo");
        mCurrentState.setAutocompleteText("gle.com");
        assertEquals("google.com", mCurrentState.getText());

        clearInvocations(mConnection, mDelegate);
        mModel.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        assertEquals("goo", mCurrentState.getText());
    }

    @Test
    public void dispatchKeyEvent_handleAltDel() {
        mCurrentState.setUserText("goo");
        mCurrentState.setAutocompleteText(null);
        assertEquals("goo", mCurrentState.getText());

        clearInvocations(mConnection, mDelegate);
        var event =
                new KeyEvent(
                        0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0, KeyEvent.META_ALT_ON);
        mModel.dispatchKeyEvent(event);

        // Should delegate a FORWARD_DEL event.
        verify(mDelegate).super_dispatchKeyEvent(mKeyEventCaptor.capture());
        assertEquals(KeyEvent.KEYCODE_FORWARD_DEL, mKeyEventCaptor.getValue().getKeyCode());
        assertEquals(0, mKeyEventCaptor.getValue().getMetaState());
    }

    @Test
    public void testSpanCursorController_setSpan_clampsSelection() {
        SpannableStringBuilder editable = new SpannableStringBuilder("userText");
        doReturn(editable).when(mDelegate).getEditableText();

        SpanCursorController controller = mModel.getSpanCursorController();

        AutocompleteState state =
                new AutocompleteState(
                        "userText",
                        "auto",
                        null,
                        new TextSelection(Integer.MAX_VALUE, Integer.MAX_VALUE),
                        null);

        controller.setSpan(state);

        assertEquals(12, Selection.getSelectionStart(editable));
        assertEquals(12, Selection.getSelectionEnd(editable));
    }

    @Test
    public void dispatchKeyEvent_deleteWordBackward() {
        mCurrentState.setAutocompleteText(null);
        mCurrentState.setUserText("google.com");
        mCurrentState.setSelection(new TextSelection(10, 10));

        doReturn(7).when(mWordBoundaryNatives).getDeletionBoundary("google.com", 10, false);

        var event =
                new KeyEvent(
                        0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0, KeyEvent.META_CTRL_ON);

        clearInvocations(mConnection, mDelegate);
        boolean handled = mModel.dispatchKeyEvent(event);

        assertTrue(handled);
        verify(mConnection).deleteSurroundingText(3, 0);
    }

    @Test
    public void dispatchKeyEvent_deleteWordForward() {
        mCurrentState.setAutocompleteText(null);
        mCurrentState.setUserText("google::com_");
        mCurrentState.setSelection(new TextSelection(6, 6));

        doReturn(12).when(mWordBoundaryNatives).getDeletionBoundary("google::com_", 6, true);

        var event =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_FORWARD_DEL,
                        0,
                        KeyEvent.META_CTRL_ON);

        clearInvocations(mConnection, mDelegate);
        boolean handled = mModel.dispatchKeyEvent(event);

        assertTrue(handled);
        verify(mConnection).deleteSurroundingText(0, 6);
    }
}
