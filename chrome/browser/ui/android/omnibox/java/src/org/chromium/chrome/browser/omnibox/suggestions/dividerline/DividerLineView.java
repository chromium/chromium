// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.dividerline;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.omnibox.R;

/** View for divider line. */
public class DividerLineView extends View {
    public DividerLineView(Context context) {
        super(context, null);
        setMinimumHeight(
                context.getResources()
                        .getDimensionPixelOffset(
                                R.dimen.omnibox_suggestion_list_divider_line_padding));
    }
}
