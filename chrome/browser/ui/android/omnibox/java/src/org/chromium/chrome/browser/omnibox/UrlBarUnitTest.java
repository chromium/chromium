// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.text.InputType;
import android.text.SpannableStringBuilder;
import android.view.ViewStructure;
import android.view.inputmethod.EditorInfo;

import androidx.core.view.inputmethod.EditorInfoCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;

/**
 * Unit tests for the URL bar UI component.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class UrlBarUnitTest {
    private UrlBar mUrlBar;
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock UrlBarDelegate mUrlBarDelegate;
    private @Mock ViewStructure mViewStructure;

    @Before
    public void setUp() {
        mUrlBar = new UrlBarApi26(ContextUtils.getApplicationContext(), null);
        mUrlBar.setDelegate(mUrlBarDelegate);
    }

    @Test
    public void testAutofillStructureReceivesFullURL() {
        mUrlBar.setTextForAutofillServices("https://www.google.com");
        mUrlBar.setText("www.google.com");
        mUrlBar.onProvideAutofillStructure(mViewStructure, 0);

        ArgumentCaptor<SpannableStringBuilder> haveUrl =
                ArgumentCaptor.forClass(SpannableStringBuilder.class);
        verify(mViewStructure).setText(haveUrl.capture());
        assertEquals("https://www.google.com", haveUrl.getValue().toString());
    }

    @Test
    public void onCreateInputConnection_ensureNoAutocorrect() {
        var info = new EditorInfo();
        mUrlBar.onCreateInputConnection(info);
        assertEquals(EditorInfo.TYPE_TEXT_VARIATION_URI,
                info.inputType & EditorInfo.TYPE_TEXT_VARIATION_URI);
        assertEquals(0, info.inputType & EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT);
    }

    @Test
    public void onCreateInputConnection_disallowKeyboardLearningPassedToIme() {
        doReturn(true).when(mUrlBarDelegate).allowKeyboardLearning();

        var info = new EditorInfo();
        mUrlBar.onCreateInputConnection(info);
        assertEquals(0, info.imeOptions & EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING);
    }

    @Test
    public void onCreateInputConnection_allowKeyboardLearningPassedToIme() {
        doReturn(false).when(mUrlBarDelegate).allowKeyboardLearning();

        var info = new EditorInfo();
        mUrlBar.onCreateInputConnection(info);
        assertEquals(EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING,
                info.imeOptions & EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING);
    }

    @Test
    public void onCreateInputConnection_setDefaultsWhenDelegateNotPresent() {
        mUrlBar.setDelegate(null);

        var info = new EditorInfo();
        mUrlBar.onCreateInputConnection(info);

        assertEquals(EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING,
                info.imeOptions & EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING);
        assertEquals(EditorInfo.TYPE_TEXT_VARIATION_URI,
                info.inputType & EditorInfo.TYPE_TEXT_VARIATION_URI);
        assertEquals(0, info.inputType & EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT);
    }

    @Test
    public void urlBar_editorSetupPermitsWordSelection() {
        // This test verifies whether the Omnibox is set up so that it permits word selection. See:
        // https://cs.android.com/search?q=function:Editor.needsToSelectAllToSelectWordOrParagraph

        int klass = mUrlBar.getInputType() & InputType.TYPE_MASK_CLASS;
        int variation = mUrlBar.getInputType() & InputType.TYPE_MASK_VARIATION;
        int flags = mUrlBar.getInputType() & InputType.TYPE_MASK_FLAGS;
        assertEquals(InputType.TYPE_CLASS_TEXT, klass);
        assertEquals(InputType.TYPE_TEXT_VARIATION_NORMAL, variation);
        assertEquals(InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS, flags);
    }
}
