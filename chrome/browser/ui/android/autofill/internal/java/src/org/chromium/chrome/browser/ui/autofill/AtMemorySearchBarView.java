// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.text.Editable;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;

import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.widget.LoadingView;

/** Custom View representing the reusable search bar for AtMemory components. */
@NullMarked
public class AtMemorySearchBarView extends LinearLayout {
    private EditText mSearchEditText;
    private ImageView mSearchIcon;
    private LoadingView mSearchSpinner;
    private View mClearButton;
    private @Nullable Delegate mDelegate;

    interface Delegate {
        void onQuerySubmitted(String query);

        void onQueryTextChanged(String query);

        void onSearchFocus(boolean hasFocus);
    }

    public AtMemorySearchBarView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mSearchEditText = findViewById(R.id.search_query_input);
        mSearchIcon = findViewById(R.id.search_icon);
        mSearchSpinner = findViewById(R.id.search_spinner);
        mClearButton = findViewById(R.id.search_clear_button);

        mClearButton.setOnClickListener(
                v -> {
                    clearSearchText();
                    focusSearchArea();
                });

        mSearchEditText.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable s) {
                        mClearButton.setVisibility(s.length() > 0 ? View.VISIBLE : View.GONE);
                        if (mDelegate != null) {
                            mDelegate.onQueryTextChanged(s.toString());
                        }
                    }
                });

        mSearchEditText.setOnEditorActionListener(
                (v, actionId, event) -> {
                    if (actionId == EditorInfo.IME_ACTION_SEARCH
                            || (event != null
                                    && event.getAction() == KeyEvent.ACTION_DOWN
                                    && event.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
                        hideKeyboardAndClearFocus();
                        if (mDelegate != null) {
                            mDelegate.onQuerySubmitted(v.getText().toString());
                        }
                        return true;
                    }
                    return false;
                });

        mSearchEditText.setOnFocusChangeListener(
                (v, hasFocus) -> {
                    if (mDelegate != null) {
                        mDelegate.onSearchFocus(hasFocus);
                    }
                });
    }

    public void focusSearchArea() {
        // TODO(crbug.com/512802813): Fix cursor not blinking on subsequent openings of the bottom
        // sheet.
        mSearchEditText.requestFocus();
        KeyboardUtils.showKeyboard(mSearchEditText);
    }

    public void clearSearchText() {
        if (mSearchEditText.getText().length() > 0) {
            mSearchEditText.setText("");
        }
    }

    public boolean searchHasFocus() {
        return mSearchEditText.hasFocus();
    }

    public void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    public void hideKeyboardAndClearFocus() {
        KeyboardUtils.hideAndroidSoftKeyboard(mSearchEditText);
        mSearchEditText.clearFocus();
    }

    public void setIsLoading(boolean isLoading) {
        mSearchIcon.setVisibility(isLoading ? View.GONE : View.VISIBLE);
        mSearchSpinner.setVisibility(isLoading ? View.VISIBLE : View.GONE);
    }
}
