// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import android.content.Context;
import android.text.TextUtils.TruncateAt;
import android.view.Gravity;
import android.widget.TextView;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** View for Group Headers. */
public class HeaderView extends TextView {
    public HeaderView(Context context) {
        super(context);

        setMaxLines(1);
        setEllipsize(TruncateAt.END);
        setTextAppearance(ChromeColors.getTextMediumThickSecondaryStyle(false));
        setGravity(Gravity.CENTER_VERTICAL);
        setTextAlignment(TextView.TEXT_ALIGNMENT_VIEW_START);
        setMinHeight(
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height));
        setPaddingRelative(OmniboxResourceProvider.getHeaderStartPadding(context), 0, 0, 0);
    }
}
