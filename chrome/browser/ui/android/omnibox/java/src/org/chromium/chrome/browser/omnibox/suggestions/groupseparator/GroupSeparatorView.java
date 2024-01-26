// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.groupseparator;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.omnibox.R;

/** View spacing out adjacent, headerless vertical suggestion groups. */
public class GroupSeparatorView extends View {
    public GroupSeparatorView(Context context) {
        super(context, null);
        setMinimumHeight(
                context.getResources()
                        .getDimensionPixelOffset(
                                R.dimen.omnibox_suggestion_list_divider_line_padding));
    }
}
