// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.appcompat.widget.Toolbar;

import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.NullMarked;

/**
 * Toolbar for the PDF viewer. To handle clicks on the navigation icon, set a listener with
 * {@link #setNavigationOnClickListener(OnClickListener)}.
 */
@NullMarked
public class PdfToolbar extends Toolbar {
    private TextView mCurrentPage;
    private TextView mPageCount;

    public PdfToolbar(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mCurrentPage = findViewById(R.id.current_page);
        mPageCount = findViewById(R.id.page_count);
    }

    void setPageNumber(String pageNumber) {
        mCurrentPage.setText(pageNumber);
    }

    void setPageCount(String pageCount) {
        mPageCount.setText(pageCount);
    }
}
