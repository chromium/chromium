// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import androidx.annotation.Nullable;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Control drawing using the Android Edge to Edge Feature.
 * This allows drawing under Android System Bars.
 */
public interface EdgeToEdgeController extends Destroyable {
    /**
     * Notifies the controller that a different tab is under observation.<br>
     * @param tab The tab that the observer is now observing. This can be {@code null}.
     */
    void onTabSwitched(@Nullable Tab tab);
}
