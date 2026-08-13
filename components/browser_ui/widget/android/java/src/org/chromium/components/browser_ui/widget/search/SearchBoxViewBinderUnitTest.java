// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
import android.widget.EditText;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link SearchBoxViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchBoxViewBinderUnitTest {
    private PropertyModel mModel;
    private SearchBoxView mView;
    private EditText mSearchText;
    private View mClearButton;
    private View mPlaceholder;

    private String mSearchTextChangedValue;
    private boolean mFocusChangedValue;
    private boolean mClearButtonClicked;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView = new SearchBoxView(activity, null);

        // Placeholder view solely used to forcefully tear focus away from EditText in Robolectric
        mPlaceholder = new View(activity);
        mPlaceholder.setFocusable(true);
        mPlaceholder.setFocusableInTouchMode(true);
        mView.addView(mPlaceholder);

        mSearchText = mView.findViewById(R.id.search_text);
        mClearButton = mView.findViewById(R.id.clear_text_button);

        mModel =
                new PropertyModel.Builder(SearchBoxProperties.ALL_KEYS)
                        .with(
                                SearchBoxProperties.TEXT_CHANGED_CALLBACK,
                                text -> mSearchTextChangedValue = text)
                        .with(
                                SearchBoxProperties.FOCUS_CHANGED_CALLBACK,
                                focus -> mFocusChangedValue = focus)
                        .with(
                                SearchBoxProperties.CLEAR_SEARCH_TEXT_RUNNABLE,
                                () -> mClearButtonClicked = true)
                        .build();

        PropertyModelChangeProcessor.create(mModel, mView, SearchBoxViewBinder::bind);
    }

    @Test
    public void testSearchText() {
        mModel.set(SearchBoxProperties.SEARCH_TEXT, "test query");
        assertEquals("test query", mSearchText.getText().toString());
    }

    @Test
    public void testHintText() {
        mModel.set(SearchBoxProperties.HINT_TEXT, "search hint");
        assertEquals("search hint", mSearchText.getHint());
    }

    @Test
    public void testClearButtonVisibility() {
        mModel.set(SearchBoxProperties.CLEAR_BUTTON_VISIBILITY, true);
        assertEquals(View.VISIBLE, mClearButton.getVisibility());

        mModel.set(SearchBoxProperties.CLEAR_BUTTON_VISIBILITY, false);
        assertEquals(View.GONE, mClearButton.getVisibility());
    }

    @Test
    public void testTextChangedCallback() {
        mSearchTextChangedValue = null;
        mSearchText.setText("new query");
        assertEquals("new query", mSearchTextChangedValue);
    }

    @Test
    public void testClearSearchTextRunnable() {
        mClearButtonClicked = false;
        mClearButton.performClick();
        assertTrue(mClearButtonClicked);
    }

    @Test
    public void testFocusChangedCallback() {
        mFocusChangedValue = false;
        mSearchText.requestFocus();
        assertTrue(mFocusChangedValue);

        mPlaceholder.requestFocus();
        assertFalse(mFocusChangedValue);
    }

    @Test
    public void testHasFocus() {
        mModel.set(SearchBoxProperties.HAS_FOCUS, true);
        assertTrue(mSearchText.hasFocus());

        // Focus goes to placeholder view so that clearFocus doesn't just
        // re-focus the EditText.
        mModel.set(SearchBoxProperties.HAS_FOCUS, false);
        mPlaceholder.requestFocus();
        assertFalse(mSearchText.hasFocus());
    }
}
