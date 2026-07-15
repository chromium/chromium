// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

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

    /** Optional explicit accessibility title for the side panel. */
    public final @Nullable String mAccessibilityTitle;

    public SidePanelContent(View view) {
        this(view, null);
    }

    public SidePanelContent(View view, @Nullable String accessibilityTitle) {
        mView = view;
        mAccessibilityTitle = accessibilityTitle;
    }
}
