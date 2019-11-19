// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget.text;

import android.content.Context;
import android.support.v7.widget.AppCompatEditText;
import android.util.AttributeSet;
import android.widget.EditText;

/**
 * This custom {@link EditText} does not allow the user to scroll the text vertically.  This is
 * can be useful when the EditText is put into a layout where it is smaller than it's normal height.
 */
public class VerticallyFixedEditText extends AppCompatEditText {
    public VerticallyFixedEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    private boolean mBringingPointIntoView;

    @Override
    public boolean bringPointIntoView(int offset) {
        try {
            mBringingPointIntoView = true;
            return super.bringPointIntoView(offset);
        } finally {
            mBringingPointIntoView = false;
        }
    }

    @Override
    public void scrollTo(int x, int y) {
        // To prevent vertical scroll on touch events, only allow
        // TextView.bringPointIntoView(...) to change the vertical scroll.
        super.scrollTo(x, mBringingPointIntoView ? y : getScrollY());
    }
}
