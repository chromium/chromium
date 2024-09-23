// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

/** Triggered when the edge-to-edge state is updated. */
public interface EdgeToEdgePadAdjuster {

    /**
     * Override the bottom inset of the adjuster with additional bottom padding.
     *
     * @param inset The additional bottom inset in px to add to original view padding. Passing in an
     *     inset of 0 will reset to the original padding.
     */
    void overrideBottomInset(int inset);

    /** Properly tear down the pad adjuster. */
    void destroy();
}
