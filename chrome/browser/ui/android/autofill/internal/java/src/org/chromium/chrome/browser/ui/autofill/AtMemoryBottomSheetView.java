// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;

/** View wrapper for the @memory bottom sheet. */
@NullMarked
public class AtMemoryBottomSheetView {
    private final View mContentView;

    public AtMemoryBottomSheetView(Context context) {
        mContentView = LayoutInflater.from(context).inflate(R.layout.at_memory_bottom_sheet, null);
        initializeSearchQueryInput();
    }

    private void initializeSearchQueryInput() {
        EditText searchInput = mContentView.findViewById(R.id.search_query_input);
        View clearButton = mContentView.findViewById(R.id.clear_search_button);

        clearButton.setOnClickListener(v -> clearSearchText());
        searchInput.addTextChangedListener(
                new TextWatcher() {
                    @Override
                    public void beforeTextChanged(
                            CharSequence s, int start, int count, int after) {}

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        clearButton.setVisibility(s.length() > 0 ? View.VISIBLE : View.GONE);
                    }

                    @Override
                    public void afterTextChanged(Editable s) {}
                });
    }

    public View getContentView() {
        return mContentView;
    }

    public void focusSearchArea() {
        View searchInput = mContentView.findViewById(R.id.search_query_input);
        assert searchInput != null;
        // TODO(crbug.com/512802813): Fix cursor not blinking on subsequent openings of the bottom
        // sheet.
        searchInput.requestFocus();
        KeyboardUtils.showKeyboard(searchInput);
    }

    public void clearSearchText() {
        View searchInput = mContentView.findViewById(R.id.search_query_input);
        assert searchInput instanceof EditText;
        ((EditText) searchInput).setText("");
    }
}
