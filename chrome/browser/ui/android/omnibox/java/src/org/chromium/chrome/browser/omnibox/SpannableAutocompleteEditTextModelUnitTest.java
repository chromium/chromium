// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
/**
 * Unit tests for {@link SpannableAutocompleteEditTextModel}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SpannableAutocompleteEditTextModelUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock SpannableAutocompleteEditTextModel.AutocompleteInputConnection mConnection;
    private @Mock AutocompleteEditTextModelBase.Delegate mDelegate;
    private SpannableAutocompleteEditTextModel mModel;

    @Before
    public void setUp() {
        doReturn(new SpannableStringBuilder("text")).when(mDelegate).getText();
        mModel = new SpannableAutocompleteEditTextModel(mDelegate);
        mModel.setInputConnectionForTesting(mConnection);
    }

    @Test
    public void testNonCompositionalText() {
        assertTrue(SpannableAutocompleteEditTextModel.isNonCompositionalText("http://123.com"));
        assertTrue(SpannableAutocompleteEditTextModel.isNonCompositionalText("goo"));
        assertFalse(SpannableAutocompleteEditTextModel.isNonCompositionalText("네이버"));
        assertFalse(SpannableAutocompleteEditTextModel.isNonCompositionalText("네"));
        assertFalse(SpannableAutocompleteEditTextModel.isNonCompositionalText("123네이버"));
    }

    @Test
    public void dispatchKeyEvent_commitAutocompleteOnDpadLtr() {
        mModel.setLayoutDirectionIsLtr(true);

        // No autocomplete on arrow left.
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_LEFT);
        mModel.dispatchKeyEvent(event);
        verify(mConnection, times(0)).commitAutocomplete();
        verify(mDelegate).super_dispatchKeyEvent(event);

        // Autocomplete triggers on DPAD Right key down.
        event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_RIGHT);
        mModel.dispatchKeyEvent(event);
        verify(mConnection).commitAutocomplete();
        verify(mDelegate).super_dispatchKeyEvent(event);
    }

    @Test
    public void dispatchKeyEvent_commitAutocompleteOnDpadRtl() {
        mModel.setLayoutDirectionIsLtr(false);

        // No autocomplete on arrow right.
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_RIGHT);
        mModel.dispatchKeyEvent(event);
        verify(mConnection, times(0)).commitAutocomplete();
        verify(mDelegate).super_dispatchKeyEvent(event);

        // Autocomplete triggers on DPAD Left key down.
        event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_LEFT);
        mModel.dispatchKeyEvent(event);
        verify(mConnection).commitAutocomplete();
        verify(mDelegate).super_dispatchKeyEvent(event);
    }

    @Test
    public void dispatchKeyEvent_commitAutocompleteOnEnter() {
        // Enter should work in RTL text direction.
        mModel.setLayoutDirectionIsLtr(false);
        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
        mModel.dispatchKeyEvent(event);
        verify(mConnection).commitAutocomplete();
        verify(mDelegate).super_dispatchKeyEvent(event);

        clearInvocations(mConnection);

        // Same for the LTR text direction.
        mModel.setLayoutDirectionIsLtr(true);
        event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
        mModel.dispatchKeyEvent(event);
        verify(mConnection).commitAutocomplete();
        verify(mDelegate).super_dispatchKeyEvent(event);
    }
}
