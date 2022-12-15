// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import android.content.Context;
import android.text.TextUtils.TruncateAt;
import android.view.Gravity;
import android.widget.TextView;

import androidx.core.widget.TextViewCompat;

import org.chromium.components.browser_ui.styles.ChromeColors;

/**
 * View for Group Headers.
 */
public class HeaderView extends TextView {
    /**
     * Constructs a new header view.
     *
     * @param context Current context.
     */
    public HeaderView(Context context) {
        super(context);

        setMaxLines(1);
        setEllipsize(TruncateAt.END);
        TextViewCompat.setTextAppearance(
                this, ChromeColors.getTextMediumThickSecondaryStyle(false));
        setGravity(Gravity.CENTER_VERTICAL);
        setTextAlignment(TextView.TEXT_ALIGNMENT_VIEW_START);
    }

    /**
     * Specifies the paddings for suggestion header.
     *
     * @param minHeight the min height of header view.
     * @param paddingStart the start padding of header view.
     * @param paddingTop the top padding of header view.
     * @param paddingBottom the bottom padding of header view.
     */
    void setUpdateHeaderPadding(
            int minHeight, int paddingStart, int paddingTop, int paddingBottom) {
        setMinHeight(minHeight);
        setPaddingRelative(paddingStart, paddingTop, 0, paddingBottom);
    }
}
