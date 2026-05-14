// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
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
