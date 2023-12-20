// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;


import org.chromium.base.lifetime.Destroyable;

/**
 * Control drawing using the Android Edge to Edge Feature. This allows drawing under Android System
 * Bars.
 */
public interface EdgeToEdgeController extends Destroyable, EdgeToEdgeSupplier {
    /**
     * @return the inset in DPs needed for the bottom UI to adjust views to draw below the Bottom
     *     Nav Bar. Returns 0 when Edge To Edge is not enabled or when the controller is drawing the
     *     page ToNormal.
     */
    int getBottomInset();
}
