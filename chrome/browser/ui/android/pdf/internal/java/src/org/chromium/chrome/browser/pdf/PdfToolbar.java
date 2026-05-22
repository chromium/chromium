// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;

import androidx.appcompat.widget.Toolbar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Toolbar for the PDF viewer. To handle clicks on the navigation icon, set a listener with {@link
 * #setNavigationOnClickListener(OnClickListener)}.
 */
@NullMarked
public class PdfToolbar extends Toolbar {
    public PdfToolbar(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        EditText currentPage = findViewById(R.id.current_page);
        currentPage.setFocusableInTouchMode(true);
    }

    @Override
    public void clearChildFocus(View child) {
        super.clearChildFocus(child);
        if (child.getId() == R.id.current_page) {
            hideKeyboard(child);
        }
    }

    private void hideKeyboard(View view) {
        InputMethodManager imm =
                (InputMethodManager)
                        view.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) {
            imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
        }
    }
}
