// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import android.graphics.drawable.Drawable;
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

    /** Whether to render a header (with title and close button). */
    public final boolean mShowHeader;

    /** The optional representative icon shown at the start of the header. */
    public final @Nullable Drawable mIcon;

    /** The title of the side panel content, to be shown in the header. */
    public final @Nullable String mTitle;

    public SidePanelContent(
            View view, boolean showHeader, @Nullable Drawable icon, @Nullable String title) {
        mView = view;
        mShowHeader = showHeader;
        mIcon = icon;
        mTitle = title;
    }
}
