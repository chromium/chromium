// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

/** Triggered when the edge-to-edge state is updated. */
public interface EdgeToEdgePadAdjuster {

    /**
     * Override the bottom inset of the adjuster with additional bottom padding.
     *
     * @param defaultInset The additional bottom inset in px to add to original view padding.
     *     Passing in an inset of 0 will reset to the original padding.
     * @param insetWithBrowserControls The additional bottom inset in px, adjusted to account for
     *     browser controls. If browser controls are not visible, either due to not being present or
     *     being scrolled of, this value will match defaultInset.
     */
    void overrideBottomInset(int defaultInset, int insetWithBrowserControls);
}
