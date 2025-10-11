// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Adapter for recent tabs shown in the attachments popup */
@NullMarked
public class TabAttachmentPopupChoicesRecyclerViewAdapter extends SimpleRecyclerViewAdapter {
    public static final int TAB_ATTACHMENT_ITEM_TYPE = 1;

    public TabAttachmentPopupChoicesRecyclerViewAdapter(ModelList data) {
        super(data);
        registerType(
                TAB_ATTACHMENT_ITEM_TYPE,
                (parent) ->
                        parent.getContext()
                                .getSystemService(android.view.LayoutInflater.class)
                                .inflate(R.layout.tab_attachment_button, parent, false),
                TabAttachmentPopupChoiceViewBinder::bind);
    }
}
