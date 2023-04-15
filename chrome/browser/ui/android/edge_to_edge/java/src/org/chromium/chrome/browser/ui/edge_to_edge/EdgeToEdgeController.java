// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

/**
 * Control drawing using the Android Edge to Edge Feature.
 * This allows drawing under Android System Bars.
 */
public interface EdgeToEdgeController {
    /**
     * Enables drawing underneath one or more of the Android System Bars, e.g. the Navigation Bar.
     */
    void drawUnderSystemBars();
}
