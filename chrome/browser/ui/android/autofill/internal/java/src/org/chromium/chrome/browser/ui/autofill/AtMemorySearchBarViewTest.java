// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.widget.LoadingView;

/** Unit tests for {@link AtMemorySearchBarView}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class AtMemorySearchBarViewTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AtMemorySearchBarView.Delegate mMockDelegate;

    private Context mContext;
    private AtMemorySearchBarView mView;
    private EditText mSearchEditText;
    private ImageView mSearchIcon;
    private LoadingView mSearchSpinner;
    private View mClearButton;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mView =
                (AtMemorySearchBarView)
                        LayoutInflater.from(mContext).inflate(R.layout.at_memory_search_bar, null);
        mSearchEditText = mView.findViewById(R.id.search_query_input);
        mSearchIcon = mView.findViewById(R.id.search_icon);
        mSearchSpinner = mView.findViewById(R.id.search_spinner);
        mClearButton = mView.findViewById(R.id.search_clear_button);
    }

    @Test
    public void testClearSearchText() {
        mSearchEditText.setText("some text");
        assertEquals("some text", mSearchEditText.getText().toString());

        mView.clearSearchText();
        assertEquals("", mSearchEditText.getText().toString());
    }

    @Test
    public void testClearSearchText_emptyTextIsNoOp() {
        mView.setDelegate(mMockDelegate);
        mView.clearSearchText();
        Mockito.verifyNoInteractions(mMockDelegate);
    }

    @Test
    public void testClearButtonVisibilityOnTextChange() {
        assertEquals(View.GONE, mClearButton.getVisibility());

        mSearchEditText.setText("hello");
        mSearchEditText.clearFocus();
        assertFalse(mSearchEditText.hasFocus());
        assertEquals(View.VISIBLE, mClearButton.getVisibility());

        mClearButton.performClick();
        assertEquals("", mSearchEditText.getText().toString());
        assertEquals(View.GONE, mClearButton.getVisibility());
        assertTrue(mSearchEditText.hasFocus());
    }

    @Test
    public void testSetIsLoading() {
        mView.setIsLoading(true);
        assertEquals(View.GONE, mSearchIcon.getVisibility());
        assertEquals(View.VISIBLE, mSearchSpinner.getVisibility());

        mView.setIsLoading(false);
        assertEquals(View.VISIBLE, mSearchIcon.getVisibility());
        assertEquals(View.GONE, mSearchSpinner.getVisibility());
    }

    @Test
    public void testOnQuerySubmittedCallback() {
        mView.setDelegate(mMockDelegate);
        mSearchEditText.setText("test query");

        mSearchEditText.onEditorAction(EditorInfo.IME_ACTION_SEARCH);
        verify(mMockDelegate).onQuerySubmitted("test query");
    }

    @Test
    public void testOnQueryTextChangedCallback() {
        mView.setDelegate(mMockDelegate);
        mSearchEditText.setText("abc");
        verify(mMockDelegate).onQueryTextChanged("abc");
    }
}
