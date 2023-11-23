// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

/** The content of the tab shown in the TranslateTabLayout. */
public class TranslateTabContent extends FrameLayout {
    private TextView mTextView;
    private ProgressBar mProgressBar;

    /** Constructor for inflating from XML. */
    public TranslateTabContent(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTextView = (TextView) findViewById(R.id.translate_infobar_tab_text);
        mProgressBar = (ProgressBar) findViewById(R.id.translate_infobar_tab_progressbar);
    }

    /**
     * Sets the text color for all the states (normal, selected, focused) to be this color.
     * @param colors The color state list of the title text.
     */
    public void setTextColor(ColorStateList colors) {
        mTextView.setTextColor(colors);
    }

    /**
     * Set the title text for this tab.
     * @param tabTitle The new title string.
     */
    public void setText(CharSequence tabTitle) {
        mTextView.setText(tabTitle);
    }

    /** Hide progress bar and show text. */
    public void hideProgressBar() {
        mProgressBar.setVisibility(View.INVISIBLE);
        mTextView.setVisibility(View.VISIBLE);
    }

    /** Show progress bar and hide text. */
    public void showProgressBar() {
        mTextView.setVisibility(View.INVISIBLE);
        mProgressBar.setVisibility(View.VISIBLE);
    }
}
