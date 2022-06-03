// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.VisibleForTesting;

import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/** ListItem element used with OmniboxSuggestionList. */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public class DropdownItemViewInfo extends MVCListAdapter.ListItem {
    /** Processor managing the item. */
    public final DropdownItemProcessor processor;
    /** Group ID this ViewInfo belongs to. */
    public final int groupId;

    public DropdownItemViewInfo(DropdownItemProcessor processor, PropertyModel model, int groupId) {
        super(processor.getViewTypeId(), model);
        this.processor = processor;
        this.groupId = groupId;
    }

    @Override
    public String toString() {
        return "DropdownItemViewInfo(group=" + groupId + ", type=" + type + ")";
    }
}
