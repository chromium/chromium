// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.ui.base.WindowDelegate;

/** Provider of capabilities required to embed the omnibox suggestion list into the UI. */
public interface OmniboxSuggestionsDropdownEmbedder {
    /** Return the anchor view the suggestion list should be drawn below. */
    @NonNull
    View getAnchorView();

    /**
     * Return the view that the omnibox suggestions should be aligned horizontally to.  The
     * view must be a descendant of {@link #getAnchorView()} or the anchor view itself.
     */
    @NonNull
    View getAlignmentView();

    /** Return the delegate used to interact with the Window. */
    @NonNull
    WindowDelegate getWindowDelegate();

    /** Return whether the suggestions are being rendered in the tablet UI. */
    boolean isTablet();
}
