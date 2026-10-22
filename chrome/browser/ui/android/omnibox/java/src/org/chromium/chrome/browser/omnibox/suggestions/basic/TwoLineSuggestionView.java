// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;

/** View for basic omnibox suggestions containing two lines of text. */
@NullMarked
public class TwoLineSuggestionView extends LinearLayout {
    private TextView mLine1;
    private TextView mLine2;

    public TwoLineSuggestionView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public TwoLineSuggestionView(Context context) {
        this(context, null);
    }

    @Initializer
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mLine1 = findViewById(R.id.line_1);
        mLine2 = findViewById(R.id.line_2);
    }

    public TextView getLine1() {
        return mLine1;
    }

    public TextView getLine2() {
        return mLine2;
    }
}
