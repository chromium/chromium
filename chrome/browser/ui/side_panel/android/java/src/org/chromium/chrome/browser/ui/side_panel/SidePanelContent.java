// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Content inside a side panel container. */
@NullMarked
public final class SidePanelContent {

    /**
     * The feature-specific View.
     *
     * <p>This View doesn't include any common UI provided by the container.
     */
    public final View mView;

    /** The title of the side panel content, to be shown in the header. */
    public final @Nullable String mTitle;

    /** Whether to render a header (with title and close button). */
    public final boolean mShowHeader;

    public SidePanelContent(View view) {
        this(view, /* title= */ null, /* showHeader= */ false);
    }

    public SidePanelContent(View view, @Nullable String title, boolean showHeader) {
        mView = view;
        mTitle = title;
        mShowHeader = showHeader;
    }
}
