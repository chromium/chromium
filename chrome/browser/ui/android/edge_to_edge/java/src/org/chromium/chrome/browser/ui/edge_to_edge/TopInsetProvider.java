// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.layouts.LayoutType;

/**
 * Interface for components that need to observe and query top inset state for edge-to-edge display.
 */
@NullMarked
public interface TopInsetProvider extends Destroyable {

    /** Observer to notify when a change has been made in the top inset. */
    interface Observer {
        /**
         * Notifies that a change has been made in the top inset and supplies the new inset.
         *
         * @param systemTopInset The system's top inset. This represents the height of the status
         *     bar, regardless of whether the page is drawing edge-to-edge.
         * @param consumeTopInset Whether the system's top inset will be removed.
         * @param layoutType The current active layout type from {@link LayoutType}.
         */
        void onToEdgeChange(
                int systemTopInset, boolean consumeTopInset, @LayoutType int layoutType);
    }

    /** Adds an observer. */
    void addObserver(Observer observer);

    /** Removes an observer. */
    void removeObserver(Observer observer);
}
