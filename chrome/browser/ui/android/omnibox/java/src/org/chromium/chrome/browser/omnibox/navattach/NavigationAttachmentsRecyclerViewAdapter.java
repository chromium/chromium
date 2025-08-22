// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** ModelListAdapter for NavigationAttachments component. */
@NullMarked
class NavigationAttachmentsRecyclerViewAdapter extends SimpleRecyclerViewAdapter {
    NavigationAttachmentsRecyclerViewAdapter(ModelList data) {
        super(data);
    }
}
