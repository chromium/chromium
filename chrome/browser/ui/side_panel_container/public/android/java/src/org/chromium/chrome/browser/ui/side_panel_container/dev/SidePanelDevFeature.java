// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container.dev;

import org.chromium.build.annotations.NullMarked;

/**
 * A side panel feature for developing side panel container, UI coordination logic, etc.
 *
 * <p>This is useful for testing any UI in the side panel container without integrating a production
 * feature.
 */
@NullMarked
public interface SidePanelDevFeature {

    /**
     * Populates/Removes this {@link SidePanelDevFeature}'s content into/from the side panel
     * container.
     */
    void toggle();

    /** Destroys this {@link SidePanelDevFeature}. */
    void destroy();
}
