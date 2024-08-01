// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tail;

import android.content.Context;
import android.text.Spannable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.widget.TextView;

import org.chromium.chrome.browser.omnibox.R;

/** Container view for omnibox tail suggestions. */
public class TailSuggestionView extends TextView {
    private AlignmentManager mAlignmentManager;
    private int mFullTextWidth;
    private int mQueryTextWidth;

    public TailSuggestionView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setGravity(Gravity.CENTER_VERTICAL);
        setMaxLines(1);
        setTextAppearance(R.style.TextAppearance_TextLarge_Primary);
    }

    public TailSuggestionView(Context context) {
        this(context, null);
    }

    /**
     * Specifies alignment coordinator controlling horizontal alignment of tail suggestions.
     *
     * @param coordinator Manager controlling alignment.
     */
    void setAlignmentManager(AlignmentManager coordinator) {
        mAlignmentManager = coordinator;
        if (mAlignmentManager != null) {
            mAlignmentManager.registerView(this);
        }
    }

    /**
     * Specify query full text. This text is used for measurement purposes and is not displayed
     * anywhere.
     *
     * @param fullText Full query text that will be executed if user selects this suggestion.
     */
    void setFullText(String fullText) {
        mFullTextWidth = (int) getPaint().measureText(fullText, 0, fullText.length());
    }

    /**
     * Specify query tail text to be displayed in this suggestion.
     *
     * @param text Text to display inside the suggestion.
     */
    void setTailText(Spannable text) {
        mQueryTextWidth = (int) getPaint().measureText(text, 0, text.length());
        setText(text);
    }

    @Override
    public void layout(int left, int top, int right, int bottom) {
        if (mAlignmentManager != null) {
            final int pad =
                    mAlignmentManager.requestStartPadding(
                            this, mQueryTextWidth, mFullTextWidth, right - left);
            if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
                right -= pad;
            } else {
                left += pad;
            }
        }

        super.layout(left, top, right, bottom);
    }
}
